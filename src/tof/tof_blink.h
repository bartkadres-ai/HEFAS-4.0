#pragma once

#include <Arduino.h>
#include <VL53L1X.h>

#include "config.h"

/**
 * Detekcja mrugnięć przez VL53L1X (ToF na Nicla Vision).
 * Logika histerezy i baseline jak TCRT5000 w HEFAS 4.0.
 */
class TofBlink {
public:
    bool begin();

    void startCalibration();
    /** Jedna próbka w fazie równoległej IMU+ToF; false = jeszcze trwa. */
    bool calibrationSampleParallel();
    /** Faza stabilizacji baseline (~1 s); false = jeszcze trwa. */
    bool calibrationStabilizeStep();
    bool isCalibrating() const { return _calPhase != CalPhase::Idle; }
    bool isStabilizePhase() const { return _calPhase == CalPhase::Stabilize; }

    void setFreezeBaseline(bool freeze) { _freezeBaseline = freeze; }

    /** Odczyt i aktualizacja progów; wywoływać okresowo w pętli. */
    bool update();

    bool eyeClosedRaw() const { return _virtualClosed; }
    int rawMm() const { return _rawMm; }
    float baselineMm() const { return _baseline; }
    float filteredMm() const { return _filtered; }
    float fastMm() const { return _fast; }

private:
    enum class CalPhase : uint8_t { Idle, Parallel, Stabilize };

    VL53L1X _sensor;
    bool _ready = false;
    CalPhase _calPhase = CalPhase::Idle;
    int _calIndex = 0;
    int _calSamples[CAL_PROBKI_ROWNOLEGLE];

    int _rawMm = 0;
    float _baseline = 0.0f;
    float _filtered = 0.0f;
    float _fast = 0.0f;
    bool _virtualClosed = false;
    bool _freezeBaseline = false;

    bool readDistanceMm(int& outMm);
    static float trimmedMean(const int* samples, int count);
};
