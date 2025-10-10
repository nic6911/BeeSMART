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
 * ║  For support and updates, visit: https://github.com/nic6911/BeeSMART         ║
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

// Stop condition safety variables
uint8_t stopConditionCount = 0;   // Counter for stable stop condition
const uint8_t STOP_CONFIRM_CYCLES = 3; // Require 3 consecutive cycles before stopping

// Rolling average for stable weight readings
const uint8_t WEIGHT_SAMPLES = 5;     // Number of samples for rolling average
int16_t weightSamples[WEIGHT_SAMPLES]; // Circular buffer for weight samples
uint8_t sampleIndex = 0;              // Current position in circular buffer
bool samplesInitialized = false;      // Flag to track if buffer is filled
int32_t weightSum = 0;                // Sum of current samples for quick calculation

// System configuration
uint8_t gainSelector = 2;         // Current viscosity preset (0-3), default: Medium viscosity
//──────────────────────────────────────────────────────────────────────────────
// Persistent cumulative statistics (across reboots)
// Added in v3.1.0 - appended to settings file for backward compatibility
//──────────────────────────────────────────────────────────────────────────────
uint32_t cumulativeDispensedGrams = 0;  // Sum of all actual dispensed grams
int32_t  cumulativeTotalError    = 0;   // Sum of (actual - target) grams
uint32_t cumulativeTargetGrams   = 0;   // Sum of target grams (for alternate metrics)

bool autoState = 0;               // Automatic mode enabled
int looptime = 20;                // PID loop time in milliseconds

// HTTP client state tracking (removed - not used in HTTP polling architecture)

//═══════════════════════════════════════════════════════════════════════════════
// DISPENSING STATISTICS SYSTEM
//═══════════════════════════════════════════════════════════════════════════════
/**
 * Simple dispensing record structure for tracking performance metrics.
 * Maintains basic statistics without complex learning algorithms.
 */
struct DispensingRecord {
  float targetWeight;                   // Target weight for this cycle
  float actualWeight;                   // Measured final weight
  float error;                          // Dispensing error (actual - target)
  unsigned long timestamp;              // When dispensing was completed
};

// Statistics storage
DispensingRecord dispensingHistory[10]; // Rolling buffer for recent history
int historyIndex = 0;                   // Current position in circular buffer
int historyCount = 0;                   // Number of valid records (max 10)
int totalDispensingCycles = 0;          // Total lifetime dispensing count

//═══════════════════════════════════════════════════════════════════════════════
// SYSTEM LIMITS AND CONFIGURATION
//═══════════════════════════════════════════════════════════════════════════════
// Weight limits for safety and validation
uint16_t minWeight = 50;          // Minimum dosing amount (grams)
uint16_t maxWeight = 1000;        // Default maximum dosing amount (grams) 
uint16_t maxWeightLim = 20000;    // Absolute maximum weight limit (grams)
int stopHysteresis = 2;           // Stop dosing X grams before target (reduced for better accuracy)
int minGlassWeight = 10;          // Minimum glass weight for detection (grams)

// Servo configuration
int servoMin = 0;                // Servo minimum position (degrees)
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

// Servo test mode variables  
bool servoTestMode = false;       // Servo test mode active flag
float servoTestOutput = 0.0;      // Test output value (0.0-1.0)
unsigned long servoTestStartTime = 0; // Test start time for timeout

// Calibration timing variables
bool tareInProgress = false;      // Tare operation in progress
unsigned long tareStartTime = 0;  // Tare operation start time

//═══════════════════════════════════════════════════════════════════════════════
// PERSISTENT SETTINGS SYSTEM
//═══════════════════════════════════════════════════════════════════════════════
// Settings string format: kP0,Ti0,kD0,kP1,Ti1,kD1,kP2,Ti2,kD2,kP3,Ti3,kD3,
//                        desiredAmount,servoMin,servoMax,lang,hysteresis,
//                        glasWeight,maxWeight,totalCycles,viscosity
String settings = "8,10,5,8,10,5,8,7,2,10,0,1,300,1,180,0,5,10,1000,0,0";
String array;                     // File read buffer
int indx[25];                     // Index array for settings parsing (increased for safety)
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
  // Legacy format (<=v3.0.0): kP0,Ti0,kD0,kP1,Ti1,kD1,kP2,Ti2,kD2,kP3,Ti3,kD3,
  //                            amount,servoMin,servoMax,lang,hysteresis,glassWeight,maxWeight,totalCycles,viscosity
  // Extended format (>=v3.1.0) appends: cumulativeDispensedGrams,cumulativeTotalError,cumulativeTargetGrams
  indx[0] = 0;
  for (int i = 1; i <= 24; i++) { // allow room for new fields
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
    totalDispensingCycles = array.substring(indx[19] + 1, indx[20]).toInt();
    
  // Load viscosity preset selector (optional parameter)
  if (indx[20] != -1) {
    if (indx[21] != -1) {
      gainSelector = array.substring(indx[20] + 1, indx[21]).toInt();
    } else {
      gainSelector = array.substring(indx[20] + 1).toInt();
    }
  }

  // Load cumulative statistics if present (require at least indices 21-23)
  if (indx[21] != -1 && indx[22] != -1 && indx[23] != -1) {
    cumulativeDispensedGrams = array.substring(indx[21] + 1, indx[22]).toInt();
    cumulativeTotalError    = array.substring(indx[22] + 1, indx[23]).toInt();
    if (indx[24] != -1) {
      cumulativeTargetGrams = array.substring(indx[23] + 1, indx[24]).toInt();
    } else {
      cumulativeTargetGrams = array.substring(indx[23] + 1).toInt();
    }
    Serial.println("Loaded cumulative stats: disp=" + String(cumulativeDispensedGrams) + "g errSum=" + String(cumulativeTotalError) + "g tgt=" + String(cumulativeTargetGrams) + "g");
  } else {
    Serial.println("Legacy settings file - cumulative stats start at zero");
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
         String(totalDispensingCycles) + "," + 
         String(gainSelector) + "," +
         String(cumulativeDispensedGrams) + "," +
         String(cumulativeTotalError) + "," +
         String(cumulativeTargetGrams);
    
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

/**
 * @brief Record basic dispensing statistics
 * 
 * Records dispensing data for performance tracking and statistics display.
 * Uses a circular buffer to maintain the last 10 dispensing cycles.
 */
void recordDispensingStats() {
    if (setpoint > 0) {
        // Create new record
        DispensingRecord newRecord;
        newRecord.targetWeight = setpoint;
        newRecord.actualWeight = adjustedWeight;
        newRecord.error = adjustedWeight - setpoint;
        newRecord.timestamp = millis();
        
        // Store in circular buffer
        dispensingHistory[historyIndex] = newRecord;
        historyIndex = (historyIndex + 1) % 10;
        
        if (historyCount < 10) {
            historyCount++;
        }
        
    // Increment total cycle count
    totalDispensingCycles++;

    // Update cumulative persistent statistics
    cumulativeDispensedGrams += (uint32_t)adjustedWeight;
    cumulativeTargetGrams   += (uint32_t)setpoint;
    cumulativeTotalError    += (int32_t)(adjustedWeight - setpoint);
        
    // Trigger settings save to preserve statistics (debounced)
    if(!(totalDispensingCycles % 20)){
      markSettingsChanged();
    }
    }
}

/**
 * @brief Calculate basic statistics from dispensing history
 * 
 * @param avgError Pointer to store average error in grams
 * @param totalDispensed Pointer to store total dispensed weight in kg
 * 
 * Calculates basic performance metrics from the circular buffer history.
 */
void calculateBasicStats(float* avgError, float* totalDispensed) {
    *avgError = 0.0;
    *totalDispensed = 0.0;
    
    if (historyCount == 0) return;
    
    float totalError = 0.0;
    
    for (int i = 0; i < historyCount; i++) {
        totalError += dispensingHistory[i].error;
        *totalDispensed += dispensingHistory[i].actualWeight;
    }
    
    *avgError = totalError / historyCount;
    *totalDispensed = *totalDispensed / 1000.0; // Convert grams to kg
}

/**
 * @brief Initialize weight sampling system
 * 
 * Sets up the rolling average buffer for stable weight readings.
 * Should be called during system startup.
 */
void initializeWeightSampling() {
    sampleIndex = 0;
    samplesInitialized = false;
    weightSum = 0;
    
    // Clear sample buffer
    for (int i = 0; i < WEIGHT_SAMPLES; i++) {
        weightSamples[i] = 0;
    }
    
    Serial.println("Weight sampling system initialized with " + String(WEIGHT_SAMPLES) + " sample rolling average");
}

/**
 * @brief Get stable weight reading using rolling average
 * 
 * @return Averaged weight reading from the last WEIGHT_SAMPLES readings
 * 
 * Takes multiple rapid samples from the HX711 and maintains a rolling average
 * to provide stable, noise-free weight readings.
 */
int16_t getStableWeight() {
    // Get new raw reading from scale
    int16_t newReading = scale.get_units(1); // Single reading for speed
    
    // Add new reading to circular buffer
    if (samplesInitialized) {
        // Remove old sample from sum
        weightSum -= weightSamples[sampleIndex];
    }
    
    // Add new sample
    weightSamples[sampleIndex] = newReading;
    weightSum += newReading;
    
    // Move to next position in circular buffer
    sampleIndex = (sampleIndex + 1) % WEIGHT_SAMPLES;
    
    // Check if we've filled the buffer for the first time
    if (!samplesInitialized && sampleIndex == 0) {
        samplesInitialized = true;
        Serial.println("Weight sampling buffer filled - stable readings available");
    }
    
    // Return average if buffer is full, otherwise return current reading
    if (samplesInitialized) {
        return weightSum / WEIGHT_SAMPLES;
    } else {
        return newReading; // Use direct reading until buffer is full
    }
}

/**
 * @brief Initialize statistics system
 * 
 * Clears all dispensing history and resets counters to zero.
 * Called during system startup or when statistics are reset.
 */
void initializeStats() {
    historyCount = 0;
    historyIndex = 0;
    
    // Clear history buffer
    for (int i = 0; i < 10; i++) {
        dispensingHistory[i] = {0, 0, 0, 0};
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
    stopConditionCount = 0;  // Reset stop condition counter
    
    Serial.println("SYSTEM STOPPED - Auto-start disabled. Weight: " + String(adjustedWeight) + "g, Target: " + String(setpoint) + "g");
}

//═══════════════════════════════════════════════════════════════════════════════
// COMMUNICATION SYSTEM (HTTP POLLING ARCHITECTURE)
//═══════════════════════════════════════════════════════════════════════════════
/**
 * HTTP Polling Architecture:
 * 
 * This system uses HTTP polling instead of WebSocket connections for maximum
 * compatibility and reliability. The web interface polls the /api/status endpoint
 * every 200ms for real-time weight updates, and /api/settings endpoint as needed.
 * 
 * This approach ensures:
 * - Better compatibility across all devices and browsers
 * - No connection drops or reconnection issues
 * - Simpler implementation and debugging
 * - Reliable operation in various network environments
 */

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

/**
 * @brief Handle /api/status endpoint - provides real-time system status
 * 
 * Returns JSON with current system state, weights, calibration status,
 * statistics, and user messages. Called every 200ms by web interface.
 */
void handleApiStatus() {
  // Determine appropriate status message based on system state
  String statusMsg = "";
  switch(stateMachine) {
    case 1: statusMsg = step2Text[lang]; break;              // Place glass
    case 2:
    case 3: statusMsg = step3Text[lang]; break;              // Filling
    case 4: statusMsg = (actualWeight < minGlassWeight && autoState == 1) ? step1Text[lang] : 
                        (actualWeight < minGlassWeight) ? step1Text[lang] : step4Text[lang]; break; // Ready/Complete
    default: statusMsg = step1Text[lang]; break;             // Press start
  }
  
  String json = "{\"running\":" + String(stateMachine != 4 ? "true" : "false") + 
                ",\"stateMachine\":" + String(stateMachine) + 
                ",\"autoState\":" + String(autoState ? "true" : "false") + 
                ",\"message\":\"" + statusMsg + 
                "\",\"weights\":{\"total\":" + String(actualWeight) + 
                ",\"honey\":" + String(adjustedWeight) + 
                ",\"glass\":" + String(glasWeight) + "}";
  
  // Add basic statistics information
  float avgError, totalDispensed;
  calculateBasicStats(&avgError, &totalDispensed);
  
  json += ",\"statistics\":{";
  json += "\"totalCycles\":" + String(totalDispensingCycles);
  json += ",\"totalDispensed\":" + String(totalDispensed, 2);
  json += ",\"averageError\":" + String(avgError, 1);
  // Add cumulative persistent statistics
  if (totalDispensingCycles > 0) {
    float cumulativeAvgErr = (float)cumulativeTotalError / totalDispensingCycles;
    json += ",\"cumulativeDispensed\":" + String(cumulativeDispensedGrams);
    json += ",\"cumulativeAverageError\":" + String(cumulativeAvgErr, 1);
  } else {
    json += ",\"cumulativeDispensed\":0,\"cumulativeAverageError\":0";
  }
  
  // Add recent dispensing history for statistics
  json += ",\"recentHistory\":[";
  for (int i = 0; i < historyCount; i++) {
    if (i > 0) json += ",";
    DispensingRecord record = dispensingHistory[i];
    json += "{\"target\":" + String(record.targetWeight, 1);
    json += ",\"actual\":" + String(record.actualWeight, 1);
    json += ",\"error\":" + String(record.error, 1) + "}";
  }
  json += "]";
  json += "}";
  
  // Add calibration info
  String calMsg = "";
  switch(calStateMachine) {
    case 0: 
      // Check if system is actually calibrated
      if (LittleFS.exists("/cal.txt")) {
        calMsg = calStep1Text[lang]; // System is calibrated, ready for re-calibration
      } else {
        calMsg = "System skal kalibreres, tryk Kalibrer for at starte"; // System needs calibration
      }
      break;
    case 1: calMsg = calStep2Text[lang]; break;
    case 2:
    case 4: calMsg = calStep3Text[lang]; break;
    case 3: calMsg = calStep4Text[lang]; break;
  }
  
  json += ",\"calibration\":{\"message\":\"" + calMsg + "\",\"state\":" + String(calStateMachine);
  
  // Add calibration progress information
  if (calAveraging && calStateMachine == 4) {
    // During sampling: progress from 10% to 100% (90% range for sampling)
    int samplingProgress = (calCount * 90) / calSamples;  // 0-90% for sampling
    int totalProgress = 10 + samplingProgress;  // Add 10% from tare operation
    json += ",\"progress\":{";
    json += "\"active\":true";
    json += ",\"percent\":" + String(totalProgress);
    json += ",\"current\":" + String(calCount + 10);  // Add offset for tare step
    json += ",\"total\":" + String(calSamples + 10);
    json += "}";
  } else if (calStateMachine == 2 && tareInProgress) {
    // During tare operation: 0-10% progress
    json += ",\"progress\":{";
    json += "\"active\":true";
    json += ",\"percent\":5";  // Show 5% progress during tare (halfway)
    json += ",\"current\":5";
    json += ",\"total\":110";  // Total includes tare + sampling
    json += "}";
  } else if (calStateMachine == 3) {
    // Waiting for calibration weight: stay at 10% (tare completed)
    json += ",\"progress\":{";
    json += "\"active\":true";
    json += ",\"percent\":10";
    json += ",\"current\":10";
    json += ",\"total\":110";
    json += "}";
  } else {
    json += ",\"progress\":{\"active\":false}";
  }
  
  json += "}";
  
  // Add save message if available
  if (hasSaveMessage) {
    json += ",\"saveStatus\":{\"success\":true,\"message\":\"" + lastSaveMessage + "\"}";
    hasSaveMessage = false;
    lastSaveMessage = "";
  }
  
  json += "}";
  
  server.send(200, "application/json", json);
}

/**
 * @brief Handle /api/settings endpoint - provides current system configuration
 * 
 * Returns JSON with all user-configurable settings including PID parameters,
 * servo limits, system thresholds, and language preferences.
 */
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

/**
 * @brief Handle /api/command endpoint - processes user commands
 * 
 * Accepts POST requests with JSON payload containing command and parameters.
 * Processes system control commands (start, stop, settings changes, etc.)
 */
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
    servoTestMode = true;
    servoTestStartTime = millis(); // Reset timer on each button press
    if (position == "min") {
      servoTestOutput = 0.0;
    } else if (position == "max") {
      servoTestOutput = 1.0;
    }
  }
  else if (command == "calibrate") {
    if (calStateMachine == 0) {
      // Start calibration immediately - go directly to tare step
      calStateMachine = 2;
    } else if (calStateMachine == 1) {
      calStateMachine = 2;
    } else if (calStateMachine == 3) {
      calStateMachine = 4;
    }
  }
  else if (command == "resetStatistics") {
    // Clear basic statistics
    totalDispensingCycles = 0;
    initializeStats();
    cumulativeDispensedGrams = 0;
    cumulativeTotalError = 0;
    cumulativeTargetGrams = 0;
    saveSettings(); // Immediately save reset statistics (critical event)
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
    
    // Initialize file system for persistent storage
    if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
        Serial.println("CRITICAL: LittleFS Mount Failed");
        ESP.restart();
    }
    Serial.println("LittleFS file system mounted successfully");
    
    // Load system configuration and calibration
    readCal(LittleFS, "/cal.txt");   // Load scale calibration factor
    readFile(LittleFS, "/data.txt"); // Load system settings
    initializeStats();               // Initialize basic statistics
    initializeWeightSampling();      // Initialize weight averaging system
    
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
 * Continuously executes the following tasks:
 * 1. Handle network communication (DNS and HTTP requests)
 * 2. Process auto-save with debouncing to protect flash memory
 * 3. Read and process weight measurements
 * 4. Execute calibration state machine
 * 5. Execute main dosing state machine with PID control
 * 6. Control servo position based on PID output
 * 
 * Optimized for 20ms PID control cycles while maintaining responsive
 * network communication and real-time weight updates.
 */
void loop(void) {
  static unsigned long lastLoopTime = 0;
  unsigned long now = millis();
  if (now - lastLoopTime >= looptime) {
    lastLoopTime = now;
    // Handle network tasks
    dnsServer.processNextRequest();
    server.handleClient();

    // Check for auto-save (debounced)
    checkAutoSave();

    actualWeight = getStableWeight(); // Get averaged weight reading for stability
    int16_t temp = (actualWeight-glasWeight);
    adjustedWeight = max((int16_t)0,temp); // Weight from scale minus glass weight, floored to zero

    // Handle calibration state machine
    switch(calStateMachine){
      case 0: // Check if calibration exists
        if (LittleFS.exists("/cal.txt") == 0){ // No calibration data saved
          calStateMachine = 1;
        }
        break;

      case 1: // Wait for calibration button press
        break;

      case 2: // Perform tare operation
          if (!tareInProgress) {
            tareInProgress = true;
            tareStartTime = millis();
          }
          // Simulate tare progress for better user feedback
          if (millis() - tareStartTime > 1000) { // 1 second delay
            scale.set_scale();
            scale.tare();
            calStateMachine = 3;
            tareInProgress = false;
          }
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
        }
        break;
      case 2: // Start PID control system
        setpoint = desiredAmount.toInt(); // Lock in selected target weight
        setpointPI = setpoint/setpoint;
        myController.start();
        stateMachine = 3; // Move to filling state
        break;
      case 3: // Fill glass to target weight
        if(setpoint-adjustedWeight<stopHysteresis){ // When close enough to target (within hysteresis)
          stopConditionCount++; // Increment counter for stable condition
          if(stopConditionCount >= STOP_CONFIRM_CYCLES) {
            // Record basic statistics
            recordDispensingStats();
            myController.stop(); 
            myController.reset();
            output = 0;
            stateMachine = 4; // Move to complete state
            stopConditionCount = 0; // Reset counter
            Serial.println("Dosing completed - target weight reached");
          }
        } else {
          stopConditionCount = 0; // Reset counter if condition not met
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

    // Handle servo test mode with auto-timeout
    if (servoTestMode) {
      if (millis() - servoTestStartTime > 3000) { // 3 second timeout for responsive testing
        servoTestMode = false;
        servoTestOutput = 0.0;
      } else {
        output = servoTestOutput; // Override PID output during test
      }
    }

    // Update servo position based on PID output or test mode
    myservo.write(servoMin + output * (servoMax - servoMin));
  }
}