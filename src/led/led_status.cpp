#include "led/led_status.h"

#include "config.h"

void LedStatus::reset() {
    _scroll = false;
    _drag = false;
    _calibrating = false;
    _calParallel = false;
    _systemReady = false;
    _pulseUntilMs = 0;
    _lastNoLinkPulseMs = 0;
    _blinkRemaining = 0;
    _blinkPhase = BlinkPhase::Idle;
}

void LedStatus::setScrollMode(bool on) {
    _scroll = on;
}

void LedStatus::setDragActive(bool on) {
    _drag = on;
}

void LedStatus::setCalibrating(bool on, bool parallelPhase) {
    _calibrating = on;
    _calParallel = parallelPhase;
}

void LedStatus::setSystemReady(bool ready) {
    _systemReady = ready;
}

void LedStatus::pulseNoLink() {
    const unsigned long now = millis();
    if (now - _lastNoLinkPulseMs >= OKRES_MIG_LED_BRAK_POL_MS) {
        _lastNoLinkPulseMs = now;
        _pulseUntilMs = now + CZAS_MIG_LED_BRAK_POL_ON_MS;
    }
}

void LedStatus::startBlinkPattern(int count, int halfPeriodMs) {
    if (count <= 0 || halfPeriodMs <= 0) {
        return;
    }
    _blinkRemaining = count;
    _blinkHalfMs = halfPeriodMs;
    _blinkPhase = BlinkPhase::On;
    _blinkSinceMs = millis();
    digitalWrite(LED_BUILTIN, HIGH);
}

void LedStatus::tick(bool hidCanSend) {
    const unsigned long now = millis();

    if (_blinkPhase != BlinkPhase::Idle) {
        if (now - _blinkSinceMs >= (unsigned long)_blinkHalfMs) {
            if (_blinkPhase == BlinkPhase::On) {
                digitalWrite(LED_BUILTIN, LOW);
                _blinkPhase = BlinkPhase::Off;
            } else {
                _blinkRemaining--;
                if (_blinkRemaining <= 0) {
                    _blinkPhase = BlinkPhase::Idle;
                } else {
                    digitalWrite(LED_BUILTIN, HIGH);
                    _blinkPhase = BlinkPhase::On;
                }
            }
            _blinkSinceMs = now;
        }
        return;
    }

    if (_scroll || _drag) {
        digitalWrite(LED_BUILTIN, HIGH);
        return;
    }

    if (_calibrating) {
        digitalWrite(LED_BUILTIN, _calParallel ? HIGH : LOW);
        return;
    }

    if (_systemReady && !hidCanSend) {
        pulseNoLink();
    }

    digitalWrite(LED_BUILTIN, (now < _pulseUntilMs) ? HIGH : LOW);
}
