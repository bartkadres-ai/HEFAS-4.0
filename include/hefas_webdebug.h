/**
 * HEFAS – Modul diagnostyczny WiFi (WebDebug)
 *
 * Stawia Access Point i serwuje strone z logami na 192.168.4.1.
 * Calosc jest opcjonalna – sterowana flaga WEBDEBUG_AKTYWNY
 * w hefas_config.h. Gdy wylaczona, wszystkie funkcje kompiluja
 * sie do pustych inline i nie wplywaja na reszte kodu.
 */

#ifndef HEFAS_WEBDEBUG_H
#define HEFAS_WEBDEBUG_H

#include "hefas_config.h"

#if WEBDEBUG_AKTYWNY

void webDebugInit();
void webDebugLoop();
void webDebugLog(const char* wiadomosc);
void webDebugLog(const String& wiadomosc);

extern bool webPauzaMyszy;
extern bool webZadanieRekalibracji;

#else

inline void webDebugInit() {}
inline void webDebugLoop() {}
inline void webDebugLog(const char*) {}
inline void webDebugLog(const String&) {}

#endif
#endif // HEFAS_WEBDEBUG_H
