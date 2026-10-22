#pragma once

#include <Arduino.h>

/** Zmienne współdzielone z panelem WebDebug (jak w HEFAS 4.0 main.cpp). */
extern bool trybScrolla;
extern bool przytrzymanieAktywne;
extern int kursorDeltaX;
extern int kursorDeltaY;
extern uint32_t licznikKlikLewych;
extern uint32_t licznikKlikPrawych;

extern float tcrtBaseline;
extern float tcrtFiltered;
extern float tcrtFast;
extern int tcrtRaw;
extern bool wirtualnyStanCzujnika;
extern bool okoPotwierdzoneZamkniete;
extern bool trybBezprzewodowy;
extern bool statusUsbHidAktywny;
extern bool statusUsbKabelAktywny;
extern bool statusOgniwoMontowane;
extern bool statusLadowanie;
extern bool bleRadioWlaczony;
extern bool bleUspionyBezczynnoscia;

extern bool czyDebugWlaczony;
extern bool trwaKalibracja;

extern uint8_t licznikImpulsow;
extern unsigned long czasOstatniegoImpulsu;
extern unsigned long czasPierwszegoImpulsuSerii;
extern unsigned long ostatniaPrzerwaMiedzyImpulsamiMs;
extern unsigned long deadlineKoniecSeriiMs;

bool hefasUsbHidConnected();
void hefasSyncTelemetryFromDrivers();
