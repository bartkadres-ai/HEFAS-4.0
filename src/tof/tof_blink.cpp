#include "tof/tof_blink.h"

#include <Wire.h>
#include <algorithm>

bool TofBlink::begin() {
    Wire1.begin();
    Wire1.setClock(400000);
    _sensor.setBus(&Wire1);

    if (!_sensor.init()) {
        return false;
    }

    _sensor.setDistanceMode(VL53L1X::Short);
    _sensor.setMeasurementTimingBudget(20000);
    _sensor.startContinuous(50);
    _ready = true;
    return true;
}

bool TofBlink::readDistanceMm(int& outMm) {
    if (!_ready) {
        return false;
    }
    const uint16_t reading = _sensor.read(false);
    if (reading == 0 || reading >= TOF_BRAK_POMIARU_MM) {
        return false;
    }
    outMm = (int)reading;
    return outMm >= TOF_ODCZYT_MIN_MM && outMm <= TOF_ODCZYT_MAX_MM;
}

float TofBlink::trimmedMean(const int* samples, int count) {
    int copy[CAL_PROBKI_ROWNOLEGLE];
    for (int i = 0; i < count; i++) {
        copy[i] = samples[i];
    }
    std::sort(copy, copy + count);
    const int reject = count / 10;
    const int used = count - 2 * reject;
    long sum = 0;
    for (int i = reject; i < count - reject; i++) {
        sum += copy[i];
    }
    return (float)sum / (float)used;
}

void TofBlink::startCalibration() {
    _calPhase = CalPhase::Parallel;
    _calIndex = 0;
    _virtualClosed = false;
}

bool TofBlink::calibrationSampleParallel() {
    if (_calPhase == CalPhase::Idle || _calPhase == CalPhase::Stabilize) {
        return true;
    }
    if (_calPhase != CalPhase::Parallel) {
        return true;
    }

    int mm = 0;
    if (!readDistanceMm(mm)) {
        return false;
    }

    _calSamples[_calIndex++] = mm;
    if (_calIndex < CAL_PROBKI_ROWNOLEGLE) {
        return false;
    }

    _baseline = trimmedMean(_calSamples, CAL_PROBKI_ROWNOLEGLE);
    _filtered = _baseline;
    _fast = _baseline;
    _calPhase = CalPhase::Stabilize;
    _calIndex = 0;
    return false;
}

bool TofBlink::calibrationStabilizeStep() {
    if (_calPhase != CalPhase::Stabilize) {
        return true;
    }

    int mm = 0;
    if (!readDistanceMm(mm)) {
        return false;
    }

    _filtered = EMA_ALPHA_WYSWIETLANIA * (float)mm + (1.0f - EMA_ALPHA_WYSWIETLANIA) * _filtered;
    _fast = EMA_ALPHA_SZYBKI * (float)mm + (1.0f - EMA_ALPHA_SZYBKI) * _fast;
    _calIndex++;

    if (_calIndex < CAL_PROBKI_STABILIZACJI) {
        return false;
    }

    _baseline = _filtered;
    _calPhase = CalPhase::Idle;
    return true;
}

bool TofBlink::update() {
    if (!_ready || _calPhase != CalPhase::Idle) {
        return false;
    }

    int mm = 0;
    if (!readDistanceMm(mm)) {
        return false;
    }

    _rawMm = mm;
    _fast = EMA_ALPHA_SZYBKI * (float)mm + (1.0f - EMA_ALPHA_SZYBKI) * _fast;
    _filtered = EMA_ALPHA_WYSWIETLANIA * (float)mm + (1.0f - EMA_ALPHA_WYSWIETLANIA) * _filtered;

    if (!_virtualClosed) {
        if (_fast < (_baseline - TOF_OFFSET_TRIGGER_MM) &&
            _fast > (_baseline - TOF_OFFSET_BLISKIE_MM)) {
            _virtualClosed = true;
        }
    } else {
        if (_fast > (_baseline - TOF_OFFSET_RELEASE_MM)) {
            _virtualClosed = false;
        } else if (_fast < (_baseline - TOF_OFFSET_BLISKIE_MM)) {
            _virtualClosed = false;
        }
    }

    if (!_freezeBaseline && !_virtualClosed) {
        _baseline = EMA_ALPHA_WOLNY * _fast + (1.0f - EMA_ALPHA_WOLNY) * _baseline;
    }

    return true;
}
