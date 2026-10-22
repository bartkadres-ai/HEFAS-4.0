#pragma once

#include <Arduino.h>

class LedStatus {
public:
    void reset();
    void setScrollMode(bool on);
    void setDragActive(bool on);
    void setCalibrating(bool on, bool parallelPhase);
    void setSystemReady(bool ready);
    void pulseNoLink();
    void startBlinkPattern(int count, int halfPeriodMs);
    void tick(bool hidCanSend);

private:
    enum class BlinkPhase : uint8_t { Idle, On, Off };

    bool _scroll = false;
    bool _drag = false;
    bool _calibrating = false;
    bool _calParallel = false;
    bool _systemReady = false;

    unsigned long _pulseUntilMs = 0;
    unsigned long _lastNoLinkPulseMs = 0;

    int _blinkRemaining = 0;
    int _blinkHalfMs = 0;
    BlinkPhase _blinkPhase = BlinkPhase::Idle;
    unsigned long _blinkSinceMs = 0;
};
