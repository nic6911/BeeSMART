#include "globals.h"
#include "control.h"
#include "filesystem.h"
#include "statistics.h"

void initPID(float lowlim, float highlim) {
    double ki = (Ti[gainSelector] > 0) ? 0.01 / Ti[gainSelector] : 0.0;
    myController.begin(&input, &output, &setpointPI,
                      kP[gainSelector] / 10,
                      ki,
                      kD[gainSelector]);

    myController.setSampleTime(looptime);
    myController.setOutputLimits(lowlim, highlim);
    myController.setWindUpLimits(0, 0.5);

    stopSystem();

    Serial.println("PID initialized with viscosity preset: " + String(gainSelector));
}

void stopSystem() {
    myController.stop();
    myController.reset();
    autoState = 0;
    output = 0;
    stateMachine = 4;
    stopConditionCount = 0;

    Serial.println("SYSTEM STOPPED - Auto-start disabled. Weight: " + String(adjustedWeight) + "g, Target: " + String(setpoint) + "g");
}

void runCalibrationStateMachine() {
    switch (calStateMachine) {
        case 0:
            if (LittleFS.exists("/cal.txt") == 0) {
                calStateMachine = 1;
            }
            break;

        case 1:
            break;

        case 2:
            if (!tareInProgress) {
                tareInProgress = true;
                tareStartTime = millis();
            }
            if (millis() - tareStartTime > 1000) {
                scale.set_scale();
                scale.tare();
                calStateMachine = 3;
                tareInProgress = false;
            }
            break;

        case 3:
            break;

        case 4:
            if (!calAveraging) {
                calSum = 0;
                calCount = 0;
                calAveraging = true;
            }

            calSum += scale.get_units(1);
            calCount++;

            if (calCount >= calSamples) {
                float avgReading = (float)calSum / calSamples;
                float calibFactor = avgReading / calWeight.toInt();
                writeFile(LittleFS, "/cal.txt", String(calibFactor) + ",");
                scale.set_scale(calibFactor);
                initializeWeightSampling();
                calAveraging = false;
                calStateMachine = 0;
            }
            break;

        default:
            break;
    }
}

void runDosingStateMachine() {
    switch (stateMachine) {
        case 1:
            if (actualWeight < minGlassWeight) {
                cnt = 0;
            } else {
                cnt++;
            }
            if (cnt > GLASS_CONFIRM_CYCLES) {
                glasWeight = actualWeight;
                cnt = 0;
                stateMachine = 2;
            }
            break;

        case 2:
            if (actualWeight < minGlassWeight) {
                stateMachine = 4;
                break;
            }
            setpoint = max(1.0, (double)desiredAmount.toInt());
            setpointPI = 1.0;
            myController.start();
            stateMachine = 3;
            break;

        case 3:
            if (actualWeight < minGlassWeight) {
                stopSystem();
                break;
            }
            if (setpoint - adjustedWeight < stopHysteresis) {
                stopConditionCount++;
                if (stopConditionCount >= STOP_CONFIRM_CYCLES) {
                    myController.stop();
                    myController.reset();
                    output = 0;
                    stateMachine = 4;
                    stopConditionCount = 0;
                    spikeDetected = false;
                    preSpikeWeight = 0;
                    recordWeight = adjustedWeight;
                    Serial.println("Dosing completed - target weight reached");
                }
            } else {
                stopConditionCount = 0;
            }
            break;

        case 4:
            myController.stop();
            myController.reset();

            if (setpoint > 0) {
                if (adjustedWeight > recordWeight) {
                    uint16_t delta = adjustedWeight - recordWeight;
                    if (delta > recordWeight / 20) {
                        if (!spikeDetected) {
                            spikeDetected = true;
                            preSpikeWeight = recordWeight;
                        }
                    } else {
                        recordWeight = adjustedWeight;
                    }
                }
            }

            if (actualWeight < minGlassWeight) {
                if (setpoint > 0) {
                    uint16_t finalWeight = spikeDetected ? preSpikeWeight : recordWeight;
                    recordDispensingStats(finalWeight);
                    setpoint = 0;
                }
                if (autoState == 1) {
                    stateMachine = 1;
                    cnt = 0;
                }
            }
            break;

        default:
            break;
    }
}
