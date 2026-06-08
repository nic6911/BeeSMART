#pragma once

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduPID.h>
#include <ESP32Servo.h>
#include "HX711.h"
#include "FS.h"
#include <LittleFS.h>

#define FORMAT_LITTLEFS_IF_FAILED true

const int LOADCELL_DOUT_PIN = 3;
const int LOADCELL_SCK_PIN = 1;
const int SERVO_PIN = 5;

const byte DNS_PORT = 53;
const IPAddress apIP(192, 168, 4, 1);

const uint8_t STOP_CONFIRM_CYCLES = 3;
const uint8_t GLASS_CONFIRM_CYCLES = 10;
const uint16_t STATS_SAVE_INTERVAL = 20;
const uint8_t WEIGHT_SAMPLES = 5;
const unsigned long AUTOSAVE_DELAY = 5000;

const uint16_t maxWeightLim = 20000;

struct DispensingRecord {
  float targetWeight;
  float actualWeight;
  float error;
  unsigned long timestamp;
};
