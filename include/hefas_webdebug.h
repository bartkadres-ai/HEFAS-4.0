/**
 * HEFAS – Modul diagnostyczny WiFi (WebDebug)
 *
 * Maski logow (webLogMask): WEBLOG_GYRO, IR, HID, FSM, BAT.
 * Domyslnie: WEBLOG_FSM | WEBLOG_BAT (Mrugniecia + Zasilanie w panelu).
 */

#ifndef HEFAS_WEBDEBUG_H
#define HEFAS_WEBDEBUG_H

#include "hefas_config.h"
#include <Arduino.h>

enum WebLogKategoria : uint8_t {
    WEBLOG_GYRO = 1,
    WEBLOG_IR   = 2,
    WEBLOG_HID  = 4,
    WEBLOG_FSM  = 8,
    WEBLOG_BAT  = 16,
};

#define WEBLOG_DOMYSLNA_MASKA  (WEBLOG_FSM | WEBLOG_BAT)

#if WEBDEBUG_AKTYWNY

void webDebugInit();
void webDebugLoop();
void webDebugLogKategoria(WebLogKategoria kat, const char* wiadomosc);
void webDebugLogKategoria(WebLogKategoria kat, const String& wiadomosc);

inline void webDebugLog(const char* wiadomosc) {
    webDebugLogKategoria(WEBLOG_FSM, wiadomosc);
}
inline void webDebugLog(const String& wiadomosc) {
    webDebugLogKategoria(WEBLOG_FSM, wiadomosc);
}

extern bool webPauzaMyszy;
extern bool webZadanieRekalibracji;
extern uint8_t webLogMask;

#else

inline void webDebugInit() {}
inline void webDebugLoop() {}
inline void webDebugLogKategoria(WebLogKategoria, const char*) {}
inline void webDebugLogKategoria(WebLogKategoria, const String&) {}
inline void webDebugLog(const char*) {}
inline void webDebugLog(const String&) {}

#endif
#endif // HEFAS_WEBDEBUG_H
