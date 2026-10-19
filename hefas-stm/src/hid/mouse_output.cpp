#include "hid/mouse_output.h"

#include "USBMouse.h"

#include "config.h"

static USBMouse s_mouse;

bool MouseOutput::begin() {
#if HID_PREFER_BLE
    // TODO: BLE HID (Murata 1DX) — osobna ścieżka gdy USB niedostępny.
    return true;
#else
    return true;
#endif
}

bool MouseOutput::isConnected() const {
    return true;
}

void MouseOutput::move(int8_t dx, int8_t dy, int8_t scroll) {
    if (scroll != 0) {
        s_mouse.scroll(scroll);
    } else {
        s_mouse.move(dx, dy);
    }
}

void MouseOutput::press(uint8_t button) {
    s_mouse.press(button);
}

void MouseOutput::release(uint8_t button) {
    s_mouse.release(button);
}

void MouseOutput::clickLeft() {
    queueClick(MOUSE_LEFT, false);
}

void MouseOutput::clickRight() {
    queueClick(MOUSE_RIGHT, false);
}

void MouseOutput::doubleClickLeft() {
    queueClick(MOUSE_LEFT, true);
}

bool MouseOutput::enqueue(uint8_t button, bool doubleLeft) {
    const uint8_t next = (uint8_t)((_qHead + 1) % HID_KOLEJKA_ROZMIAR);
    if (next == _qTail) {
        return false;
    }
    _queue[_qHead].button = button;
    _queue[_qHead].doubleLeft = doubleLeft;
    _qHead = next;
    return true;
}

void MouseOutput::startClick(uint8_t button, bool doubleLeft) {
    _btn = button;
    _doubleLeft = doubleLeft;
    press(button);
    _state = ClickState::FirstPress;
    _stateSince = millis();
}

void MouseOutput::queueClick(uint8_t button, bool doubleLeft) {
    if (_state != ClickState::Idle) {
        enqueue(button, doubleLeft);
        return;
    }
    startClick(button, doubleLeft);
}

void MouseOutput::pumpQueue() {
    if (_state != ClickState::Idle || _qHead == _qTail) {
        return;
    }
    const ClickJob job = _queue[_qTail];
    _qTail = (uint8_t)((_qTail + 1) % HID_KOLEJKA_ROZMIAR);
    startClick(job.button, job.doubleLeft);
}

void MouseOutput::tickClickFsm() {
    if (_state == ClickState::Idle) {
        pumpQueue();
        return;
    }

    const unsigned long now = millis();
    switch (_state) {
        case ClickState::FirstPress:
            if (now - _stateSince >= CZAS_KROTKIEGO_KLIKU_MS) {
                release(_btn);
                if (_btn == MOUSE_LEFT && _doubleLeft) {
                    _state = ClickState::BetweenDouble;
                    _stateSince = now;
                } else {
                    _state = ClickState::Idle;
                }
            }
            break;
        case ClickState::BetweenDouble:
            if (now - _stateSince >= CZAS_MIEDZY_KLIKAMI_PODWOJNEGO_MS) {
                press(_btn);
                _state = ClickState::SecondPress;
                _stateSince = now;
            }
            break;
        case ClickState::SecondPress:
            if (now - _stateSince >= CZAS_KROTKIEGO_KLIKU_MS) {
                release(_btn);
                _state = ClickState::Idle;
            }
            break;
        default:
            break;
    }

    if (_state == ClickState::Idle) {
        pumpQueue();
    }
}
