#include "globals.h"

HX711 scale;
Servo myservo;
DNSServer dnsServer;
WebServer server(80);
WebSocketsServer webSocket(81);
ArduPID myController;

double input;
double output;
double setpoint;
double setpointPI = 1.0;

double kP[4] = {8, 8, 8, 10};
double Ti[4] = {10, 10, 7, 0};
double kD[4] = {5, 5, 2, 1};

int16_t actualWeight = 0;
uint16_t adjustedWeight = 0;
uint16_t glasWeight = 0;

uint8_t stateMachine = 4;
uint8_t calStateMachine = 0;
uint8_t stopConditionCount = 0;

int16_t weightSamples[5];
uint8_t sampleIndex = 0;
bool samplesInitialized = false;
int32_t weightSum = 0;

uint8_t gainSelector = 2;

uint32_t cumulativeDispensedGrams = 0;
int32_t  cumulativeTotalError    = 0;
uint32_t cumulativeTargetGrams   = 0;

bool autoState = 0;
int looptime = 20;

DispensingRecord dispensingHistory[10];
int historyIndex = 0;
int historyCount = 0;
int totalDispensingCycles = 0;

uint16_t minWeight = 50;
uint16_t maxWeight = 1000;
int stopHysteresis = 2;
int minGlassWeight = 10;

int servoMin = 0;
int servoMax = 90;

int lang = 0;

unsigned long lastSettingChange = 0;
bool settingsChanged = false;

String lastSaveMessage = "";
bool hasSaveMessage = false;

float calFactor = 0;
long calSum = 0;
int calCount = 0;
const int calSamples = 100;
bool calAveraging = false;
String calWeight = "250";
uint8_t cnt = 0;

bool spikeDetected = false;
uint16_t preSpikeWeight = 0;
uint16_t recordWeight = 0;

bool servoTestMode = false;
float servoTestOutput = 0.0;
unsigned long servoTestStartTime = 0;

bool tareInProgress = false;
unsigned long tareStartTime = 0;

String settings = "8,10,5,8,10,5,8,7,2,10,0,1,300,1,180,0,5,10,1000,0,0";
String desiredAmount = "300";

String langString[3] = {"DA", "DE", "EN"};
String saveStateText[3] = {
    "Indstillinger gemt",
    "Einstellungen gespeichert",
    "Settings saved"
};
String step1Text[3] = {
    "Tryk start",
    "Start dr\u00FCcken",
    "Press start"
};
String step2Text[3] = {
    "Placer glas p\u00E5 v\u00E6gt",
    "Glas auf die Wage platzieren",
    "Place glass on scale"
};
String step3Text[3] = {
    "Fylder glas...",
    "Glas f\u00FCllen...",
    "Filling glass..."
};
String step4Text[3] = {
    "Glas fyldt - fjern glas",
    "Glas gef\u00FCllt - Glas entfernen",
    "Glass filled - remove glass"
};
String calStep1Text[3] = {
    "System kalibreret, tryk Kalibrer for at starte re-kalibrering",
    "System kalibriert, f\u00FCr Neukalibrierung dr\u00FCcken Sie Kalibrieren",
    "System calibrated, press Calibrate to start re-calibration"
};
String calStep2Text[3] = {
    "T\u00F8m v\u00E6gt og tryk Kalibrer",
    "Gewicht entfernen und Kalibrieren dr\u00FCcken",
    "Remove any weight from scale and press Calibrate"
};
String calStep3Text[3] = {
    "Kalibrerer...",
    "Kalibriert...",
    "Calibrating..."
};
String calStep4Text[3] = {
    "Placer kalibreringsv\u00E6gt og tryk Kalibrer",
    "Kalibriergewicht auflegen und Kalibrieren dr\u00FCcken",
    "Place calibration weight on scale and press calibrate"
};
String stopText[3] = {
    "Stop aktiveret!",
    "Stop aktiviert!",
    "Stop activated!"
};
