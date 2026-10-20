#include "click_logic/blink_fsm.h"

#include "config.h"

void BlinkFsm::reset() {
    _eyeConfirmedClosed = false;
    _closedSamples = 0;
    _openSamples = 0;
    _dragActive = false;
    _impulseCount = 0;
    _seriesActive = false;
    _pending = {};
}

bool BlinkFsm::blocksCursorGesture() const {
    return _dragActive || _eyeConfirmedClosed;
}

bool BlinkFsm::pollAction(BlinkAction& out) {
    if (_pending.type == BlinkAction::Type::None) {
        return false;
    }
    out = _pending;
    _pending.type = BlinkAction::Type::None;
    return true;
}

unsigned long BlinkFsm::silenceAfterCount(uint8_t n) {
    switch (n) {
        case 1: return SERIA_CISZA_PO_1_MS;
        case 2: return SERIA_CISZA_PO_2_MS;
        case 3: return SERIA_CISZA_PO_3_MS;
        case 4: return SERIA_CISZA_PO_4_MS;
        case 5: return SERIA_CISZA_PO_5_MS;
        default: return SERIA_CISZA_PO_6_MS;
    }
}

bool BlinkFsm::seriesTimeValid(uint8_t count, unsigned long firstMs, unsigned long lastMs) {
    if (count == 0 || firstMs == 0) {
        return true;
    }
    const unsigned long span = lastMs - firstMs;
    if (count == 3) {
        return span >= SERIA_CZAS_MIN_3_MS && span <= SERIA_CZAS_MAX_3_MS;
    }
    if (count == 4) {
        return span <= SERIA_CZAS_MAX_4_MS;
    }
    if (count == 5) {
        return span <= SERIA_CZAS_MAX_5_MS;
    }
    if (count >= 6) {
        return span <= SERIA_CZAS_MAX_6_MS;
    }
    return true;
}

void BlinkFsm::registerImpulse(unsigned long now) {
    if (_impulseCount > 0) {
        _lastGapMs = now - _lastImpulseMs;
    }

    if (_impulseCount > 0 && (now - _lastImpulseMs) <= OKNO_MIEDZY_IMPULSAMI_MS) {
        if (_impulseCount < MAX_MRUGNIEC_W_SERII) {
            _impulseCount++;
        }
    } else {
        _impulseCount = 1;
        _firstImpulseMs = now;
        _lastGapMs = 0;
    }

    _lastImpulseMs = now;
    _seriesActive = true;
    if (CZAS_REFRAKTORY_PO_IMPULSIE_MS > 0) {
        _refractoryUntilMs = now + CZAS_REFRAKTORY_PO_IMPULSIE_MS;
    }
    _seriesDeadlineMs = now + silenceAfterCount(_impulseCount);
}

void BlinkFsm::onEyeOpened(unsigned long now) {
    if (_dragActive) {
        _pending.type = BlinkAction::Type::DragRelease;
        _dragActive = false;
        return;
    }

    const unsigned long duration = now - _closedSinceMs;
    if (duration < CZAS_MIN_MRUG_MS) {
        return;
    }
    if (duration >= PROG_PRZYTRZYMANIA_MS) {
        return;
    }
    if (duration > CZAS_MAX_MRUG_MS) {
        return;
    }

    registerImpulse(now);
}

void BlinkFsm::dispatchImpulses(uint8_t count, bool trybScrolla, bool& trybScrollaOut) {
    if (count > MAX_MRUGNIEC_W_SERII) {
        count = MAX_MRUGNIEC_W_SERII;
    }

    if (count == 6) {
        _pending.type = BlinkAction::Type::ToggleDebug;
        return;
    }
    if (count == 5) {
        _pending.type = BlinkAction::Type::Recalibrate;
        return;
    }

    if (trybScrolla && count >= 2) {
        trybScrollaOut = false;
        _pending.type = BlinkAction::Type::ScrollOff;
        if (count == 2) {
            return;
        }
    }

    if (count == 4 && !trybScrolla) {
        trybScrollaOut = true;
        _pending.type = BlinkAction::Type::ScrollOn;
        return;
    }

    if (trybScrolla) {
        return;
    }

    if (count == 3) {
        _pending.type = BlinkAction::Type::RightClick;
    } else if (count == 2) {
        _pending.type = BlinkAction::Type::DoubleLeftClick;
    } else if (count == 1) {
        _pending.type = BlinkAction::Type::LeftClick;
    }
}

void BlinkFsm::finishSeries(unsigned long now, bool trybScrolla, bool& trybScrollaOut) {
    if (!_seriesActive || _impulseCount == 0) {
        return;
    }
    if (_eyeConfirmedClosed) {
        return;
    }
    if (_seriesDeadlineMs == 0 || now < _seriesDeadlineMs) {
        return;
    }

    const uint8_t n = _impulseCount;
    if (!seriesTimeValid(n, _firstImpulseMs, _lastImpulseMs)) {
        _impulseCount = 0;
        _seriesActive = false;
        _seriesDeadlineMs = 0;
        _firstImpulseMs = 0;
        return;
    }

    _impulseCount = 0;
    _seriesActive = false;
    _seriesDeadlineMs = 0;
    _firstImpulseMs = 0;

    dispatchImpulses(n, trybScrolla, trybScrollaOut);
}

void BlinkFsm::tick(bool eyeClosedSample, bool trybScrolla, bool& trybScrollaOut) {
    const unsigned long now = millis();

    if (eyeClosedSample) {
        _closedSamples++;
        _openSamples = 0;
    } else {
        _openSamples++;
        _closedSamples = 0;
    }

    if (!_eyeConfirmedClosed && _closedSamples >= PROBKI_POTWIERDZENIA_STANU) {
        if (CZAS_REFRAKTORY_PO_IMPULSIE_MS > 0 && now < _refractoryUntilMs) {
            _closedSamples = 0;
        } else {
            _eyeConfirmedClosed = true;
            _closedSinceMs = now;
            _openSamples = 0;
        }
    } else if (_eyeConfirmedClosed) {
        const uint8_t needOpen = _dragActive ? PROBKI_POTWIERDZENIA_DRAG_OFF : PROBKI_POTWIERDZENIA_STANU;
        if (_openSamples >= needOpen) {
            onEyeOpened(now);
            _eyeConfirmedClosed = false;
            _closedSamples = 0;
        } else if (!_dragActive && !trybScrolla) {
            if ((now - _closedSinceMs) >= PROG_PRZYTRZYMANIA_MS) {
                _dragActive = true;
                _pending.type = BlinkAction::Type::DragPress;
                _impulseCount = 0;
                _seriesActive = false;
                _seriesDeadlineMs = 0;
                _firstImpulseMs = 0;
            }
        }
    }

    finishSeries(now, trybScrolla, trybScrollaOut);
}
