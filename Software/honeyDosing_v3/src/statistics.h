#pragma once

#include <stdint.h>

void recordDispensingStats(uint16_t finalWeight);
void calculateBasicStats(float* avgError, float* totalDispensed);
void initializeStats();
void initializeWeightSampling();
int16_t getStableWeight();
