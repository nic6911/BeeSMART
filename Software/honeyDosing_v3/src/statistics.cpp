#include "globals.h"
#include "statistics.h"
#include "filesystem.h"

void recordDispensingStats(uint16_t finalWeight) {
    if (setpoint > 0) {
        DispensingRecord newRecord;
        newRecord.targetWeight = setpoint;
        newRecord.actualWeight = finalWeight;
        newRecord.error = finalWeight - setpoint;
        newRecord.timestamp = millis();

        dispensingHistory[historyIndex] = newRecord;
        historyIndex = (historyIndex + 1) % 10;

        if (historyCount < 10) {
            historyCount++;
        }

        totalDispensingCycles++;

        cumulativeDispensedGrams += (uint32_t)finalWeight;
        cumulativeTargetGrams   += (uint32_t)setpoint;
        cumulativeTotalError    += (int32_t)(finalWeight - setpoint);

        if (totalDispensingCycles % STATS_SAVE_INTERVAL == 0) {
            markSettingsChanged();
        }
    }
}

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
    *totalDispensed = *totalDispensed / 1000.0;
}

void initializeStats() {
    historyCount = 0;
    historyIndex = 0;

    for (int i = 0; i < 10; i++) {
        dispensingHistory[i] = {0, 0, 0, 0};
    }
}

void initializeWeightSampling() {
    sampleIndex = 0;
    samplesInitialized = false;
    weightSum = 0;

    for (int i = 0; i < WEIGHT_SAMPLES; i++) {
        weightSamples[i] = 0;
    }

    Serial.println("Weight sampling system initialized with " + String(WEIGHT_SAMPLES) + " sample rolling average");
}

int16_t getStableWeight() {
    int16_t newReading = scale.get_units(1);

    if (samplesInitialized) {
        weightSum -= weightSamples[sampleIndex];
    }

    weightSamples[sampleIndex] = newReading;
    weightSum += newReading;

    sampleIndex = (sampleIndex + 1) % WEIGHT_SAMPLES;

    if (!samplesInitialized && sampleIndex == 0) {
        samplesInitialized = true;
        Serial.println("Weight sampling buffer filled - stable readings available");
    }

    if (samplesInitialized) {
        return weightSum / WEIGHT_SAMPLES;
    } else {
        return newReading;
    }
}
