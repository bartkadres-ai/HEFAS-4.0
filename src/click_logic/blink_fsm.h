#pragma once

#include <Arduino.h>
#include <stdint.h>

struct BlinkAction {
    enum class Type : uint8_t {
        None = 0,
        LeftClick,
        DoubleLeftClick,
        RightClick,
        ScrollOn,
        ScrollOff,
        Recalibrate,
        ToggleDebug,
        DragPress,
        DragRelease,
    };

    Type type = Type::None;
};

/**
 * Maszyna stanów mrugnięć i mapowanie serii impulsów (jak HEFAS 4.0).
 * Wejście: okresowe próbki „oko zamknięte” (po debounce).
 */
class BlinkFsm {
public:
    void reset();

    /** Wywołaj co OKRES_PETLI_MS z aktualnym stanem oka (po potwierdzeniu próbek). */
    void tick(bool eyeClosedSample, bool trybScrolla, bool& trybScrollaOut);

    /** Pobierz akcję HID do wykonania (jeśli jest). */
    bool pollAction(BlinkAction& out);

    bool dragActive() const { return _dragActive; }
    bool blocksCursorGesture() const;

private:
    bool _eyeConfirmedClosed = false;
    uint8_t _closedSamples = 0;
    uint8_t _openSamples = 0;
    unsigned long _closedSinceMs = 0;
    bool _dragActive = false;

    uint8_t _impulseCount = 0;
    bool _seriesActive = false;
    unsigned long _lastImpulseMs = 0;
    unsigned long _seriesDeadlineMs = 0;
    unsigned long _firstImpulseMs = 0;
    unsigned long _refractoryUntilMs = 0;
    unsigned long _lastGapMs = 0;

    BlinkAction _pending = {};

    void onEyeOpened(unsigned long now);
    void registerImpulse(unsigned long now);
    void finishSeries(unsigned long now, bool trybScrolla, bool& trybScrollaOut);
    void dispatchImpulses(uint8_t count, bool trybScrolla, bool& trybScrollaOut);

    static unsigned long silenceAfterCount(uint8_t n);
    static bool seriesTimeValid(uint8_t count, unsigned long firstMs, unsigned long lastMs);
};
