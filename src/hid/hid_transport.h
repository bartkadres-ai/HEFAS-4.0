#pragma once

#include <Arduino.h>
#include <stdint.h>

class HidTransport {
public:
    bool begin();
    void poll();

    bool usbHidActive() const { return _usbHid; }
    bool bleHidActive() const;
    bool canSend() const;

    void move(int8_t dx, int8_t dy, int8_t scroll = 0);
    void press(uint8_t button);
    void release(uint8_t button);

    void registerActivity();
    void refreshBlePolicy(float motionDegPerS);

private:
    bool _usbHid = false;
    unsigned long _lastActivityMs = 0;
};
