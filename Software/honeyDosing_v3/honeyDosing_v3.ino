/**
 * BeeSMART Honey Dosing System v3.3.0
 * ESP32-based precision honey dosing with PID control and web interface.
 *
 * Architecture:
 *  - honeyDosing_v3.ino: entry point (setup + loop)
 *  - src/globals.h/.cpp: global variables and language strings
 *  - src/filesystem.h/.cpp: file I/O and settings persistence
 *  - src/statistics.h/.cpp: dispensing statistics and weight sampling
 *  - src/control.h/.cpp: PID control, dosing and calibration state machines
 *  - src/webserver.h/.cpp: HTTP API, WebSocket, captive portal
 *  - config.h: constants, pin definitions, structs
 */

#include "src/config.h"
#include "src/globals.h"
#include "src/filesystem.h"
#include "src/statistics.h"
#include "src/control.h"
#include "src/webserver.h"

void setup(void) {
    Serial.begin(115200);
    Serial.println("\n" + String('\x3D', 80));
    Serial.println("BeeSMART Honey Dosing System v3.3.0 - Initializing...");
    Serial.println(String('\x3D', 80));

    if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
        Serial.println("CRITICAL: LittleFS Mount Failed");
        ESP.restart();
    }
    Serial.println("LittleFS file system mounted successfully");

    readCal(LittleFS, "/cal.txt");
    readFile(LittleFS, "/data.txt");
    initializeStats();
    initializeWeightSampling();

    Serial.println("Initializing hardware components...");
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scale.set_scale(calFactor);
    Serial.println("Scale interface initialized with calibration factor: " + String(calFactor));

    myservo.attach(SERVO_PIN);
    Serial.println("Servo motor attached to pin " + String(SERVO_PIN));

    initPID(0.1, 1);

    WiFi.mode(WIFI_AP);
    delay(100);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

    uint32_t chipid = 0;
    for (int i = 0; i < 17; i = i + 8) {
        chipid |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    char ap_ssid[25];
    snprintf(ap_ssid, 26, "BeeSMART-%08X", chipid);
    WiFi.softAP(ap_ssid);

    Serial.println("WiFi AP created: " + String(ap_ssid));
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    dnsServer.start(DNS_PORT, "*", apIP);
    Serial.println("DNS server started for captive portal");

    initWebServer();

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.println("WebSocket server started on port 81");

    myController.stop();
    scale.tare();
    Serial.println("Scale tared and system reset to idle state");

    Serial.println(String('\x3D', 80));
    Serial.println("BeeSMART Honey Dosing System v3.3.0 Ready!");
    Serial.println("Connect to WiFi: " + String(ap_ssid));
    Serial.println("Open browser - captive portal will redirect automatically");
    Serial.println(String('\x3D', 80) + "\n");
}

void loop(void) {
    static unsigned long lastLoopTime = 0;
    static uint8_t fullStatusCounter = 0;
    unsigned long now = millis();
    if (now - lastLoopTime >= looptime) {
        lastLoopTime = now;

        dnsServer.processNextRequest();
        server.handleClient();
        webSocket.loop();

        fullStatusCounter++;
        if (fullStatusCounter >= 50) {
            fullStatusCounter = 0;
            { String status = buildStatusJson(); webSocket.broadcastTXT(status); }
        } else if ((fullStatusCounter & 1) == 0) {
            { String weight = buildWeightUpdateJson(); webSocket.broadcastTXT(weight); }
        }

        checkAutoSave();

        actualWeight = getStableWeight();
        int16_t temp = (actualWeight - glasWeight);
        adjustedWeight = (temp < 0) ? 0 : (uint16_t)temp;

        runCalibrationStateMachine();

        runDosingStateMachine();

        if (setpoint > 0) {
            input = (float)adjustedWeight / setpoint;
        } else {
            input = 0;
        }
        if (stateMachine == 3) {
            myController.compute();
        }

        if (servoTestMode) {
            if (millis() - servoTestStartTime > 3000) {
                servoTestMode = false;
                servoTestOutput = 0.0;
            } else {
                output = servoTestOutput;
            }
        }

        myservo.write(servoMin + output * (servoMax - servoMin));
    }
}
