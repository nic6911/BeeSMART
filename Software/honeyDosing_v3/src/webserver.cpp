#include "globals.h"
#include "webserver.h"
#include "statistics.h"
#include "control.h"
#include "filesystem.h"

String buildStatusMessage() {
    switch (stateMachine) {
        case 1: return step2Text[lang];
        case 2:
        case 3: return step3Text[lang];
        case 4: return (actualWeight < minGlassWeight && autoState == 1) ? step1Text[lang]
                      : (actualWeight < minGlassWeight) ? step1Text[lang] : step4Text[lang];
        default: return step1Text[lang];
    }
}

String buildCalibrationJson() {
    String calMsg = "";
    switch (calStateMachine) {
        case 0:
            calMsg = LittleFS.exists("/cal.txt") ? calStep1Text[lang] : "System skal kalibreres, tryk Kalibrer for at starte";
            break;
        case 1: calMsg = calStep2Text[lang]; break;
        case 2:
        case 4: calMsg = calStep3Text[lang]; break;
        case 3: calMsg = calStep4Text[lang]; break;
    }

    String json = "\"message\":\"" + calMsg + "\",\"state\":" + String(calStateMachine);
    if (calAveraging && calStateMachine == 4) {
        int samplingProgress = (calCount * 90) / calSamples;
        int totalProgress = 10 + samplingProgress;
        json += ",\"progress\":{\"active\":true,\"percent\":" + String(totalProgress) +
                ",\"current\":" + String(calCount + 10) + ",\"total\":" + String(calSamples + 10) + "}";
    } else if (calStateMachine == 2 && tareInProgress) {
        json += ",\"progress\":{\"active\":true,\"percent\":5,\"current\":5,\"total\":110}";
    } else if (calStateMachine == 3) {
        json += ",\"progress\":{\"active\":true,\"percent\":10,\"current\":10,\"total\":110}";
    } else {
        json += ",\"progress\":{\"active\":false}";
    }
    return json;
}

String buildWeightUpdateJson() {
    String json = "{\"running\":" + String(stateMachine != 4 ? "true" : "false") +
                  ",\"stateMachine\":" + String(stateMachine) +
                  ",\"autoState\":" + String(autoState ? "true" : "false") +
                  ",\"message\":\"" + buildStatusMessage() +
                  "\",\"weights\":{\"total\":" + String(actualWeight) +
                  ",\"honey\":" + String(adjustedWeight) +
                  ",\"glass\":" + String(glasWeight) + "}" +
                  ",\"calibration\":{" + buildCalibrationJson() + "}";

    if (hasSaveMessage) {
        json += ",\"saveStatus\":{\"success\":true,\"message\":\"" + lastSaveMessage + "\"}";
        hasSaveMessage = false;
        lastSaveMessage = "";
    }

    json += "}";
    return json;
}

String buildStatusJson() {
    String json = buildWeightUpdateJson();

    json = json.substring(0, json.length() - 1);

    float avgError, totalDispensed;
    calculateBasicStats(&avgError, &totalDispensed);

    json += ",\"statistics\":{";
    json += "\"totalCycles\":" + String(totalDispensingCycles);
    json += ",\"totalDispensed\":" + String(totalDispensed, 2);
    json += ",\"averageError\":" + String(avgError, 1);
    if (totalDispensingCycles > 0) {
        float cumulativeAvgErr = (float)cumulativeTotalError / totalDispensingCycles;
        json += ",\"cumulativeDispensed\":" + String(cumulativeDispensedGrams);
        json += ",\"cumulativeAverageError\":" + String(cumulativeAvgErr, 1);
    } else {
        json += ",\"cumulativeDispensed\":0,\"cumulativeAverageError\":0";
    }

    json += ",\"recentHistory\":[";
    int start = (historyIndex - historyCount + 10) % 10;
    for (int i = 0; i < historyCount; i++) {
        if (i > 0) json += ",";
        DispensingRecord record = dispensingHistory[(start + i) % 10];
        json += "{\"target\":" + String(record.targetWeight, 1);
        json += ",\"actual\":" + String(record.actualWeight, 1);
        json += ",\"error\":" + String(record.error, 1) + "}";
    }
    json += "]}}";

    return json;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.println("WebSocket client " + String(num) + " disconnected");
            break;
        case WStype_CONNECTED:
            Serial.println("WebSocket client " + String(num) + " connected");
            { String status = buildStatusJson(); webSocket.sendTXT(num, status); }
            break;
        default:
            break;
    }
}

void setCORSHeaders() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleCORS() {
    setCORSHeaders();
    server.send(200, "text/plain", "");
}

void handleApiSettings() {
    String json = "{\"desiredAmount\":" + desiredAmount +
                  ",\"kp\":" + String(kP[gainSelector]) +
                  ",\"ti\":" + String(Ti[gainSelector]) +
                  ",\"kd\":" + String(kD[gainSelector]) +
                  ",\"servoMin\":" + String(servoMin) +
                  ",\"servoMax\":" + String(servoMax) +
                  ",\"stopHysteresis\":" + String(stopHysteresis) +
                  ",\"minGlassWeight\":" + String(minGlassWeight) +
                  ",\"maxWeight\":" + String(maxWeight) +
                  ",\"viscosity\":" + String(gainSelector) +
                  ",\"calWeight\":" + calWeight +
                  ",\"autoState\":" + String(autoState ? "true" : "false") +
                  ",\"language\":" + String(lang) + "}";

    server.send(200, "application/json", json);
}

void handleApiCommand() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }

    String body = server.arg("plain");
    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }

    String command = doc["command"];
    JsonObject payload = doc["payload"];

    if (command == "start") {
        if (stateMachine == 4 && calStateMachine == 0) {
            stateMachine = 1;
        }
    } else if (command == "stop") {
        stopSystem();
    } else if (command == "tare") {
        scale.tare();
    } else if (command == "setAuto") {
        autoState = payload["value"];
        markSettingsChanged();
    } else if (command == "setAmount") {
        desiredAmount = String(constrain((int)payload["value"], (int)minWeight, (int)maxWeight));
        markSettingsChanged();
    } else if (command == "setServoMin") {
        int val = payload["value"];
        if (val >= 0 && val <= 180) {
            servoMin = val;
            markSettingsChanged();
        }
    } else if (command == "setServoMax") {
        int val = payload["value"];
        if (val >= 0 && val <= 180) {
            servoMax = val;
            markSettingsChanged();
        }
    } else if (command == "setStopHysteresis") {
        int val = payload["value"];
        if (val >= 0 && val <= 500) {
            stopHysteresis = val;
            markSettingsChanged();
        }
    } else if (command == "setMinGlassWeight") {
        int val = payload["value"];
        if (val >= 0 && val <= 500) {
            minGlassWeight = val;
            markSettingsChanged();
        }
    } else if (command == "setMaxWeight") {
        int val = payload["value"];
        if (val >= 50 && val <= maxWeightLim) {
            maxWeight = val;
            markSettingsChanged();
        }
    } else if (command == "setCalWeight") {
        int val = payload["value"];
        if (val >= 50 && val <= 10000) {
            calWeight = String(val);
            markSettingsChanged();
        }
    } else if (command == "setViscosity") {
        int val = payload["value"];
        if (val < 0 || val > 3) {
            server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid viscosity\"}");
            return;
        }
        gainSelector = val;

        if (gainSelector == 1) {
            kP[1] = 8; Ti[1] = 10; kD[1] = 5;
        } else if (gainSelector == 2) {
            kP[2] = 8; Ti[2] = 7; kD[2] = 2;
        } else if (gainSelector == 3) {
            kP[3] = 10; Ti[3] = 0; kD[3] = 1;
        }

        initPID(0.1, 1);
        saveSettings();
    } else if (command == "setLanguage") {
        lang = constrain((int)payload["value"], 0, 2);
        saveSettings();
    } else if (command == "setPID") {
        if (gainSelector == 0) {
            if (payload.containsKey("kp")) {
                kP[0] = payload["kp"];
            }
            if (payload.containsKey("ti")) {
                Ti[0] = payload["ti"];
            }
            if (payload.containsKey("kd")) {
                kD[0] = payload["kd"];
            }
            initPID(0.1, 1);
            saveSettings();
        }
    } else if (command == "servoTest") {
        String position = payload["position"];
        servoTestMode = true;
        servoTestStartTime = millis();
        if (position == "min") {
            servoTestOutput = 0.0;
        } else if (position == "max") {
            servoTestOutput = 1.0;
        }
    } else if (command == "calibrate") {
        if (calStateMachine == 0) {
            calStateMachine = 2;
        } else if (calStateMachine == 1) {
            calStateMachine = 2;
        } else if (calStateMachine == 3) {
            calStateMachine = 4;
        }
    } else if (command == "resetStatistics") {
        totalDispensingCycles = 0;
        initializeStats();
        cumulativeDispensedGrams = 0;
        cumulativeTotalError = 0;
        cumulativeTargetGrams = 0;
        saveSettings();
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Unknown command\"}");
        return;
    }

    server.send(200, "application/json", "{\"success\":true}");
}

void handleCaptivePortal() {
    String host = server.hostHeader();

    if (server.uri() == "/generate_204" ||
        server.uri() == "/fwlink" ||
        server.uri() == "/hotspot-detect.html" ||
        server.uri() == "/connectivity-check.html" ||
        server.uri() == "/check_network_status.txt" ||
        server.uri() == "/ncsi.txt") {
        server.sendHeader("Location", "http://192.168.4.1", true);
        server.send(302, "text/plain", "Redirecting...");
        return;
    }

    if (host != "192.168.4.1" && host != "beesmart.local") {
        server.sendHeader("Location", "http://192.168.4.1", true);
        server.send(302, "text/plain", "Redirecting...");
        return;
    }

    String path = server.uri();
    if (path.endsWith("/")) path += "index.html";
    String contentType = "text/plain";
    if (path.endsWith(".html")) contentType = "text/html";
    else if (path.endsWith(".css")) contentType = "text/css";
    else if (path.endsWith(".js")) contentType = "application/javascript";
    else if (path.endsWith(".json")) contentType = "application/json";
    else if (path.endsWith(".png")) contentType = "image/png";
    else if (path.endsWith(".webp")) contentType = "image/webp";
    else if (path.endsWith(".ico")) contentType = "image/x-icon";
    else if (path.endsWith(".svg")) contentType = "image/svg+xml";

    if (LittleFS.exists(path)) {
        File file = LittleFS.open(path);
        server.streamFile(file, contentType);
        file.close();
    } else {
        server.send(404, "text/plain", "File not found");
    }
}

void initWebServer() {
    server.on("/api/settings", HTTP_GET, handleApiSettings);
    server.on("/api/command", HTTP_POST, handleApiCommand);
    server.on("/api/settings", HTTP_OPTIONS, handleCORS);
    server.on("/api/command", HTTP_OPTIONS, handleCORS);

    const char* captivePortalPaths[] = {
        "/generate_204", "/fwlink", "/hotspot-detect.html",
        "/connectivity-check.html", "/check_network_status.txt", "/ncsi.txt"
    };
    for (size_t i = 0; i < sizeof(captivePortalPaths) / sizeof(captivePortalPaths[0]); i++) {
        server.on(captivePortalPaths[i], [](){
            handleCaptivePortal();
        });
    }

    server.onNotFound([](){
        handleCaptivePortal();
    });

    server.begin();
    Serial.println("HTTP web server started");
}
