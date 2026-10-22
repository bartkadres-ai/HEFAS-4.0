#include "hid/hid_transport.h"

#include "USBMouse.h"
#include "ble/ble_control.h"
#include "config.h"
#include "hefas_telemetry.h"

static USBMouse s_usbMouse(false);

bool HidTransport::begin() {
#if BLE_AKTYWNY
    (void)bleControlBegin();
#endif
    return true;
}

void HidTransport::poll() {
#if BLE_AKTYWNY
    bleControlPoll();
#endif
    _usbHid = s_usbMouse.ready();
    statusUsbHidAktywny = _usbHid;
    trybBezprzewodowy = !_usbHid;
    bleRadioWlaczony = bleControlRadioOn();
}

bool HidTransport::bleHidActive() const {
#if BLE_AKTYWNY
    return !_usbHid && bleControlConnected();
#else
    return false;
#endif
}

bool HidTransport::canSend() const {
    return _usbHid || bleHidActive();
}

void HidTransport::registerActivity() {
    _lastActivityMs = millis();
}

void HidTransport::refreshBlePolicy(float motionDegPerS) {
#if !BLE_AKTYWNY
    return;
#else
    if (_usbHid) {
        bleControlSetAdvertising(false, false);
        bleUspionyBezczynnoscia = false;
        return;
    }

    const bool connected = bleControlConnected();
    if (connected) {
        registerActivity();
    }

    unsigned long idleMs = 0;
    if (_lastActivityMs != 0) {
        idleMs = millis() - _lastActivityMs;
    }

    if (!connected && motionDegPerS >= PROG_PROBUDZENIA_BLE_DEG_S) {
        registerActivity();
        idleMs = 0;
    }

    const bool idleSleep = !connected && idleMs >= CZAS_BEZCZYNNOSCI_BLE_MS;
    const bool wantAdv = connected || !idleSleep;
    bleControlSetAdvertising(wantAdv, idleSleep);
    bleUspionyBezczynnoscia = idleSleep;
#endif
}

void HidTransport::move(int8_t dx, int8_t dy, int8_t scroll) {
    if (!canSend()) {
        return;
    }
    registerActivity();
    if (_usbHid) {
        if (scroll != 0) {
            s_usbMouse.scroll(scroll);
        } else {
            s_usbMouse.move(dx, dy);
        }
    } else {
        bleMouseMove(dx, dy, scroll);
    }
}

void HidTransport::press(uint8_t button) {
    if (!canSend()) {
        return;
    }
    registerActivity();
    if (_usbHid) {
        s_usbMouse.press(button);
    } else {
        bleMousePress(button);
    }
}

void HidTransport::release(uint8_t button) {
    if (!canSend()) {
        return;
    }
    registerActivity();
    if (_usbHid) {
        s_usbMouse.release(button);
    } else {
        bleMouseRelease(button);
    }
}
