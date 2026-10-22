#pragma once

#include <Arduino.h>

#include "config.h"

#if BLE_AKTYWNY

bool bleControlBegin();
void bleControlPoll();
void bleControlSetAdvertising(bool on, bool sleepDueToIdle);
bool bleControlConnected();
bool bleControlRadioOn();

void bleMouseMove(int8_t dx, int8_t dy, int8_t wheel);
void bleMousePress(uint8_t buttons);
void bleMouseRelease(uint8_t buttons);

#else

inline bool bleControlBegin() { return true; }
inline void bleControlPoll() {}
inline void bleControlSetAdvertising(bool, bool) {}
inline bool bleControlConnected() { return false; }
inline bool bleControlRadioOn() { return false; }
inline void bleMouseMove(int8_t, int8_t, int8_t) {}
inline void bleMousePress(uint8_t) {}
inline void bleMouseRelease(uint8_t) {}

#endif
