/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║                          BeeSMART Honey Dosing System                        ║
 * ║                                 Version 3.0.0                                ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 * 
 * @file honeyDosing_v3.ino
 * @brief ESP32-based precision honey dosing system with modern web interface
 * 
 * SYSTEM OVERVIEW:
 * ================
 * This system provides precise, automated honey dispensing using PID control with
 * a modern, responsive web interface. It features real-time weight monitoring,
 * multiple viscosity presets, and comprehensive calibration capabilities.
 * 
 * KEY FEATURES:
 * =============
 * • Modern iOS-like responsive web interface with golden honey theme
 * • HTTP-based communication for maximum stability and compatibility
 * • Captive portal for automatic webpage opening on WiFi connection
 * • PID control with 4 presets (User Defined, Low, Medium, High viscosity)
 * • Real-time weight monitoring with 200ms update rate
 * • Automatic glass detection and tare functionality
 * • Multi-language support (Danish, German, English)
 * • Persistent settings storage with auto-save functionality
 * • Advanced servo calibration and control
 * • Comprehensive scale calibration system
 * 
 * HARDWARE REQUIREMENTS:
 * ======================
 * • ESP32 Development Board
 * • HX711 Load Cell Amplifier (Pins 1 & 3)
 * • Load Cell (connected to HX711)
 * • Servo Motor (Pin 5) for valve control
 * • 5V Power Supply
 * 
 * SOFTWARE ARCHITECTURE:
 * ======================
 * • WebServer: HTTP-based API for frontend communication
 * • LittleFS: File system for settings and calibration storage
 * • ArduPID: Advanced PID control for precise dispensing
 * • DNS Server: Captive portal functionality
 * • JSON: Structured data exchange between frontend and backend
 * 
 * COMMUNICATION PROTOCOL:
 * =======================
 * • HTTP REST API endpoints:
 *   - GET /api/status: System status and weights
 *   - GET /api/settings: Current configuration
 *   - POST /api/command: Execute system commands
 * • 200ms polling rate for real-time weight updates
 * • Auto-save with 5-second debouncing to prevent wear
 * 
 * SAFETY FEATURES:
 * ================
 * • Glass detection prevents operation without container
 * • Maximum weight limits prevent overflow
 * • Emergency stop functionality
 * • Automatic valve closure on completion
 * • Persistent settings backup
 * 
 * AUTHOR: Mogens Groth Nicolaisen 
 * 
 * LICENSE: Open Source - Please maintain attribution
 * DATE: October 2025
 * 
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  For support and updates, visit: https://github.com/beesmart-honey-dosing    ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 */

//═══════════════════════════════════════════════════════════════════════════════
// SYSTEM INCLUDES AND DEPENDENCIES
//═══════════════════════════════════════════════════════════════════════════════
#include <DNSServer.h>       // Captive portal DNS redirection
#include <WiFi.h>            // ESP32 WiFi functionality  
#include <WebServer.h>       // HTTP server for REST API
#include <ArduinoJson.h>     // JSON parsing and generation
#include "ArduPID.h"         // Advanced PID control library
#include <ESP32Servo.h>      // Servo motor control
#include "HX711.h"           // Load cell amplifier interface
#include "FS.h"              // File system interface
#include <LittleFS.h>        // Flash file system

#define FORMAT_LITTLEFS_IF_FAILED true

//═══════════════════════════════════════════════════════════════════════════════
// HARDWARE PIN CONFIGURATION
//═══════════════════════════════════════════════════════════════════════════════
// HX711 Load Cell Amplifier Connections
const int LOADCELL_DOUT_PIN = 3;  // Data output pin
const int LOADCELL_SCK_PIN = 1;   // Serial clock pin
const int SERVO_PIN = 5;          // Servo control pin for dispensing valve

// Hardware instances
HX711 scale;                      // Load cell interface
Servo myservo;                    // Servo motor controller

//═══════════════════════════════════════════════════════════════════════════════
// NETWORK AND SERVER CONFIGURATION  
//═══════════════════════════════════════════════════════════════════════════════
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);   // Access Point IP address
DNSServer dnsServer;              // DNS server for captive portal
WebServer server(80);             // HTTP server on port 80

//═══════════════════════════════════════════════════════════════════════════════
// PID CONTROL SYSTEM
//═══════════════════════════════════════════════════════════════════════════════
ArduPID myController;             // PID controller instance
double input;                     // Current weight ratio (0.0-1.0)
double output;                    // PID output (0.0-1.0) 
double setpoint;                  // Target weight in grams
double setpointPI;                // Normalized setpoint for PID (always 1.0)

// PID Parameters for different honey viscosities
// Index 0: User Defined, 1: Low, 2: Medium, 3: High viscosity
double kP[4] = {8, 8, 8, 10};     // Proportional gains
double Ti[4] = {10, 10, 7, 0};    // Integral time constants
double kD[4] = {5, 5, 2, 1};      // Derivative gains

//═══════════════════════════════════════════════════════════════════════════════
// SYSTEM STATE VARIABLES
//═══════════════════════════════════════════════════════════════════════════════
// Weight measurement variables
int16_t actualWeight = 0;         // Raw weight from scale (grams)
uint16_t adjustedWeight = 0;      // Weight minus glass (honey only)
uint16_t glasWeight = 0;          // Empty glass weight (grams)

// System state machines
uint8_t stateMachine = 4;         // Main dosing state (0-4)
uint8_t calStateMachine = 0;      // Calibration state (0-4)

// System configuration
uint8_t gainSelector = 0;         // Current viscosity preset (0-3)
bool autoState = 0;               // Automatic mode enabled
int looptime = 20;                // PID loop time in milliseconds

// HTTP client state tracking (for optimization)
struct ClientData {
  unsigned long lastUpdate;
  uint8_t stateMachine;
  int16_t actualWeight;
  uint16_t adjustedWeight;
  uint16_t glasWeight;
  uint8_t calStateMachine;
};
ClientData lastSentData = {0, 255, -999, 65535, 65535, 255};

//═══════════════════════════════════════════════════════════════════════════════
// SYSTEM LIMITS AND CONFIGURATION
//═══════════════════════════════════════════════════════════════════════════════
// Weight limits for safety and validation
uint16_t minWeight = 50;          // Minimum dosing amount (grams)
uint16_t maxWeight = 1000;        // Default maximum dosing amount (grams) 
uint16_t maxWeightLim = 20000;    // Absolute maximum weight limit (grams)
int stopHysteresis = 5;           // Stop dosing X grams before target
int minGlassWeight = 10;          // Minimum glass weight for detection (grams)

// Servo configuration
int servoMin = 90;                // Servo minimum position (degrees)
int servoMax = 90;                // Servo maximum position (degrees)

// Language and user interface
int lang = 0;                     // Current language (0=DA, 1=DE, 2=EN)

//═══════════════════════════════════════════════════════════════════════════════
// AUTO-SAVE SYSTEM WITH DEBOUNCING
//═══════════════════════════════════════════════════════════════════════════════
unsigned long lastSettingChange = 0;      // Timestamp of last setting change
const unsigned long AUTOSAVE_DELAY = 5000; // 5 second delay before auto-save
bool settingsChanged = false;              // Flag indicating unsaved changes

// User feedback system
String lastSaveMessage = "";              // Last save confirmation message
bool hasSaveMessage = false;               // Flag for pending save message

//═══════════════════════════════════════════════════════════════════════════════
// CALIBRATION SYSTEM
//═══════════════════════════════════════════════════════════════════════════════
long reading;                     // Calibration reading buffer
float calFactor = 0;              // Scale calibration factor
long calSum = 0;                  // Sum for calibration averaging
int calCount = 0;                 // Sample count for calibration
const int calSamples = 100;       // Number of samples for calibration
bool calAveraging = false;        // Calibration averaging in progress
String calWeight = "250";         // Calibration weight value (grams)
uint8_t cnt = 0;                  // General purpose counter

//═══════════════════════════════════════════════════════════════════════════════
// PERSISTENT SETTINGS SYSTEM
//═══════════════════════════════════════════════════════════════════════════════
// Settings string format: kP0,Ti0,kD0,kP1,Ti1,kD1,kP2,Ti2,kD2,kP3,Ti3,kD3,
//                        desiredAmount,servoMin,servoMax,lang,hysteresis,
//                        glasWeight,maxWeight,viscosity
String settings = "8,10,5,8,10,5,8,7,2,10,0,1,300,1,180,0,5,10,1000,0";
String array;                     // File read buffer
int indx[20];                     // Index array for settings parsing
String desiredAmount = String(minWeight + maxWeight/2);  // Current target weight

//═══════════════════════════════════════════════════════════════════════════════
// MULTI-LANGUAGE SYSTEM
//═══════════════════════════════════════════════════════════════════════════════

// Language identifiers and system messages
String langString[3] = {"DA", "DE", "EN"};  // Danish, German, English

// Save confirmation messages
String saveStateText[3] = {
  "Indstillinger gemt",              // Danish
  "Einstellungen gespeichert",       // German  
  "Settings saved"                   // English
};

// Main dosing process status messages
String step1Text[3] = {
  "Tryk start",                      // Danish: Press start
  "Start drücken",                  // German: Press start
  "Press start"                     // English: Press start
};

String step2Text[3] = {
  "Placer glas på vægt",            // Danish: Place glass on scale
  "Glas auf die Wage platzieren",   // German: Place glass on scale
  "Place glass on scale"            // English: Place glass on scale
};

String step3Text[3] = {
  "Fylder glas...",                 // Danish: Filling glass...
  "Glas füllen...",                // German: Filling glass...
  "Filling glass..."               // English: Filling glass...
};

String step4Text[3] = {
  "Glas fyldt - fjern glas",        // Danish: Glass filled - remove glass
  "Glas gefüllt - Glas entfernen", // German: Glass filled - remove glass
  "Glass filled - remove glass"    // English: Glass filled - remove glass
};

// Calibration process status messages
String calStep1Text[3] = {
  "System kalibreret, tryk Kalibrer for at starte re-kalibrering",     // Danish
  "System kalibriert, für Neukalibrierung drücken Sie Kalibrieren",    // German
  "System calibrated, press Calibrate to start re-calibration"         // English
};

String calStep2Text[3] = {
  "Tøm vægt og tryk Kalibrer",                    // Danish: Remove weight and press Calibrate
  "Gewicht entfernen und Kalibrieren drücken",   // German: Remove weight and press Calibrate
  "Remove any weight from scale and press Calibrate"  // English: Remove weight and press Calibrate
};

String calStep3Text[3] = {
  "Kalibrerer...",                  // Danish: Calibrating...
  "Kalibriert...",                 // German: Calibrating...
  "Calibrating..."                 // English: Calibrating...
};

String calStep4Text[3] = {
  "Placer kalibreringsvægt og tryk Kalibrer",           // Danish: Place calibration weight and press Calibrate
  "Kalibriergewicht auflegen und Kalibrieren drücken", // German: Place calibration weight and press Calibrate
  "Place calibration weight on scale and press calibrate"  // English: Place calibration weight and press Calibrate
};

String stopText[3] = {
  "Stop aktiveret!",               // Danish: Stop activated!
  "Stop aktiviert!",              // German: Stop activated!
  "Stop activated!"               // English: Stop activated!
};

//═══════════════════════════════════════════════════════════════════════════════
// FILE SYSTEM AND PERSISTENCE FUNCTIONS
//═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Load system settings from persistent storage
 * @param fs File system reference (LittleFS)
 * @param path Path to settings file ("/data.txt")
 * 
 * Loads all system configuration from comma-separated values file:
 * - All 4 PID parameter sets (User Defined, Low, Medium, High viscosity)
 * - System limits and preferences
 * - Language and interface settings
 * - Hardware calibration values
 */
void readFile(fs::FS &fs, const char * path) {
    File file = fs.open(path);
    if (!file || file.isDirectory()) {
        Serial.println("Failed to open settings file: " + String(path));
        return;
    }
    
    // Read entire file content
    while(file.available()) {
        array = file.readString();
    }
    file.close();
    
    // Parse comma-separated values (CSV format)
    // Settings format: kP0,Ti0,kD0,kP1,Ti1,kD1,kP2,Ti2,kD2,kP3,Ti3,kD3,
    //                  amount,servoMin,servoMax,lang,hysteresis,glassWeight,maxWeight,viscosity
    indx[0] = 0;
    for (int i = 1; i <= 19; i++) {
        indx[i] = array.indexOf(',', indx[i-1] + 1);
    }
    
    // Load PID parameters for all 4 viscosity presets
    // Set 0: User Defined, Set 1: Low, Set 2: Medium, Set 3: High
    for (int set = 0; set < 4; set++) {
        int baseIdx = set * 3;
        kP[set] = array.substring(indx[baseIdx], indx[baseIdx + 1]).toDouble();
        Ti[set] = array.substring(indx[baseIdx + 1] + 1, indx[baseIdx + 2]).toDouble();
        kD[set] = array.substring(indx[baseIdx + 2] + 1, indx[baseIdx + 3]).toDouble();
    }
    
    // Load system configuration parameters
    desiredAmount = array.substring(indx[12] + 1, indx[13]);
    servoMin = array.substring(indx[13] + 1, indx[14]).toInt();
    servoMax = array.substring(indx[14] + 1, indx[15]).toInt();
    lang = array.substring(indx[15] + 1, indx[16]).toInt();
    stopHysteresis = array.substring(indx[16] + 1, indx[17]).toInt();
    minGlassWeight = array.substring(indx[17] + 1, indx[18]).toInt();
    maxWeight = array.substring(indx[18] + 1, indx[19]).toInt();
    
    // Load viscosity preset selector (optional parameter)
    if (indx[19] != -1) {
        gainSelector = array.substring(indx[19] + 1).toInt();
    }
    
    Serial.println("Settings loaded successfully from: " + String(path));
}

/**
 * @brief Load scale calibration factor from persistent storage
 * @param fs File system reference (LittleFS)
 * @param path Path to calibration file ("/cal.txt")
 * 
 * Reads the scale calibration factor used to convert raw HX711 readings
 * to actual weight values in grams. The calibration factor is determined
 * during the calibration process using a known reference weight.
 */
void readCal(fs::FS &fs, const char * path) {
    File file = fs.open(path);
    if (!file || file.isDirectory()) {
        Serial.println("Calibration file not found: " + String(path));
        return;
    }
    
    String calArray;
    while(file.available()) {
        calArray = file.readString();
    }
    file.close();
    
    // Parse calibration factor (first value before comma)
    int calIndx = calArray.indexOf(',');
    if (calIndx > 0) {
        calFactor = calArray.substring(0, calIndx).toFloat();
        Serial.println("Calibration factor loaded: " + String(calFactor));
    }
}

/**
 * @brief Write data to file in LittleFS
 * @param fs File system reference (LittleFS)
 * @param path Target file path
 * @param message Data to write to file
 * 
 * General purpose file writing function with error handling.
 * Includes a small delay to ensure write completion before returning.
 */
void writeFile(fs::FS &fs, const char * path, String message) {
    File file = fs.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing: " + String(path));
        return;
    }
    
    file.print(message);
    file.close();
    delay(100); // Ensure write completion
}

/**
 * @brief Save all system settings to persistent storage
 * 
 * Constructs a comma-separated values string containing all system configuration
 * and writes it to LittleFS. Includes all PID parameter sets, system limits,
 * language preferences, and hardware calibration values. Provides user feedback
 * through save status messages.
 */
void saveSettings() {
    // Build PID parameters string for all 4 viscosity presets
    String pidParams = "";
    for (int i = 0; i < 4; i++) {
        pidParams += String(kP[i]) + "," + String(Ti[i]) + "," + String(kD[i]);
        if (i < 3) pidParams += ",";
    }
    
    // Construct complete settings string
    settings = pidParams + "," + 
               desiredAmount + "," + 
               String(servoMin) + "," + 
               String(servoMax) + "," + 
               String(lang) + "," + 
               String(stopHysteresis) + "," + 
               String(minGlassWeight) + "," + 
               String(maxWeight) + "," + 
               String(gainSelector);
    
    // Write to persistent storage
    writeFile(LittleFS, "/data.txt", settings);
    
    // Set user feedback message
    lastSaveMessage = saveStateText[lang];
    hasSaveMessage = true;
    settingsChanged = false; // Reset change flag
    
    Serial.println("Settings saved to persistent storage");
}

/**
 * @brief Mark settings as changed for debounced auto-save
 * 
 * Triggers the auto-save timer to prevent excessive write operations
 * to flash memory. Settings will be automatically saved after the
 * debounce delay expires without further changes.
 */
void markSettingsChanged() {
    settingsChanged = true;
    lastSettingChange = millis();
}

/**
 * @brief Check if auto-save should trigger (call from main loop)
 * 
 * Implements debounced auto-save to prevent flash wear from frequent
 * write operations. Settings are saved automatically after 5 seconds
 * of no changes to protect against power loss or system reset.
 */
void checkAutoSave() {
    if (settingsChanged && (millis() - lastSettingChange >= AUTOSAVE_DELAY)) {
        saveSettings();
        Serial.println("Auto-saved settings after " + String(AUTOSAVE_DELAY/1000) + " second delay");
    }
}

//═══════════════════════════════════════════════════════════════════════════════
// PID CONTROL SYSTEM FUNCTIONS
//═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Initialize PID controller with current parameters and limits
 * @param lowlim Minimum PID output value (0.0 = closed valve)
 * @param highlim Maximum PID output value (1.0 = fully open valve)
 * 
 * Configures the PID controller with the currently selected viscosity preset.
 * The PID input is normalized weight ratio (current/target), and output is
 * a 0.0-1.0 value that controls servo position for the dispensing valve.
 */
void initPID(float lowlim, float highlim) {
    // Configure PID with current viscosity preset parameters
    myController.begin(&input, &output, &setpointPI, 
                      kP[gainSelector]/10,        // Proportional gain (scaled)
                      0.01/Ti[gainSelector],      // Integral coefficient  
                      kD[gainSelector]);          // Derivative gain
    
    // Set control loop timing and constraints
    myController.setSampleTime(looptime);        // 20ms control loop
    myController.setOutputLimits(lowlim, highlim); // Valve position limits
    myController.setWindUpLimits(0, 0.5);        // Anti-windup protection
    
    // Ensure system starts in stopped state
    stopSystem();
    
    Serial.println("PID initialized with viscosity preset: " + String(gainSelector));
}

/**
 * @brief Stop the dosing system and reset to safe state
 * 
 * Immediately stops PID control, closes the dispensing valve, disables
 * automatic mode, and resets the system to idle state. This function
 * provides emergency stop functionality and normal operation completion.
 */
void stopSystem() {
    myController.stop();     // Halt PID calculations
    myController.reset();    // Clear PID integral/derivative history
    autoState = 0;           // Disable automatic mode
    output = 0;              // Close dispensing valve
    stateMachine = 4;        // Set to idle state
    
    // Notify connected clients of status change
    sendStatusUpdate();
    
    Serial.println("Dosing system stopped and reset to safe state");
}

//═══════════════════════════════════════════════════════════════════════════════
// COMMUNICATION SYSTEM (HTTP POLLING ARCHITECTURE)
//═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Placeholder for status updates (HTTP polling model)
 * 
 * In the HTTP polling architecture, data is pulled by the client every 200ms
 * rather than pushed via WebSocket. This ensures maximum compatibility and
 * stability across different network conditions and devices.
 */
void sendStatusUpdate() {
    // Status data is provided via GET /api/status endpoint
    // Client polls every 200ms for real-time updates
}

/**
 * @brief Placeholder for weight updates (HTTP polling model)
 */
void sendWeightUpdate() {
    // Weight data is included in status endpoint response
    // Updated every 200ms via HTTP polling
}

/**
 * @brief Placeholder for settings updates (HTTP polling model)
 */
void sendSettingsUpdate() {
    // Settings data is provided via GET /api/settings endpoint
    // Retrieved when needed by client interface
}

/**
 * @brief Placeholder for calibration updates (HTTP polling model)
 */
void sendCalibrationUpdate() {
    // Calibration status is included in status endpoint response
    // Updated in real-time via HTTP polling
}

//═══════════════════════════════════════════════════════════════════════════════
// HTTP REST API SYSTEM
//═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Set Cross-Origin Resource Sharing (CORS) headers
 * 
 * Enables web browsers to make requests to the ESP32 server from any origin.
 * This is essential for the captive portal functionality and allows the
 * modern web interface to communicate with the backend API.
 */
void setCORSHeaders() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

/**
 * @brief Handle CORS preflight requests
 * 
 * Responds to browser preflight requests with appropriate CORS headers.
 * This enables complex requests like POST with JSON payloads to function
 * correctly in modern web browsers.
 */
void handleCORS() {
    setCORSHeaders();
    server.send(200, "text/plain", "");
}

// HTTP API handlers
void handleApiStatus() {
  String statusMsg = "";
  switch(stateMachine) {
    case 1: statusMsg = step2Text[lang]; break;
    case 2:
    case 3: statusMsg = step3Text[lang]; break;
    case 4: statusMsg = (actualWeight < minGlassWeight && autoState == 1) ? step1Text[lang] : 
                        (actualWeight < minGlassWeight) ? step1Text[lang] : step4Text[lang]; break;
    default: statusMsg = step1Text[lang]; break;
  }
  
  String json = "{\"running\":" + String(stateMachine != 4 ? "true" : "false") + 
                ",\"stateMachine\":" + String(stateMachine) + 
                ",\"autoState\":" + String(autoState ? "true" : "false") + 
                ",\"message\":\"" + statusMsg + 
                "\",\"weights\":{\"total\":" + String(actualWeight) + 
                ",\"honey\":" + String(adjustedWeight) + 
                ",\"glass\":" + String(glasWeight) + "}";
  
  // Add calibration info
  String calMsg = "";
  switch(calStateMachine) {
    case 0: calMsg = calStep1Text[lang]; break;
    case 1: calMsg = calStep2Text[lang]; break;
    case 2:
    case 4: calMsg = calStep3Text[lang]; break;
    case 3: calMsg = calStep4Text[lang]; break;
  }
  json += ",\"calibration\":{\"message\":\"" + calMsg + "\",\"state\":" + String(calStateMachine) + "}";
  
  // Add save message if available
  if (hasSaveMessage) {
    json += ",\"saveStatus\":{\"success\":true,\"message\":\"" + lastSaveMessage + "\"}";
    hasSaveMessage = false;
    lastSaveMessage = "";
  }
  
  json += "}";
  
  server.send(200, "application/json", json);
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
  deserializeJson(doc, body);
  
  String command = doc["command"];
  JsonObject payload = doc["payload"];
  
  // Handle commands (same logic as before)
  if (command == "start") {
    if (stateMachine == 4) {
      stateMachine = 1;
    }
  }
  else if (command == "stop") {
    stopSystem();
  }
  else if (command == "tare") {
    scale.tare();
  }
  else if (command == "setAuto") {
    autoState = payload["value"];
    markSettingsChanged(); // Auto-save after delay
  }
  else if (command == "setAmount") {
    desiredAmount = String((int)payload["value"]);
    markSettingsChanged(); // Auto-save after delay
  }
  else if (command == "setServoMin") {
    servoMin = payload["value"];
    markSettingsChanged(); // Auto-save after delay
  }
  else if (command == "setServoMax") {
    servoMax = payload["value"];
    markSettingsChanged(); // Auto-save after delay
  }
  else if (command == "setStopHysteresis") {
    stopHysteresis = payload["value"];
    markSettingsChanged(); // Auto-save after delay
  }
  else if (command == "setMinGlassWeight") {
    minGlassWeight = payload["value"];
    markSettingsChanged(); // Auto-save after delay
  }
  else if (command == "setMaxWeight") {
    maxWeight = payload["value"];
    markSettingsChanged(); // Auto-save after delay
  }
  else if (command == "setCalWeight") {
    calWeight = String((int)payload["value"]);
    markSettingsChanged(); // Auto-save after delay
  }
  else if (command == "setViscosity") {
    gainSelector = payload["value"];
    
    // Reset preset viscosity levels to their default values
    if (gainSelector == 1) { // Low viscosity
      kP[1] = 8; Ti[1] = 10; kD[1] = 5;
    } else if (gainSelector == 2) { // Medium viscosity  
      kP[2] = 8; Ti[2] = 7; kD[2] = 2;
    } else if (gainSelector == 3) { // High viscosity
      kP[3] = 10; Ti[3] = 0; kD[3] = 1;
    }
    // gainSelector 0 (User Defined) keeps its saved values
    
    initPID(0.1, 1);
    saveSettings(); // Save viscosity setting and any preset resets
  }
  else if (command == "setLanguage") {
    lang = payload["value"];
    saveSettings(); // Immediately save critical setting
  }
  else if (command == "setPID") {
    // Only allow PID parameter changes for User Defined (viscosity 0)
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
      initPID(0.1, 1); // Re-initialize PID with new parameters
      saveSettings(); // Save user-defined parameters
    }
  }
  else if (command == "servoTest") {
    String position = payload["position"];
    if (position == "min") {
      output = 0;
    } else if (position == "max") {
      output = 1;
    }
  }
  else if (command == "calibrate") {
    if (calStateMachine == 0) {
      calStateMachine = 1;
    } else if (calStateMachine == 1) {
      calStateMachine = 2;
    } else if (calStateMachine == 3) {
      calStateMachine = 4;
    }
  }

  
  server.send(200, "application/json", "{\"success\":true}");
}

// Handle captive portal detection and redirect to main interface
void handleCaptivePortal() {
  String host = server.hostHeader();
  
  // Check if this is a captive portal detection request
  if (server.uri() == "/generate_204" || 
      server.uri() == "/fwlink" || 
      server.uri() == "/hotspot-detect.html" || 
      server.uri() == "/connectivity-check.html" || 
      server.uri() == "/check_network_status.txt" || 
      server.uri() == "/ncsi.txt") {
    
    // Respond with redirect to force captive portal
    server.sendHeader("Location", "http://192.168.4.1/");
    server.send(302, "text/html", "");
    return;
  }
  
  // For any other unhandled request, redirect to main interface
  if (host != "192.168.4.1" && host != "beesmart.local") {
    server.sendHeader("Location", "http://192.168.4.1/");
    server.send(302, "text/html", "");
    return;
  }
  
  // If already on correct host, serve main page
  File file = LittleFS.open("/index.html", "r");
  if (file) {
    server.streamFile(file, "text/html");
    file.close();
  } else {
    server.send(404, "text/html", "File not found");
  }
}

// Initialize web server and routes
void initWebServer() {
  // API endpoints
  server.on("/api/status", HTTP_GET, [](){
    setCORSHeaders();
    handleApiStatus();
  });
  
  server.on("/api/settings", HTTP_GET, [](){
    setCORSHeaders();
    handleApiSettings();
  });
  
  server.on("/api/command", HTTP_POST, [](){
    setCORSHeaders();
    handleApiCommand();
  });
  
  server.on("/api/command", HTTP_OPTIONS, handleCORS);
  
  // Serve static files from LittleFS
  server.on("/", [](){
    File file = LittleFS.open("/index.html", "r");
    if (file) {
      server.streamFile(file, "text/html");
      file.close();
    } else {
      server.send(404, "text/plain", "File not found");
    }
  });
  
  server.on("/styles.css", [](){
    File file = LittleFS.open("/styles.css", "r");
    if (file) {
      server.streamFile(file, "text/css");
      file.close();
    } else {
      server.send(404, "text/plain", "File not found");
    }
  });
  
  server.on("/app.js", [](){
    File file = LittleFS.open("/app.js", "r");
    if (file) {
      server.streamFile(file, "application/javascript");
      file.close();
    } else {
      server.send(404, "text/plain", "File not found");
    }
  });
  
  server.on("/beesmart_bee.png", [](){
    File file = LittleFS.open("/beesmart_bee.png", "r");
    if (file) {
      server.streamFile(file, "image/png");
      file.close();
    } else {
      server.send(404, "text/plain", "File not found");
    }
  });

  // Captive portal detection endpoints
  server.on("/generate_204", [](){
    handleCaptivePortal();
  });
  
  server.on("/fwlink", [](){
    handleCaptivePortal();
  });
  
  server.on("/hotspot-detect.html", [](){
    handleCaptivePortal();
  });
  
  server.on("/connectivity-check.html", [](){
    handleCaptivePortal();
  });
  
  server.on("/check_network_status.txt", [](){
    handleCaptivePortal();
  });
  
  server.on("/ncsi.txt", [](){
    handleCaptivePortal();
  });

  // Handle 404 - redirect to captive portal
  server.onNotFound([](){
    handleCaptivePortal();
  });

  // Start server
  server.begin();
  Serial.println("HTTP web server started");
}

//═══════════════════════════════════════════════════════════════════════════════
// SYSTEM INITIALIZATION
//═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief System initialization and hardware setup
 * 
 * Initializes all system components in the correct order:
 * 1. Serial communication for debugging
 * 2. File system for persistent storage
 * 3. Load saved settings and calibration
 * 4. Configure hardware (scale, servo, PID)
 * 5. Setup WiFi access point with unique name
 * 6. Start DNS server for captive portal
 * 7. Initialize HTTP web server and API
 * 8. Perform initial system calibration
 */
void setup(void) {
    Serial.begin(115200);
    Serial.println("\n" + String('═', 80));
    Serial.println("BeeSMART Honey Dosing System v3.0.0 - Initializing...");
    Serial.println(String('═', 80));
    
    // Initialize communication system state
    lastSentData = {0, 255, -999, 65535, 65535, 255};
    Serial.println("HTTP polling system initialized");
    
    // Initialize file system for persistent storage
    if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
        Serial.println("CRITICAL: LittleFS Mount Failed");
        ESP.restart();
    }
    Serial.println("LittleFS file system mounted successfully");
    
    // Load system configuration and calibration
    readCal(LittleFS, "/cal.txt");   // Load scale calibration factor
    readFile(LittleFS, "/data.txt"); // Load system settings
    
    // Initialize hardware components
    Serial.println("Initializing hardware components...");
    
    // Configure HX711 load cell interface
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scale.set_scale(calFactor);
    Serial.println("Scale interface initialized with calibration factor: " + String(calFactor));
    
    // Configure servo motor for dispensing valve
    myservo.attach(SERVO_PIN);
    Serial.println("Servo motor attached to pin " + String(SERVO_PIN));
    
    // Initialize PID control system
    initPID(0.1, 1);
    
    // Setup WiFi Access Point with unique identifier
    WiFi.mode(WIFI_AP);
    delay(100);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    
    // Generate unique AP name based on chip ID
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
    
    // Start DNS server for captive portal functionality
    dnsServer.start(DNS_PORT, "*", apIP);
    Serial.println("DNS server started for captive portal");
    
    // Initialize HTTP web server and REST API
    initWebServer();
    
    // Perform initial system reset and tare
    myController.stop();
    scale.tare();
    Serial.println("Scale tared and system reset to idle state");
    
    Serial.println(String('═', 80));
    Serial.println("BeeSMART Honey Dosing System v3.0.0 Ready!");
    Serial.println("Connect to WiFi: " + String(ap_ssid));
    Serial.println("Open browser - captive portal will redirect automatically");
    Serial.println(String('═', 80) + "\n");
}

//═══════════════════════════════════════════════════════════════════════════════
// MAIN CONTROL LOOP
//═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Main system control loop
 * 
 * Executes continuously and handles:
 * 1. Network communication (DNS and HTTP requests)
 * 2. Auto-save system with debouncing
 * 3. Weight measurement and processing
 * 4. Calibration state machine
 * 5. Main dosing state machine
 * 6. PID control execution
 * 7. Servo position control
 * 
 * Loop timing is optimized for 20ms PID control cycles while maintaining
 * responsive network communication and user interface updates.
 */

void loop(void)
{
  // Handle network tasks
  dnsServer.processNextRequest();
  server.handleClient();
  
  // Check for auto-save (debounced)
  checkAutoSave();
  
  actualWeight = scale.get_units(); // Current weight reading from scale
  int16_t temp = (actualWeight-glasWeight);
  adjustedWeight = max((int16_t)0,temp); // Weight from scale minus glass weight, floored to zero

  // Handle calibration state machine
  switch(calStateMachine){
    case 0: // Check if calibration exists
      if (LittleFS.exists("/cal.txt") == 0){ // No calibration data saved
        calStateMachine = 1;
        sendCalibrationUpdate();
      }
      break;

    case 1: // Wait for calibration button press
      break;

    case 2: // Perform tare operation
        scale.set_scale();
        scale.tare();
        calStateMachine = 3;
        sendCalibrationUpdate();
      break;
    
    case 3: // Wait for calibration weight placement and button press
      break;

    case 4:
      if (!calAveraging) {
        calSum = 0;
        calCount = 0;
        calAveraging = true;
      }

      // Take one sample per loop
      calSum += scale.get_units(1);
      calCount++;

      if (calCount >= calSamples) {
        reading = calSum / calSamples;
        writeFile(LittleFS, "/cal.txt", String((reading / calWeight.toInt())) + ",");
        scale.set_scale(reading / calWeight.toInt());
        calAveraging = false;
        calStateMachine = 0;
        sendCalibrationUpdate();
      }
      break;

    default:
      break;
  }  

  // Handle main dosing state machine
  switch (stateMachine){
    case 1: // Calibrate glass weight when start is pressed
      if(actualWeight < minGlassWeight){ // Less than minimum weight - no glass detected
        cnt = 0;
      }
      else{
        cnt++;
      }
      if(cnt > 10){ // Glass presence confirmed after 10 readings
        glasWeight = actualWeight; // Record glass weight
        cnt = 0;
        stateMachine = 2;
        sendStatusUpdate();
      }
      break;
    case 2: // Start PID control system
      setpoint = desiredAmount.toInt(); // Lock in selected target weight
      setpointPI = setpoint/setpoint;
      myController.start();
      stateMachine = 3; // Move to filling state
      sendStatusUpdate();
      break;
    case 3: // Fill glass to target weight
      if(setpoint-adjustedWeight<stopHysteresis){ // When close enough to target (within hysteresis)
        myController.stop(); 
        myController.reset();
        output = 0;
        stateMachine = 4; // Move to complete state
        sendStatusUpdate();
      }
      break;
    case 4: // Dosing complete - wait for glass removal
        myController.stop(); 
        myController.reset();
        if(actualWeight < minGlassWeight){ // Glass has been removed
          if(autoState == 1){
            stateMachine = 1; // Auto-restart without requiring start button
            cnt = 0;
          }
        }
      break;
    default:
      break;
  }

  input = adjustedWeight/setpoint; // Calculate PID input: current weight ratio (0.0-1.0)
  myController.compute();
  myservo.write(servoMin+output*(servoMax-servoMin)); // Set servo position based on PID output
  
  // No periodic updates needed - data is pulled by HTTP polling
  // Small delay to prevent watchdog issues
  delay(10);
}