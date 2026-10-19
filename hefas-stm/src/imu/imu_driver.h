#pragma once

#include <Arduino.h>

/**
 * Odczyt LSM6DSOX i przeliczenie ruchu głowy na delty kursora (rate-control).
 * Logika zgodna z HEFAS 4.0 (osie Gx → X, Gz → Y).
 */
class ImuDriver {
public:
    bool begin();
    bool isReady() const { return _ready; }

    /** Kalibracja offsetów żyroskopu (średnia z N próbek, blokująca). */
    void startCalibration();
    bool calibrationStep();
    bool isCalibrating() const { return _calibrating; }

    /** Wywoływać co OKRES_PETLI_MS po zakończonej kalibracji. */
    void update(bool trybScrolla);

    float gyroXDegPerS() const { return _gx; }
    float gyroYDegPerS() const { return _gy; }
    float gyroZDegPerS() const { return _gz; }

    int deltaX() const { return _dx; }
    int deltaY() const { return _dy; }

private:
    bool _ready = false;
    bool _calibrating = false;
    int _calIndex = 0;
    long _sumGx = 0;
    long _sumGy = 0;
    long _sumGz = 0;

    float _offGx = 0.0f;
    float _offGy = 0.0f;
    float _offGz = 0.0f;

    float _gx = 0.0f;
    float _gy = 0.0f;
    float _gz = 0.0f;
    int _dx = 0;
    int _dy = 0;
};
