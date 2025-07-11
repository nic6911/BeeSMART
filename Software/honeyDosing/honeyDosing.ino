
/**
 * @file honeyDosing.ino
 * @brief ESP32-based honey dosing system with web UI, PID control, servo actuation, and scale calibration.
 *
 * This project implements a smart honey dosing system using an ESP32 microcontroller.
 * It features a web-based user interface (ESPUI) for configuration and operation, PID control for precise dosing,
 * servo motor control for dispensing, and an HX711-based load cell for weight measurement.
 * The system supports multiple languages, persistent settings storage via LittleFS, and calibration routines.
 *
 * Main Features:
 * - Web UI for starting/stopping dosing, setting desired amount, PID parameters, servo limits, and calibration.
 * - PID controller for accurate dispensing based on weight feedback.
 * - Servo motor control for opening/closing the dispensing valve.
 * - HX711 load cell integration for weight measurement and calibration.
 * - Persistent settings and calibration data storage using LittleFS.
 * - Multi-language support (Danish, German, English).
 * - Automatic and manual operation modes.
 * - Glass detection and tare functionality.
 * - Advanced settings for fine-tuning system behavior.
 *
 * Key Components:
 * - ESPUI: Web-based user interface library.
 * - ArduPID: PID control library.
 * - ESP32Servo: Servo motor control library.
 * - HX711: Load cell interface library.
 * - LittleFS: File system for persistent storage.
 * - DNSServer/WiFi: For ESP32 access point and captive portal functionality.
 *
 * Usage:
 * - Configure system parameters via the web UI.
 * - Start dosing process manually or automatically.
 * - Calibrate the scale using known weights.
 * - Adjust PID and servo settings for optimal performance.
 *
 * Author: Mogens Groth Nicolaisen
 * Version: 2.0.0
 * Date: 2025-07-11
 */
#include <DNSServer.h>
#include <ESPUI.h>
#include "ArduPID.h"
#include <ESP32Servo.h>
#include "HX711.h"
#include "FS.h"
#include <LittleFS.h>
#include <WiFi.h>

#define FORMAT_LITTLEFS_IF_FAILED true

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 3; 
const int LOADCELL_SCK_PIN = 1;
HX711 scale;

// WiFi
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;

// PID controller
ArduPID myController;
double input;
double output;
double setpoint;
double setpointPI;
double kP[4] = {8,8,8,10};
double Ti[4] = {10,10,7,0};
double kD[4] = {5,5,2,1};

// Servo
Servo myservo;
const int SERVO_PIN = 5;

// Globals
long reading;
uint8_t gainSelector = 0;
bool autoState = 0;
uint16_t minWeight = 50;
uint16_t maxWeight = 1000;
uint16_t maxWeightLim = 20000;
int stopHysteresis = 5;//we close x grams before Setpoint
int stopHysteresisId;
int minGlassWeightId;
int amountInputId;
int amountInputId2;
int selectorId;
int maxWeightLimId;
int kpId;
int kiId;
int kdId;
int servoMinId;
int servoMaxId;
int saveStateId;
int saveStateId2;
int startId;
int desiredamountInputId;
int infoTextId;
int infoTextCalId;
int switchId;
int honeyweight;
int tareweight;
int actCalWeight;
int servoPosition;
int servoMin = 90;
int servoMax = 90;
int looptime = 20;// PID looptime
int minGlassWeight = 10;//10g minimum weight of empty glass
int lang = 0;
int calibrateId;
int calInputId;
int calInputId2;
int16_t actualWeight = 0;
uint16_t adjustedWeight = 0;
uint16_t glasWeight = 0;
uint8_t cnt = 0;
float calFactor = 0;
long calSum = 0;
int calCount = 0;
const int calSamples = 100;
bool calAveraging = false;
String desiredAmount = String(minWeight+maxWeight/2);
String calWeight = "250";
String settings = "1,0,0,300,1,180,0,5,10,1000";//P,I,D,weight,servoMin,servoMax,lang,hysteresis,glasweight,maxweight
String array;
String saveState = " ";
String infoText = " ";
String infoTextCal = " ";
String saveText[3] = {"Gem", "Speichern", "Save"};
String langString[3] = {"DA", "DE", "EN"};
String saveTextHeading[3] = {"Gem indstillinger", "Einstellungen speichern", "Save settings"};
String saveStateText[3] = {"Indstillinger gemt", "Einstellungen gespeichert", "Settings saved"};
String step1Text[3] = {"Tryk start","Start drücken","Press start"};
String step2Text[3] = {"Placer glas på vægt","Glas auf die Wage platzieren","Place glass on scale"};
String step3Text[3] = {"Fylder glas...","Glas füllen...","Filling glass..."};
String step4Text[3] = {"Glas fyldt - fjern glas", "Glas gefüllt - Glas entfernen", "Glass filled - remove glass"};
String tab1Text[3] = {"Tapning", "Abfüllung", "Filling"};
String tab2Text[3] = {"Indstillinger", "Einstellungen", "Settings"};
String tab3Text[3] = {"Avancerede indstillinger", "Erweiterte Einstellungen", "Advanced settings"};
String tab4Text[3] = {"Kalibrering vægt", "Kalibrierung Wage", "Calibration Scale"};
String autoText[3] = {"Automatisk start af tapning","Automatischer Start des Abfüllens", "Automatic start of filling"};
String SPText[3] = {"Ønsket Mængde","Gewünschte Menge","Desired quantity"};
String PVText[3] = {"Aktuel Honning Vægt (uden glas):","Aktuelles Honig Gewicht (ohne Glas):","Current Honey Weight (ex. glass):"};
String tareText[3] = {"Aktuel Vægt:","Aktuelles Gewicht:","Current Weight:"};
String servoText[3] = {"Indstil min og max Servo position","Min und max Servoposition einstellen","Set min and max servo position"};
String servoText2[3] = {"Gå til servo position","Gehe zur Servoposition","Go to servo position"};
String tap3Heading[3] = {"Kontrol Parametre","Regelparameter", "Control Parameters"};
String langHeading[3] = {"Sprog","Sprache","Language"};
String stopText[3] = {"Stop aktiveret !","Stop aktiviert !", "Stop activated !"};
String prestopTextHeading[3] = {"Luk hanen når der mangler [g]","Abfüllen abbrechen wenn fehlt [g]", "Close tap when missing [g]"};
String minGlassWeightTextHeading[3] = {"Glasregistrering: Glas vejer mere end [g]","Glasregistrierung: Glas wiegt mehr als [g]", "Glass registration: Glass weighs more than [g]"};
String maxWeightTextHeading[3] = {"Max tappemængde [g]","Maximaler Abfüll Menge [g]","Maximum fill quantity [g]"};
String tareTextHeading[3] = {"Tare kun med tom vægt !","Tare nur mit Leergewicht verwenden !","Tare with empty scale only !"};
String viscosityHeading[3] = {"Viskositet","Viskosität","Viscosity"};
String viscosityLow[3] = {"Lav","Niedrig","Low"};
String viscosityMedium[3] = {"Medium","Mittel","Medium"};
String viscosityHigh[3] = {"Høj","Hoch","High"};
String viscosityCustom[3] = {"Bruderdefineret","Benutzerdefiniert","Custom"};
String calSelectorText[3] = {"Kalibreringsvægt","Kalibriergewicht","Calibration weight"};
String calButtonText[3] = {"Kalibrer","Kalibrieren","Calibrate"};
String calStep1Text[3] = {"System kalibreret, tryk Kalibrer for at starte re-kalibrering","System kalibriert, für Neukalibrierung drücken Sie Kalibrieren","System calibrated, press Calibrate to start re-calibration"};
String calStep2Text[3] = {"Tøm vægt og tryk Kalibrer","Gewicht entfernen und Kalibrieren drücken","Remove any weight from scale and press Calibrate"};
String calStep3Text[3] = {"Kalibrerer...","Kalibriert...","Calibrating..."};
String calStep4Text[3] = {"Placer kalibreringsvægt og tryk Kalibrer","Kalibriergewicht auflegen und Kalibrieren drücken","Place calibration weight on scale and press calibrate"};

int indx[11];
uint8_t stateMachine = 4;
uint8_t calStateMachine = 0;

// Hardcoded readfile for settings reading
void readFile(fs::FS &fs, const char * path){
    File file = fs.open(path);
    if(!file || file.isDirectory()){
        return;
    }
    while(file.available()){
      array = file.readString();
    }
    file.close();
    // process data
    indx[0] = 0;
    for (int i = 1; i <= 10; i++) {
      indx[i] = array.indexOf(',',indx[i-1]+1);
    }
    kP[0] = array.substring(0,indx[1]).toDouble();
    Ti[0] = array.substring(indx[1]+1,indx[2]).toDouble();
    kD[0] = array.substring(indx[2]+1,indx[3]).toDouble();
    desiredAmount = array.substring(indx[3]+1,indx[4]);
    servoMin = array.substring(indx[4]+1,indx[5]).toInt();
    servoMax = array.substring(indx[5]+1,indx[6]).toInt();
    lang = array.substring(indx[6]+1,indx[7]).toInt();
    stopHysteresis = array.substring(indx[7]+1,indx[8]).toInt();
    minGlassWeight = array.substring(indx[8]+1,indx[9]).toInt();
    maxWeight = array.substring(indx[9]+1,indx[10]).toInt();
}

// Initialize PID controller with limits
void initPID(float lowlim, float highlim)
{
	//PID setup
  myController.begin(&input, &output, &setpointPI, kP[gainSelector]/10, 0.01/Ti[gainSelector], kD[gainSelector]);
  myController.setSampleTime(looptime);//how often will we run the PID? 50ms
  myController.setOutputLimits(lowlim, highlim);//servomin+myController.output*(servomax-servomin)
  myController.setWindUpLimits(0, 0.5);// Groth bounds for the integral term to prevent integral wind-up
  stopSystem();
}

// Stop the dosing system
void stopSystem()
{
  myController.stop(); 
  myController.reset();
  ESPUI.updateSwitcher(switchId, false);
  autoState = 0;
  output = 0;
  ESPUI.setElementStyle(startId, "background-color: #999999;");
  infoText = stopText[lang]; 
  stateMachine = 4;
}

// Read calibration factor from file
void readCal(fs::FS &fs, const char * path){
    File file = fs.open(path);
    if(!file || file.isDirectory()){
        return;
    }
    while(file.available()){
      array = file.readString();
    }
    file.close();
    indx[0] = 0;
    indx[1] = array.indexOf(',',indx[0]+1);
    calFactor = array.substring(0,indx[1]).toFloat();
}

// Write to file
void writeFile(fs::FS &fs, const char * path, String message){
    File file = fs.open(path, FILE_WRITE);
    if (!file) { // failed to open the file, return false
      return;
    }
    file.print(message);
    delay(100);
    saveState = saveStateText[lang];
    ESPUI.print(saveStateId, saveState);
    ESPUI.print(saveStateId2, saveState);
    file.close();
}

// Save settings to file
void saveSettings(){
  settings = String(kP[0])+","+String(Ti[0])+","+String(kD[0])+","+desiredAmount+","+String(servoMin)+","+String(servoMax)+","+String(lang)+","+String(stopHysteresis)+","+String(minGlassWeight)+","+String(maxWeight);
  writeFile(LittleFS, "/data.txt", settings);
}

// Callback for number inputs
void numberCall(Control* sender, int type)
{
    if(sender->label == "PID Kp")
    {
      kP[gainSelector] = sender->value.toDouble();
      saveState = " ";
      ESPUI.print(saveStateId, saveState);
      ESPUI.print(saveStateId2, saveState);
    }
    if(sender->label == "PID Ti")
    { 
      Ti[gainSelector] = sender->value.toDouble();
      saveState = " ";
      ESPUI.print(saveStateId, saveState);
      ESPUI.print(saveStateId2, saveState);
    }
    if(sender->label == "PID Kd")
    {
      kD[gainSelector] = sender->value.toDouble();
      saveState = " ";
      ESPUI.print(saveStateId, saveState);
      ESPUI.print(saveStateId2, saveState);
    }
    else if(sender->label == "Ønsket Mængde")
    {
      float temp = sender->value.toFloat();
      desiredAmount = String(temp, 0);
      ESPUI.updateNumber(amountInputId2, temp);
      saveState = " ";
      ESPUI.print(saveStateId, saveState);
      ESPUI.print(saveStateId2, saveState);
	    ESPUI.print(desiredamountInputId, desiredAmount+" g");
    }   
    else if(sender->label == "Ønsket Mængde2")
    {
      float temp = sender->value.toFloat();
      if (temp>maxWeight)
      {
        temp=maxWeight;
      }
      if (temp < minWeight)
      {
        temp = minWeight;
      }
      desiredAmount = String(temp, 0);
      ESPUI.updateSlider(amountInputId, temp);
      ESPUI.updateNumber(amountInputId2, temp);
      saveState = " ";
      ESPUI.print(saveStateId, saveState);
      ESPUI.print(saveStateId2, saveState);
	    ESPUI.print(desiredamountInputId, desiredAmount+" g");
    }   
    else if(sender->label == "Calibration Weight")
    {
      float temp = sender->value.toFloat();
      calWeight = String(temp, 0);
      ESPUI.updateNumber(calInputId2, temp);
    }   
    else if(sender->label == "Calibration Weight2")
    {
      float temp = sender->value.toFloat();
      if (temp>maxWeight)
      {
        temp=maxWeight;
      }
      if (temp < minWeight)
      {
        temp = minWeight;
      }
      calWeight = String(temp, 0);
      ESPUI.updateSlider(calInputId, temp);
      ESPUI.updateNumber(calInputId2, temp);
    }  
    else if(sender->label == "maxweightlim")
    {
      float temp = sender->value.toFloat();
      if (temp>maxWeightLim)
      {
        temp=maxWeightLim;
      }
      if (temp < minWeight)
      {
        temp = minWeight;
      }
      maxWeight = temp;
      ESPUI.updateNumber(maxWeightLimId, temp);
      saveState = " ";
      ESPUI.print(saveStateId, saveState);
      ESPUI.print(saveStateId2, saveState);
      saveSettings();
      ESP.restart();
    }          
    else if(sender->label == "Servo Minimum")
    {
      servoMin = sender->value.toInt();
      saveState = " ";
      ESPUI.print(saveStateId, saveState);
      ESPUI.print(saveStateId2, saveState);
    }   
    else if(sender->label == "Servo Maximum")
    {
      servoMax = sender->value.toInt();
      saveState = " ";
      ESPUI.print(saveStateId, saveState);
      ESPUI.print(saveStateId2, saveState);
    }   
    else if(sender->label == "prestop")
    {
      stopHysteresis = sender->value.toInt();
      if (stopHysteresis>maxWeight/2)
      {
        stopHysteresis=maxWeight/2;
      }
      if (stopHysteresis < 0)
      {
        stopHysteresis = 0;
      }
      
      ESPUI.updateNumber(stopHysteresisId, stopHysteresis);
      saveState = " ";
      ESPUI.print(saveStateId, saveState);
      ESPUI.print(saveStateId2, saveState);
    }   
    else if(sender->label == "glasweight")
    {
      minGlassWeight = sender->value.toInt();
      if (minGlassWeight>minWeight)
      {
        minGlassWeight=minWeight;
      }
      if (minGlassWeight < 0)
      {
        minGlassWeight = 0;
      }
      
      ESPUI.updateNumber(minGlassWeightId, minGlassWeight);
      saveState = " ";
      ESPUI.print(saveStateId, saveState);
      ESPUI.print(saveStateId2, saveState);
    }       
}

// Callback for selector input
void selectorCallback(Control* sender, int type)
{
  if(sender->value == "Brugerdefineret")
  {
    gainSelector = 0; 
    initPID(0.1, 1);
  }
  else if(sender->value == "Lav")
  {
    gainSelector = 1;
    initPID(0.1, 0.75);
  }
  else if(sender->value == "Medium")
  {
    gainSelector = 2;
    initPID(0.15, 1);
  }  
  else if(sender->value == "Høj")
  {
    gainSelector = 3;
    initPID(0.25, 1);
  }  
  ESPUI.updateNumber(kpId, kP[gainSelector]);
  ESPUI.updateNumber(kiId, Ti[gainSelector]);
  ESPUI.updateNumber(kdId, kD[gainSelector]);  
}

// Callback for button presses
void buttonCallback(Control* sender, int type)
{
    switch (type)
    {
    case B_DOWN:
        if(sender->label == "Start" && stateMachine == 4){
          stateMachine = 1;
          ESPUI.setElementStyle(startId, "background-color: #9BC268;");
        }
        else if(sender->label == "Tare"){
          scale.tare();	
        }
        else if(sender->label == "Gem indstillinger"){
          //construct settings string
          saveSettings();
          initPID(0.1, 1);
        }
        else if(sender->label == "Gå til servo minimum"){
          output = 0;
        }
        else if(sender->label == "Gå til servo maximum"){
          output = 1;
        }
        else if(sender->label == "Stop"){
          stopSystem();
        }
        else if(sender->label == "Calibrate" && calStateMachine == 0){
          calStateMachine = 1;
        }
        else if(sender->label == "Calibrate" && calStateMachine == 1){
          calStateMachine = 2;
        }
        else if(sender->label == "Calibrate" && calStateMachine == 3){
          calStateMachine = 4;
        }
        break;

    case B_UP:
        break;
    }
}

// Callback for language selection
void selectLanguage(Control* sender, int value)
{
  if(sender->value == "DA"){
    lang = 0;
  }
  else if(sender->value == "DE"){
    lang = 1;
  }
  else if(sender->value == "EN"){
    lang = 2;
  }
  saveSettings();
  ESP.restart();
}

// Callback for switch input
void switchCallback(Control* sender, int value)
{
    switch (value)
    {
    case S_ACTIVE:
      autoState = 1;
        break;

    case S_INACTIVE:
      autoState = 0;
        break;
    }
}

void setup(void)
{
  //init file system and load settings
  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
      ESP.restart();
  }
  readCal(LittleFS, "/cal.txt"); // Read cal factor for weight
  readFile(LittleFS, "/data.txt"); // Read the contents of the file
  
  //setup scale
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calFactor); // this value is obtained by calibrating the scale with known weights
      
	//Servo setup
	myservo.attach(SERVO_PIN);//
	
	//PID setup
  initPID(0.1, 1);
	
	//WiFi setup
  ESPUI.setVerbosity(Verbosity::VerboseJSON);
	WiFi.mode(WIFI_AP);
	delay(100);
	WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
	uint32_t chipid = 0;
	for (int i = 0; i < 17; i = i + 8)
	{
	  chipid |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
	}
	char ap_ssid[25];
	snprintf(ap_ssid, 26, "BeeSMART-%08X", chipid);
	WiFi.softAP(ap_ssid);
		  
  dnsServer.start(DNS_PORT, "*", apIP);

  // --------------  Initialize ESPUI  ----------------------//
  // Tabs definition
  uint16_t tab1 = ESPUI.addControl(Tab, "Tapning", tab1Text[lang]);
  uint16_t tab2 = ESPUI.addControl(Tab, "Indstillinger", tab2Text[lang]);
  uint16_t tab3 = ESPUI.addControl(Tab, "Avancerede indstillinger", tab3Text[lang]);
  uint16_t tab4 = ESPUI.addControl(Tab, "Kalibrering", tab4Text[lang]);

  // Styling for labels
  String switcherLabelStyle = "width: 60px; margin-left: .3rem; margin-right: .3rem; background-color: unset;";
  String clearLabelStyle = "background-color: unset; width: 100%;";
  String lineLabelStyle = "background-color: LightGrey; width: 100%;";

  // Page 1 - Start dosing & show weight of container and actual weight
  int tapLabel = ESPUI.addControl(Label, "", tab1Text[lang], None, tab1);
  ESPUI.setElementStyle(tapLabel, clearLabelStyle);

  // Start button
  startId = ESPUI.addControl(Button, "Start", "Start", Dark, tapLabel, &buttonCallback);
  ESPUI.addControl(Button, "Stop", "Stop", Dark, tapLabel, &buttonCallback);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, tapLabel), clearLabelStyle);

  // Auto start of process switch
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", autoText[lang], None, tapLabel), clearLabelStyle);
  switchId = ESPUI.addControl(Switcher, "Auto", "", Dark, tapLabel, &switchCallback);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, tapLabel), clearLabelStyle);

  // Display desired amount selected on the settings tab
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", SPText[lang]+":", None, tapLabel), clearLabelStyle);
  desiredamountInputId = ESPUI.addControl(Label, "Ønsket Mængde:", desiredAmount+" g", Dark, tapLabel);
  ESPUI.setElementStyle(desiredamountInputId, clearLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, tapLabel), clearLabelStyle);

  // Display actual scale feedback
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", PVText[lang], None, tapLabel), clearLabelStyle);
  honeyweight = ESPUI.addControl(Label, "Aktuel Vægt:", "0 g", Dark, tapLabel);
  ESPUI.setElementStyle(honeyweight, clearLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, tapLabel), clearLabelStyle);

  // Display infoText
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, tapLabel), clearLabelStyle);
  infoTextId = ESPUI.addControl(Label, " ", " ", Dark, tapLabel);
  ESPUI.setElementStyle(infoTextId, "background-color: Grey; width: 50%;");
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, tapLabel), clearLabelStyle);    

  // Page 2 - Settings
  int setLabel = ESPUI.addControl(Label, "", "", None, tab2);
  ESPUI.setElementStyle(setLabel, clearLabelStyle);

  // Desired amount headline label
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", SPText[lang]+" [g]", None, setLabel), clearLabelStyle);

  // Desired amount slider
  amountInputId = ESPUI.addControl(Slider, "Ønsket Mængde", desiredAmount, Dark, setLabel, &numberCall);
  ESPUI.sliderContinuous = true;    
  ESPUI.addControl(Min, "Min", String(minWeight), None, amountInputId);
  ESPUI.addControl(Max, "Max", String(maxWeight), None, amountInputId);

  amountInputId2 = ESPUI.addControl(Number, "Ønsket Mængde2", String(desiredAmount.toFloat(), 0), Dark, setLabel, &numberCall); 
  ESPUI.addControl(Min, "Min", String(minWeight), None, amountInputId2);
  ESPUI.addControl(Max, "Max", String(maxWeight), None, amountInputId2);
  ESPUI.setElementStyle(amountInputId2, "width: 30%;color: #000000;");
  

  // PID presets - viscosity selection
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, setLabel), clearLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, setLabel), lineLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "Selector", viscosityHeading[lang], None, setLabel), clearLabelStyle);
  //These are the values for the selector's options. (Note that they *must* be declared static
  //so that the storage is allocated in global memory and not just on the stack of this function.)
  static String optionValues[] {viscosityCustom[lang], viscosityLow[lang], viscosityMedium[lang], viscosityHigh[lang]};
  selectorId = ESPUI.addControl(Select, "Selector", "Selector", Dark, setLabel, &selectorCallback);
  for(auto const& v : optionValues) {
    ESPUI.addControl(Option, v.c_str(), v, None, selectorId);
  }   

  // PID parameter number inputs
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", tap3Heading[lang], None, setLabel), clearLabelStyle);
  ESPUI.setElementStyle(setLabel, clearLabelStyle);
  kpId = ESPUI.addControl(Number, "PID Kp", String(kP[gainSelector]), Dark, setLabel, &numberCall);
  ESPUI.addControl(Min, "Min", String(0), None, kpId);
  ESPUI.addControl(Max, "Max", String(50), None, kpId);
  ESPUI.setElementStyle(kpId, "width: 30%;color: #000000;");
  kiId = ESPUI.addControl(Number, "PID Ti", String(Ti[gainSelector]), Dark, setLabel, &numberCall);
  ESPUI.addControl(Min, "Min", String(0), None, kiId);
  ESPUI.addControl(Max, "Max", String(50), None, kiId);
  ESPUI.setElementStyle(kiId, "width: 30%;color: #000000;");
  kdId = ESPUI.addControl(Number, "PID Kd", String(kD[gainSelector]), Dark, setLabel, &numberCall);
  ESPUI.addControl(Min, "Min", String(0), None, kdId);
  ESPUI.addControl(Max, "Max", String(50), None, kdId);
  ESPUI.setElementStyle(kdId, "width: 30%;color: #000000;");
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, setLabel), clearLabelStyle);

  // PID labels
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "Kp", None, setLabel), switcherLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "Ti", None, setLabel), switcherLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "Kd", None, setLabel), switcherLabelStyle);

  //Tare
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, setLabel), clearLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, setLabel), lineLabelStyle);    
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", tareText[lang], None, setLabel), clearLabelStyle);
  tareweight = ESPUI.addControl(Label, "Aktuel Vægt:", "0 g", Dark, setLabel);
  ESPUI.setElementStyle(tareweight, clearLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", tareTextHeading[lang], None, setLabel), clearLabelStyle);
  ESPUI.addControl(Button, "Tare", "Tare", Dark, setLabel, &buttonCallback);

  // Language selector
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, setLabel), clearLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, setLabel), lineLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "Language", langHeading[lang], None, setLabel), clearLabelStyle);
  uint16_t languageSelect = ESPUI.addControl( Select, "Select Language", langString[lang], Dark, setLabel, &selectLanguage );
  ESPUI.addControl( Option, "Dansk", "DA", Dark, languageSelect);
  ESPUI.addControl( Option, "Deutsch", "DE", Dark, languageSelect);
  ESPUI.addControl( Option, "English", "EN", Dark, languageSelect);

  // Save settings
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, setLabel), clearLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, setLabel), lineLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "Gem indstillinger", saveTextHeading[lang], None, setLabel), clearLabelStyle);
  ESPUI.addControl(Button, "Gem indstillinger", saveText[lang], Dark, setLabel, &buttonCallback);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, setLabel), clearLabelStyle);
  saveStateId = ESPUI.addControl(Label, "", saveState, Dark, setLabel);
  ESPUI.setElementStyle(saveStateId, clearLabelStyle);

  // Page 3 - Advanced settings
  int pidLabel = ESPUI.addControl(Label, "", "", None, tab3);
  ESPUI.setElementStyle(pidLabel, clearLabelStyle);

  // Servo settings headline label
  ESPUI.setElementStyle(ESPUI.addControl(Label, "Servo opsætning", servoText[lang], None, pidLabel), clearLabelStyle);
  ESPUI.setElementStyle(pidLabel, clearLabelStyle);
  
  // Servo min position selector
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, pidLabel), clearLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "Servo Minimum", "Servo Minimum", None, pidLabel), clearLabelStyle);
  servoMinId = ESPUI.addControl(Slider, "Servo Minimum", String(servoMin), Dark, pidLabel, &numberCall);
  ESPUI.sliderContinuous = true;    
  ESPUI.addControl(Min, "Min", "1", None, servoMinId);
  ESPUI.addControl(Max, "Max", "90", None, servoMinId);

  // Servo max position selector
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, pidLabel), clearLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "Servo Maximum", "Servo Maximum", None, pidLabel), clearLabelStyle);    
  servoMaxId = ESPUI.addControl(Slider, "Servo Maximum", String(servoMax), Dark, pidLabel, &numberCall);
  ESPUI.sliderContinuous = true;     
  ESPUI.addControl(Min, "Min", "90", None, servoMaxId);
  ESPUI.addControl(Max, "Max", "180", None, servoMaxId);

  // Move to servo min/max
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, pidLabel), clearLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "Gå til servo position", servoText2[lang], None, pidLabel), clearLabelStyle);  
  ESPUI.addControl(Button, "Gå til servo minimum", "Minimum", Dark, pidLabel, &buttonCallback);
  ESPUI.addControl(Button, "Gå til servo maximum", "Maximum", Dark, pidLabel, &buttonCallback);

  // Pre-stop weight
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, pidLabel), clearLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, pidLabel), lineLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "Stop før", prestopTextHeading[lang], None, pidLabel), clearLabelStyle);
  stopHysteresisId = ESPUI.addControl(Number, "prestop", String(stopHysteresis), Dark, pidLabel, &numberCall); 
  ESPUI.addControl(Min, "Min", String(0), None, stopHysteresisId);
  ESPUI.addControl(Max, "Max", String(maxWeight/2), None, stopHysteresisId);
  ESPUI.setElementStyle(stopHysteresisId, "width: 30%;color: #000000;");

  // Glass weight
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, pidLabel), clearLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, pidLabel), lineLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "Glas vægt", minGlassWeightTextHeading[lang], None, pidLabel), clearLabelStyle);
  minGlassWeightId = ESPUI.addControl(Number, "glasweight", String(minGlassWeight), Dark, pidLabel, &numberCall); 
  ESPUI.addControl(Min, "Min", String(5), None, minGlassWeightId);
  ESPUI.addControl(Max, "Max", String(minWeight), None, minGlassWeightId);
  ESPUI.setElementStyle(minGlassWeightId, "width: 30%;color: #000000;");

  // Max weight
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, pidLabel), clearLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, pidLabel), lineLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "Max vægt", maxWeightTextHeading[lang], None, pidLabel), clearLabelStyle);
  maxWeightLimId = ESPUI.addControl(Number, "maxweightlim", String(maxWeight), Dark, pidLabel, &numberCall); 
  ESPUI.addControl(Min, "Min", String(minWeight), None, maxWeightLimId);
  ESPUI.addControl(Max, "Max", String(maxWeightLim), None, maxWeightLimId);
  ESPUI.setElementStyle(maxWeightLimId, "width: 30%;color: #000000;");

  // Save settings
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, pidLabel), clearLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, pidLabel), lineLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "Gem indstillinger", saveTextHeading[lang], None, pidLabel), clearLabelStyle);
  ESPUI.addControl(Button, "Gem indstillinger", saveText[lang], Dark, pidLabel, &buttonCallback);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, pidLabel), clearLabelStyle);
  saveStateId2 = ESPUI.addControl(Label, "", saveState, Dark, pidLabel);
  ESPUI.setElementStyle(saveStateId2, clearLabelStyle);


  // Page 4 - Calibration
  int calLabel = ESPUI.addControl(Label, "", "", None, tab4);
  ESPUI.setElementStyle(calLabel, clearLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", tareText[lang], None, calLabel), clearLabelStyle);    
  actCalWeight = ESPUI.addControl(Label, "Aktuel Vægt:", "0 g", Dark, calLabel);
  ESPUI.setElementStyle(actCalWeight, clearLabelStyle);
  // Display infoText
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, calLabel), clearLabelStyle);
  infoTextCalId = ESPUI.addControl(Label, " ", " ", Dark, calLabel);
  ESPUI.setElementStyle(infoTextCalId, "background-color: Grey; width: 50%;");
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, calLabel), clearLabelStyle);    

  // calibration weight slider
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, calLabel), clearLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, calLabel), lineLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", calSelectorText[lang], None, calLabel), clearLabelStyle);            
  calInputId = ESPUI.addControl(Slider, "Calibration Weight", calWeight, Dark, calLabel, &numberCall);
  ESPUI.sliderContinuous = true;    
  ESPUI.addControl(Min, "Min", String(minWeight), None, calInputId);
  ESPUI.addControl(Max, "Max", String(maxWeight), None, calInputId);

  calInputId2 = ESPUI.addControl(Number, "Calibration Weight2", String(calWeight.toFloat(), 0), Dark, calLabel, &numberCall); 
  ESPUI.addControl(Min, "Min", String(minWeight), None, calInputId2);
  ESPUI.addControl(Max, "Max", String(maxWeight), None, calInputId2);
  ESPUI.setElementStyle(calInputId2, "width: 30%;color: #000000;");

  // Calibrate button
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, calLabel), clearLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, calLabel), lineLabelStyle);
  calibrateId = ESPUI.addControl(Button, "Calibrate", calButtonText[lang], Dark, calLabel, &buttonCallback);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, calLabel), clearLabelStyle);

  ESPUI.begin("BeeSMART 2.0.0");
  myController.stop(); 
  scale.tare();	// reset the scale to 0
}

void loop(void)
{
  actualWeight = scale.get_units();//weight from scale
  int16_t temp = (actualWeight-glasWeight);
  adjustedWeight = max((int16_t)0,temp);//weight from scale minus glas weight floored to zero

  switch(calStateMachine){
    case 0: //
    infoTextCal = calStep1Text[lang];
    ESPUI.print(infoTextCalId, infoTextCal);
    if (LittleFS.exists("/cal.txt") == 0){//there is no calibration data saved
      calStateMachine = 1;
    }
      break;

    case 1:// calibration when calibration button is pushed 
      infoTextCal = calStep2Text[lang];
      ESPUI.print(infoTextCalId, infoTextCal);
      break;

    case 2:// now we tare 
        infoTextCal = calStep3Text[lang];
        ESPUI.print(infoTextCalId, infoTextCal);
        scale.set_scale();
        scale.tare();
        calStateMachine = 3;
      break;
    
    case 3://wait for button press
      infoTextCal = calStep4Text[lang];
      ESPUI.print(infoTextCalId, infoTextCal);
      break;

    case 4:
      infoTextCal = calStep3Text[lang];
      ESPUI.print(infoTextCalId, infoTextCal);

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

  switch (stateMachine){
    case 1:// calibrateGlass when start is pushed 
      if(actualWeight < minGlassWeight){// more than 25g on the weight - must be a glass
        cnt = 0;
      }
      else{
        cnt++;
      }
      if(cnt > 10){// glass confirmed
        glasWeight = actualWeight;// record glas weight
        cnt = 0;
        stateMachine = 2;
      }
      else{
        infoText = step2Text[lang];
      }
      break;
    case 2:// startPID   
      infoText = step3Text[lang]; 
      setpoint = desiredAmount.toInt();//Lås valgt indstilling
      setpointPI = setpoint/setpoint;
      myController.start();
      stateMachine = 3;//waitForSettle
      break;
    case 3://fyld glas til indstillet vægt
      if(setpoint-adjustedWeight<stopHysteresis){//when we are close to zero gram error
        myController.stop(); 
        myController.reset();
        output = 0;
        infoText = step4Text[lang]; 
        ESPUI.setElementStyle(startId, "background-color: #999999;");
        stateMachine = 4;//
      }
      break;
      case 4:
        myController.stop(); 
        myController.reset();
        if(actualWeight < minGlassWeight){//if glass is removed again
          infoText = step1Text[lang];
          if(autoState == 1){
            stateMachine = 1;//we dont use the startbutton at all
            cnt = 0;
          }
        }
      break;
    default:
      break;
  }

    dnsServer.processNextRequest();
    input = adjustedWeight/setpoint;// (SP-PV)/SP -> input=weight/SP
    myController.compute();
    myservo.write(servoMin+output*(servoMax-servoMin));//servo position
    static long oldTime = 0;
    if (millis() - oldTime > 100)
    {
      ESPUI.print(infoTextId, infoText);
      ESPUI.print(honeyweight, String(adjustedWeight)+" g");
      ESPUI.print(tareweight, String(actualWeight)+" g");
      ESPUI.print(actCalWeight, String(actualWeight)+" g");
      oldTime = millis();
      saveState = saveStateText[lang];
    }
}
