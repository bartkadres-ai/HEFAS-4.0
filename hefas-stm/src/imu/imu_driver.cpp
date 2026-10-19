#include "imu/imu_driver.h"

#include <Arduino_LSM6DSOX.h>
#include <math.h>

#include "config.h"

bool ImuDriver::begin() {
    if (!IMU.begin()) {
        return false;
    }
    _ready = true;
    return true;
}

void ImuDriver::startCalibration() {
    _calibrating = true;
    _calIndex = 0;
    _sumGx = _sumGy = _sumGz = 0;
}

bool ImuDriver::calibrationStep() {
    if (!_calibrating) {
        return true;
    }

    float ax, ay, az;
    float gx, gy, gz;
    if (!IMU.readGyroscope(gx, gy, gz) || !IMU.readAcceleration(ax, ay, az)) {
        return false;
    }

    // Arduino_LSM6DSOX zwraca °/s — sumujemy bezpośrednio.
    _sumGx += (long)(gx * 1000.0f);
    _sumGy += (long)(gy * 1000.0f);
    _sumGz += (long)(gz * 1000.0f);
    _calIndex++;

    if (_calIndex < CAL_PROBKI_IMU) {
        return false;
    }

    const float n = (float)CAL_PROBKI_IMU;
    _offGx = ((float)_sumGx / 1000.0f) / n;
    _offGy = ((float)_sumGy / 1000.0f) / n;
    _offGz = ((float)_sumGz / 1000.0f) / n;
    _calibrating = false;
    return true;
}

void ImuDriver::update(bool trybScrolla) {
    _dx = 0;
    _dy = 0;
    if (!_ready || _calibrating) {
        return;
    }

    float ax, ay, az;
    float gx, gy, gz;
    if (!IMU.readGyroscope(gx, gy, gz) || !IMU.readAcceleration(ax, ay, az)) {
        return;
    }

    _gx = gx - _offGx;
    _gy = gy - _offGy;
    _gz = gz - _offGz;

    float predkoscOsX = _gx;
    float predkoscOsZ = _gz;

    const float progZ = trybScrolla ? PROG_ZYRO_SKROL_DEG_S : PROG_ZYROSKOPU;
    if (fabsf(predkoscOsX) < PROG_ZYROSKOPU) {
        predkoscOsX = 0.0f;
    }
    if (fabsf(predkoscOsZ) < progZ) {
        predkoscOsZ = 0.0f;
    }

    if (fabsf(predkoscOsX) < STREFA_MARTWA) {
        predkoscOsX = 0.0f;
    }
    if (fabsf(predkoscOsZ) < STREFA_MARTWA) {
        predkoscOsZ = 0.0f;
    }

    const float dX = predkoscOsX * CZULOSC_MYSZY * (float)ODWROC_OS_X;
    const float dY = predkoscOsZ * CZULOSC_MYSZY * (float)ODWROC_OS_Y;

    _dx = constrain((int)dX, -127, 127);
    _dy = constrain((int)dY, -127, 127);
}
