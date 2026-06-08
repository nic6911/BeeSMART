#include "globals.h"
#include "filesystem.h"

void readFile(fs::FS &fs, const char * path) {
    File file = fs.open(path);
    if (!file || file.isDirectory()) {
        Serial.println("Failed to open settings file: " + String(path));
        return;
    }

    String array = file.readString();
    file.close();

    int indx[25];
    indx[0] = 0;
    for (int i = 1; i <= 24; i++) {
        indx[i] = array.indexOf(',', indx[i-1] + 1);
    }

    for (int set = 0; set < 4; set++) {
        int baseIdx = set * 3;
        kP[set] = array.substring(indx[baseIdx], indx[baseIdx + 1]).toDouble();
        Ti[set] = array.substring(indx[baseIdx + 1] + 1, indx[baseIdx + 2]).toDouble();
        kD[set] = array.substring(indx[baseIdx + 2] + 1, indx[baseIdx + 3]).toDouble();
    }

    desiredAmount = array.substring(indx[12] + 1, indx[13]);
    servoMin = array.substring(indx[13] + 1, indx[14]).toInt();
    servoMax = array.substring(indx[14] + 1, indx[15]).toInt();
    lang = array.substring(indx[15] + 1, indx[16]).toInt();
    stopHysteresis = array.substring(indx[16] + 1, indx[17]).toInt();
    minGlassWeight = array.substring(indx[17] + 1, indx[18]).toInt();
    maxWeight = array.substring(indx[18] + 1, indx[19]).toInt();
    totalDispensingCycles = array.substring(indx[19] + 1, indx[20]).toInt();

    if (indx[20] != -1) {
        if (indx[21] != -1) {
            gainSelector = array.substring(indx[20] + 1, indx[21]).toInt();
        } else {
            gainSelector = array.substring(indx[20] + 1).toInt();
        }
    }

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

    if (minGlassWeight < 0 || minGlassWeight > 500) minGlassWeight = 10;
    if (maxWeight < 50 || maxWeight > maxWeightLim) maxWeight = 1000;
    if (servoMin < 0 || servoMin > 180) servoMin = 0;
    if (servoMax < 0 || servoMax > 180) servoMax = 90;
    if (servoMin >= servoMax) { servoMin = 0; servoMax = 90; }
    if (gainSelector > 3) gainSelector = 2;
    if (lang > 2) lang = 0;
    if (stopHysteresis < 0 || stopHysteresis > 500) stopHysteresis = 5;

    int amt = desiredAmount.toInt();
    if (amt < 10 || amt > maxWeightLim) desiredAmount = String(minWeight + maxWeight / 2);

    Serial.println("Settings loaded successfully from: " + String(path));
}

void readCal(fs::FS &fs, const char * path) {
    File file = fs.open(path);
    if (!file || file.isDirectory()) {
        Serial.println("Calibration file not found: " + String(path));
        return;
    }

    String calArray;
    while (file.available()) {
        calArray = file.readString();
    }
    file.close();

    int calIndx = calArray.indexOf(',');
    if (calIndx > 0) {
        calFactor = calArray.substring(0, calIndx).toFloat();
        Serial.println("Calibration factor loaded: " + String(calFactor));
    }
}

void writeFile(fs::FS &fs, const char * path, String message) {
    File file = fs.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing: " + String(path));
        return;
    }

    file.print(message);
    file.close();
    delay(10);
}

void saveSettings() {
    String pidParams = "";
    for (int i = 0; i < 4; i++) {
        pidParams += String(kP[i]) + "," + String(Ti[i]) + "," + String(kD[i]);
        if (i < 3) pidParams += ",";
    }

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

    writeFile(LittleFS, "/data.txt", settings);

    lastSaveMessage = saveStateText[lang];
    hasSaveMessage = true;
    settingsChanged = false;

    Serial.println("Settings saved to persistent storage");
}

void markSettingsChanged() {
    settingsChanged = true;
    lastSettingChange = millis();
}

void checkAutoSave() {
    if (settingsChanged && (millis() - lastSettingChange >= AUTOSAVE_DELAY)) {
        saveSettings();
        Serial.println("Auto-saved settings after " + String(AUTOSAVE_DELAY / 1000) + " second delay");
    }
}
