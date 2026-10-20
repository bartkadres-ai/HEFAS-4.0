#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "config.h"

/**
 * Warstwa HID — USB mysz (USBMouse) z opcjonalnym BLE w przyszłości.
 */
class MouseOutput {
public:
    bool begin();
    bool isConnected() const;

    void move(int8_t dx, int8_t dy, int8_t scroll = 0);
    void press(uint8_t button);
    void release(uint8_t button);

    void clickLeft();
    void clickRight();
    void doubleClickLeft();

    /** Maszyna stanów dla kliknięć (kolejka jak w HEFAS 4.0). */
    void queueClick(uint8_t button, bool doubleLeft);
    void tickClickFsm();

private:
    enum class ClickState : uint8_t {
        Idle,
        FirstPress,
        BetweenDouble,
        SecondPress,
    };

    struct ClickJob {
        uint8_t button;
        bool doubleLeft;
    };

    ClickState _state = ClickState::Idle;
    uint8_t _btn = 0;
    bool _doubleLeft = false;
    unsigned long _stateSince = 0;

    ClickJob _queue[HID_KOLEJKA_ROZMIAR];
    uint8_t _qHead = 0;
    uint8_t _qTail = 0;

    bool enqueue(uint8_t button, bool doubleLeft);
    void startClick(uint8_t button, bool doubleLeft);
    void pumpQueue();
};
