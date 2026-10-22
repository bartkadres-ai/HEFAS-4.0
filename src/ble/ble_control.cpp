#include "ble/ble_control.h"

#include "config.h"

#if BLE_AKTYWNY

#include <ArduinoBLE.h>

namespace {

BLEService hidService("1812");
BLECharacteristic inputReport("2A4D", BLERead | BLENotify, 4);
BLECharacteristic reportMap("2A4B", BLERead, 52);
BLECharacteristic hidInfo("2A4A", BLERead, 4);
BLECharacteristic protocolMode("2A4E", BLERead | BLEWrite, 1);

const uint8_t kReportMap[] = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29,
    0x03, 0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05,
    0x81, 0x03, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75,
    0x08, 0x95, 0x03, 0x81, 0x06, 0xC0, 0xC0,
};

uint8_t g_buttons = 0;
bool g_stackReady = false;
bool g_advertising = false;
bool g_sleepIdle = false;

void sendReport(int8_t x, int8_t y, int8_t wheel) {
    if (!bleControlConnected()) {
        return;
    }
    uint8_t rep[4] = {g_buttons, (uint8_t)x, (uint8_t)y, (uint8_t)wheel};
    inputReport.writeValue(rep, sizeof(rep));
}

}  // namespace

bool bleControlBegin() {
    if (!BLE.begin()) {
        return false;
    }

    BLE.setLocalName(BLE_NAZWA_URZADZENIA);
    BLE.setDeviceName(BLE_NAZWA_URZADZENIA);

    reportMap.writeValue(kReportMap, sizeof(kReportMap));
    const uint8_t info[4] = {0x11, 0x01, 0x00, 0x02};
    hidInfo.writeValue(info, sizeof(info));
    protocolMode.writeValue((uint8_t)1);

    hidService.addCharacteristic(reportMap);
    hidService.addCharacteristic(hidInfo);
    hidService.addCharacteristic(protocolMode);
    hidService.addCharacteristic(inputReport);
    BLE.addService(hidService);

    BLE.setAdvertisedService(hidService);
    g_stackReady = true;
    return true;
}

void bleControlSetAdvertising(bool on, bool sleepDueToIdle) {
    if (!g_stackReady) {
        return;
    }
    g_sleepIdle = sleepDueToIdle;
    if (on) {
        if (!g_advertising) {
            BLE.advertise();
            g_advertising = true;
        }
    } else if (g_advertising) {
        BLE.stopAdvertise();
        g_advertising = false;
    }
}

bool bleControlRadioOn() {
    return g_advertising;
}

bool bleControlConnected() {
    BLEDevice central = BLE.central();
    return central && central.connected();
}

void bleControlPoll() {
    BLE.poll();
}

void bleMouseMove(int8_t dx, int8_t dy, int8_t wheel) {
    sendReport(dx, dy, wheel);
}

void bleMousePress(uint8_t buttons) {
    g_buttons |= buttons;
    sendReport(0, 0, 0);
}

void bleMouseRelease(uint8_t buttons) {
    g_buttons &= (uint8_t)~buttons;
    sendReport(0, 0, 0);
}

#endif
