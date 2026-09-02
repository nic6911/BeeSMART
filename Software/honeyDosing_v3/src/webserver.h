#pragma once

#include <WebSocketsServer.h>
#include <WebServer.h>

String buildStatusMessage();
String buildCalibrationJson();
String buildWeightUpdateJson();
String buildStatusJson();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void setCORSHeaders();
void handleCORS();
void handleApiSettings();
void handleApiCommand();
void handleCaptivePortal();
void initWebServer();
