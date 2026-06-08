#pragma once

#include <FS.h>

void readFile(fs::FS &fs, const char * path);
void readCal(fs::FS &fs, const char * path);
void writeFile(fs::FS &fs, const char * path, String message);
void saveSettings();
void markSettingsChanged();
void checkAutoSave();
