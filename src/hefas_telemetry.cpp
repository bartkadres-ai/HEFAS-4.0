#include "hefas_telemetry.h"

bool trybScrolla = false;
bool przytrzymanieAktywne = false;
int kursorDeltaX = 0;
int kursorDeltaY = 0;
uint32_t licznikKlikLewych = 0;
uint32_t licznikKlikPrawych = 0;

float tcrtBaseline = 0.0f;
float tcrtFiltered = 0.0f;
float tcrtFast = 0.0f;
int tcrtRaw = 0;
bool wirtualnyStanCzujnika = false;
bool okoPotwierdzoneZamkniete = false;
bool trybBezprzewodowy = true;
bool statusUsbHidAktywny = false;
bool statusUsbKabelAktywny = false;
bool statusOgniwoMontowane = true;
bool statusLadowanie = false;
bool bleRadioWlaczony = false;
bool bleUspionyBezczynnoscia = false;

bool czyDebugWlaczony = false;
bool trwaKalibracja = false;

uint8_t licznikImpulsow = 0;
unsigned long czasOstatniegoImpulsu = 0;
unsigned long czasPierwszegoImpulsuSerii = 0;
unsigned long ostatniaPrzerwaMiedzyImpulsamiMs = 0;
unsigned long deadlineKoniecSeriiMs = 0;
