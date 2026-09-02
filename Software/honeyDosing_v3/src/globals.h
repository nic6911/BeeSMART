#pragma once

#include "config.h"

extern HX711 scale;
extern Servo myservo;
extern DNSServer dnsServer;
extern WebServer server;
extern WebSocketsServer webSocket;
extern ArduPID myController;

extern double input;
extern double output;
extern double setpoint;
extern double setpointPI;

extern double kP[4];
extern double Ti[4];
extern double kD[4];

extern int16_t actualWeight;
extern uint16_t adjustedWeight;
extern uint16_t glasWeight;

extern uint8_t stateMachine;
extern uint8_t calStateMachine;
extern uint8_t stopConditionCount;

extern int16_t weightSamples[5];
extern uint8_t sampleIndex;
extern bool samplesInitialized;
extern int32_t weightSum;

extern uint8_t gainSelector;

extern uint32_t cumulativeDispensedGrams;
extern int32_t  cumulativeTotalError;
extern uint32_t cumulativeTargetGrams;

extern bool autoState;
extern int looptime;

extern DispensingRecord dispensingHistory[10];
extern int historyIndex;
extern int historyCount;
extern int totalDispensingCycles;

extern uint16_t minWeight;
extern uint16_t maxWeight;
extern int stopHysteresis;
extern int minGlassWeight;

extern int servoMin;
extern int servoMax;

extern int lang;

extern unsigned long lastSettingChange;
extern bool settingsChanged;

extern String lastSaveMessage;
extern bool hasSaveMessage;

extern float calFactor;
extern long calSum;
extern int calCount;
extern const int calSamples;
extern bool calAveraging;
extern String calWeight;
extern uint8_t cnt;

extern bool spikeDetected;
extern uint16_t preSpikeWeight;
extern uint16_t recordWeight;

extern bool servoTestMode;
extern float servoTestOutput;
extern unsigned long servoTestStartTime;

extern bool tareInProgress;
extern unsigned long tareStartTime;

extern String settings;
extern String desiredAmount;

extern String langString[3];
extern String saveStateText[3];
extern String step1Text[3];
extern String step2Text[3];
extern String step3Text[3];
extern String step4Text[3];
extern String calStep1Text[3];
extern String calStep2Text[3];
extern String calStep3Text[3];
extern String calStep4Text[3];
extern String stopText[3];
