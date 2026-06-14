/**
 * ============================================================
 *  HEFAS – Head-controlled Electronic Functional Assistive System
 * ============================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <algorithm>
#include <MPU6050.h>
#include <BleMouse.h>
#if CONFIG_BT_ENABLED
#include <BLEDevice.h>
#endif

#include "USB.h"
#include "USBHIDMouse.h"
#include "USBHIDKeyboard.h"

#include "hefas_config.h"
#include "hefas_webdebug.h"

extern "C" bool tud_mounted(void);
extern "C" bool tud_connected(void);

// ======================== OBIEKTY GLOBALNE ==========================

MPU6050            czujnikIMU;
BleMouse           bleMysz("HEFAS 4.0", "HEFAS Team", 100);
USBHIDMouse        usbMysz;
USBHIDKeyboard     usbKlaw;

// ===================== ZMIENNE KALIBRACJI ===========================

float offsetGx = 0.0f;
float offsetGy = 0.0f;
float offsetGz = 0.0f;

float ostatniaPredkoscGx = 0.0f;
float ostatniaPredkoscGy = 0.0f;
float ostatniaPredkoscGz = 0.0f;

// ==================== ZMIENNE RUCHU KURSORA =========================

int kursorDeltaX = 0;
int kursorDeltaY = 0;

uint32_t licznikKlikLewych  = 0;
uint32_t licznikKlikPrawych = 0;

// ================ ZLICZANIE MRUGNIĘĆ (seria impulsów) =================

uint8_t        licznikImpulsow           = 0;
unsigned long  czasOstatniegoImpulsu     = 0;
unsigned long  czasStartImpulsu          = 0;
unsigned long  deadlineKoniecSeriiMs      = 0;
unsigned long  czasPierwszegoImpulsuSerii = 0;
unsigned long  ostatniaPrzerwaMiedzyImpulsamiMs = 0;
unsigned long  czasKoniecRefrakcjiMrug   = 0;
bool           seriaMrugniecAktywna      = false;

// Potwierdzenie stanu oka (N próbek × 10 ms) — anty-migotanie na progu histerezy
bool           okoPotwierdzoneZamkniete  = false;
uint8_t        licznikProbekZamknietych  = 0;
uint8_t        licznikProbekOtwartych    = 0;

// =============== DETEKTOR TCRT5000 ===================================

float          tcrtBaseline            = 0.0f;
float          tcrtFiltered            = 0.0f;   // wolniejsza — logi
float          tcrtFast                = 0.0f;   // szybka — progi
int            tcrtRaw                 = 0;
bool           wirtualnyStanCzujnika   = false;

// =============== TRYB BEZPRZEWODOWY / BLE =======================================
// XIAO ESP32-S3 (Plus): BAT+/BAT− → ładowarka na płytce (bez pomiaru % w MCU).
// trybBezprzewodowy = brak USB-HID u hosta → mysz po BLE.
// Status zasilania (debug) — niezależne: USB-HID, kabel USB, ogniwo, ładowanie.

bool           trybBezprzewodowy       = false;
bool           statusUsbHidAktywny     = false;  // tud_mounted()
bool           statusUsbKabelAktywny   = false;  // tud_connected() lub PIN_VBUS_ADC
bool           statusOgniwoMontowane   = false;  // HEFAS_OGNIOWO_ZAMONTOWANE
bool           statusLadowanie         = false;  // ogniwo + zasilanie z USB (kabel/5V)
bool           bleRadioWlaczony        = false;  // reklama BLE aktywna (mysz do sparowania)
bool           bleUspionyBezczynnoscia = false;  // reklama OFF z powodu bezczynności (do pobudzenia)
static bool    bleStosUruchomiony      = false;  // bleMysz.begin() wywołane (raz na sesję)
static unsigned long czasOstatniejAktywnosciUrz = 0;

// ===================== TRYB SCROLLA =================================

bool           trybScrolla          = false;
bool           przytrzymanieAktywne = false;

// =============== LED / KALIBRACJA / GOTOWOŚĆ =======================

unsigned long  koniecBlyskuLedMs    = 0;
bool           trwaKalibracja       = false;
bool           systemGotowy         = false;
bool           bannerGotowyWyslany  = false;

// =================== ZMIENNE DIAGNOSTYKI ============================

unsigned long  ostatniCzasDiagnostyki = 0;
bool           czyDebugWlaczony       = false;

// ============== MASZYNA STANÓW KLIKNIĘĆ HID ==========================

enum class StanHidKlik : uint8_t {
    Bezczynny = 0,
    PierwszyPress,
    PauzaMiedzyDouble,
    DrugiPress,
};

struct HidZadanie {
    uint8_t przycisk;
    bool    podwojnyLewy;
};

static StanHidKlik  stanHidKlik       = StanHidKlik::Bezczynny;
static uint8_t       hidPrzycisk       = 0;
static unsigned long hidCzasStanu      = 0;
static bool          hidDoubleClick    = false;

static HidZadanie hidKolejka[HID_KOLEJKA_ROZMIAR];
static uint8_t    hidKolejkaHead      = 0;
static uint8_t    hidKolejkaTail      = 0;

// ============== MASZYNA STANÓW KALIBRACJI ============================

enum class StanKalibracji : uint8_t {
    Bezczynny = 0,
    Rownolegle,
    IrStabilizacja,
};

static StanKalibracji stanKalibracji     = StanKalibracji::Bezczynny;
static unsigned long  calNastepnyTickMs  = 0;
static int            calIndeks          = 0;
static int32_t        calSumaGx          = 0;
static int32_t        calSumaGy          = 0;
static int32_t        calSumaGz          = 0;
static int            calProbkiTCRT[CAL_PROBKI_ROWNOLEGLE];
static float          calBaselineWstepny = 0.0f;

// ============== MASZYNA STANÓW MRUGNIĘĆ LED ============================

enum class StanLedMrug : uint8_t {
    Bezczynny = 0,
    Swieci,
    Zgaszony,
};

static StanLedMrug  stanLedMrug       = StanLedMrug::Bezczynny;
static int          ledMrugPozostalo  = 0;
static int          ledMrugCzasMs     = 100;
static unsigned long ledMrugCzasStanu = 0;

// ===================== DEKLARACJE =====================================

void rozpocznijKalibracje(const char* zrodlo = nullptr);
void odswiezKalibracje();
void rozpocznijMrugnieciaLed(int ile, int czasMs);
void odswiezMrugnieciaLed();
static void hidKolejkaUruchomNastepne();
static void obsluzGestPrzechylenia();
static void odswiezGestKlawiatury();
static void rozpocznijSkrotKlawiaturyEkranowej();
static bool czyBleHidAktywne();
static bool hidMoznaWyslac();
static void odswiezSterowanieBle();
static void zarejestrujAktywnoscUrzadzenia();
static void probujProbudzicBleRuchiem();
static void odswiezStatusZasilania();
static bool czyZamrozicBaselineTCRT();

// ===================== FUNKCJE POMOCNICZE ===========================

bool czyUSBPodlaczone() {
    return tud_mounted();
}

#if defined(PIN_VBUS_ADC)
static bool czyZasilanie5VNaPinie() {
    int raw = analogRead(PIN_VBUS_ADC);
    return raw >= (int)VBUS_ADC_PROG_USB;
}
#endif

static bool czyUsbKabelLub5V() {
#if defined(PIN_VBUS_ADC)
    return czyZasilanie5VNaPinie();
#else
    return tud_connected();
#endif
}

static void odswiezStatusZasilaniaWew() {
    statusUsbHidAktywny   = czyUSBPodlaczone();
    statusUsbKabelAktywny = czyUsbKabelLub5V();
#if HEFAS_OGNIOWO_ZAMONTOWANE
    statusOgniwoMontowane = true;
#else
    statusOgniwoMontowane = false;
#endif
    statusLadowanie = statusOgniwoMontowane && statusUsbKabelAktywny;
    trybBezprzewodowy = !statusUsbHidAktywny;
}

static bool czyBleHidAktywne() {
    return bleRadioWlaczony && bleMysz.isConnected();
}

static bool czyZamrozicBaselineTCRT() {
    return trwaKalibracja || wirtualnyStanCzujnika || okoPotwierdzoneZamkniete ||
           przytrzymanieAktywne || seriaMrugniecAktywna || licznikImpulsow > 0;
}

void ustawBlyskLed(uint32_t czasMs) {
    koniecBlyskuLedMs = millis() + czasMs;
}

static bool ledMruganieAktywne() {
    return stanLedMrug != StanLedMrug::Bezczynny;
}

void rozpocznijMrugnieciaLed(int ile, int czasMs) {
    if (ile <= 0 || czasMs <= 0) return;
    ledMrugPozostalo  = ile;
    ledMrugCzasMs     = czasMs;
    stanLedMrug       = StanLedMrug::Swieci;
    ledMrugCzasStanu  = millis();
    digitalWrite(LED_BUILTIN, HIGH);
}

void odswiezMrugnieciaLed() {
    if (stanLedMrug == StanLedMrug::Bezczynny) return;

    unsigned long teraz = millis();
    if (teraz - ledMrugCzasStanu < (unsigned long)ledMrugCzasMs) return;

    if (stanLedMrug == StanLedMrug::Swieci) {
        digitalWrite(LED_BUILTIN, LOW);
        stanLedMrug      = StanLedMrug::Zgaszony;
        ledMrugCzasStanu = teraz;
    } else {
        ledMrugPozostalo--;
        if (ledMrugPozostalo <= 0) {
            stanLedMrug = StanLedMrug::Bezczynny;
        } else {
            digitalWrite(LED_BUILTIN, HIGH);
            stanLedMrug      = StanLedMrug::Swieci;
            ledMrugCzasStanu = teraz;
        }
    }
}

void odswiezLed() {
    odswiezMrugnieciaLed();

    if (trybScrolla || przytrzymanieAktywne) {
        digitalWrite(LED_BUILTIN, HIGH);
        return;
    }

    if (trwaKalibracja) {
        if (stanKalibracji == StanKalibracji::Rownolegle) {
            digitalWrite(LED_BUILTIN, HIGH);
        }
        return;
    }

    if (ledMruganieAktywne()) return;

    // Brak aktywnej myszy (USB-HID lub BLE sparowane): mrug LED = sparuj BLE / pobudź radio,
    // nie „podłącz USB” (ogniwo może być słabe, ale urządzenie i tak działa po BLE).
    if (systemGotowy && !hidMoznaWyslac()) {
        static unsigned long ostatnieMigBrakPol = 0;
        unsigned long teraz = millis();
        if (teraz - ostatnieMigBrakPol >= OKRES_MIG_LED_BRAK_POL_MS) {
            ostatnieMigBrakPol = teraz;
            koniecBlyskuLedMs = teraz + CZAS_MIG_LED_BRAK_POL_ON_MS;
        }
    }

    digitalWrite(LED_BUILTIN, (millis() < koniecBlyskuLedMs) ? HIGH : LOW);
}

static void mrugnijDiodaBlokujaco(int ile, int czasMs) {
    for (int i = 0; i < ile; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(czasMs);
        digitalWrite(LED_BUILTIN, LOW);
        delay(czasMs);
    }
}

static bool hidMoznaWyslac() {
    return czyUSBPodlaczone() || czyBleHidAktywne();
}

static void hidMousePress(uint8_t przycisk) {
    if (czyUSBPodlaczone())      usbMysz.press(przycisk);
    else if (czyBleHidAktywne()) bleMysz.press(przycisk);
}

static void hidMouseRelease(uint8_t przycisk) {
    if (czyUSBPodlaczone())      usbMysz.release(przycisk);
    else if (czyBleHidAktywne()) bleMysz.release(przycisk);
}

static bool hidKlikZajety() {
    return stanHidKlik != StanHidKlik::Bezczynny;
}

// =================== KALIBRACJA — POMOCNICZE =========================

static float obliczBaselineTCRT(const int* probki, int liczbaProb) {
    int probkiKopia[CAL_PROBKI_ROWNOLEGLE];
    for (int i = 0; i < liczbaProb; i++) {
        probkiKopia[i] = probki[i];
    }
    std::sort(probkiKopia, probkiKopia + liczbaProb);
    int odrzuc = liczbaProb / 10;
    int liczba = liczbaProb - 2 * odrzuc;
    long suma  = 0;
    for (int i = odrzuc; i < liczbaProb - odrzuc; i++) {
        suma += probkiKopia[i];
    }
    return (float)suma / (float)liczba;
}

static void sprawdzBaselineTCRT(const char* prefiks) {
    if (tcrtBaseline < 100.0f || tcrtBaseline > 3900.0f) {
        if (czyDebugWlaczony) {
            Serial.print(prefiks);
            Serial.println(F(" OSTRZEZENIE: baseline TCRT poza zakresem"));
        }
        webDebugLogKategoria(WEBLOG_IR, String(prefiks) + " OSTRZEZENIE: baseline TCRT poza zakresem");
        rozpocznijMrugnieciaLed(5, 100);
    }
}

static void zakonczKalibracje(bool pierwszyStart) {
    trwaKalibracja    = false;
    stanKalibracji    = StanKalibracji::Bezczynny;
    digitalWrite(LED_BUILTIN, LOW);

    okoPotwierdzoneZamkniete = false;
    licznikProbekZamknietych  = 0;
    licznikProbekOtwartych    = 0;
    licznikImpulsow           = 0;
    seriaMrugniecAktywna      = false;
    deadlineKoniecSeriiMs     = 0;
    czasOstatniegoImpulsu     = 0;
    czasPierwszegoImpulsuSerii = 0;
    ostatniaPrzerwaMiedzyImpulsamiMs = 0;
    czasKoniecRefrakcjiMrug   = 0;

    if (czyDebugWlaczony) {
        Serial.print(F("[KAL] Gx="));      Serial.print(offsetGx, 1);
        Serial.print(F("  Gy="));          Serial.print(offsetGy, 1);
        Serial.print(F("  Gz="));          Serial.print(offsetGz, 1);
        Serial.print(F("  TCRT wst.="));   Serial.print(calBaselineWstepny, 1);
        Serial.print(F("  fin.="));       Serial.println(tcrtBaseline, 1);
    }
    webDebugLogKategoria(WEBLOG_IR, String("[KAL] Baseline wst.=") + (int)calBaselineWstepny
                         + " fin.=" + (int)tcrtBaseline);

    sprawdzBaselineTCRT("[KAL]");

    {
        String msg = F("[KAL] Gotowe — baseline ");
        msg += (int)tcrtBaseline;
        webDebugLogKategoria(WEBLOG_FSM, msg);
    }

    if (!systemGotowy) {
        systemGotowy = true;
        zarejestrujAktywnoscUrzadzenia();
    }

    rozpocznijMrugnieciaLed(3, 200);

    if (pierwszyStart && !bannerGotowyWyslany && czyDebugWlaczony) {
        bannerGotowyWyslany = true;
        Serial.println(F("============================================"));
        Serial.println(F("  HEFAS 4.0 GOTOWY"));
        Serial.println(F("  1 mrug  = LPM"));
        Serial.println(F("  2 mrug  = double-click LPM (wyl. scroll: OFF)"));
        Serial.println(F("  3 mrug  = PPM"));
        Serial.println(F("  4 mrug  = scroll ON (gdy wylaczony)"));
        Serial.println(F("  5 mrug  = rekalibracja MPU + TCRT"));
        Serial.println(F("  6 mrug  = toggle trybu debug"));
        Serial.println(F("  Przechyl glowe w PRAWO (~0,3 s) = klaw. ekr. (Ctrl+Win+O, USB)"));
        Serial.println(F("============================================"));
    }
}

void rozpocznijKalibracje(const char* zrodlo) {
    if (trwaKalibracja) return;

    analogReadResolution(12);
    trwaKalibracja       = true;
    stanKalibracji       = StanKalibracji::Rownolegle;
    calIndeks            = 0;
    calSumaGx            = 0;
    calSumaGy            = 0;
    calSumaGz            = 0;
    calNastepnyTickMs    = millis();

    if (czyDebugWlaczony) {
        Serial.print(F("[KAL] Start"));
        if (zrodlo) {
            Serial.print(F(" ("));
            Serial.print(zrodlo);
            Serial.print(')');
        }
        Serial.println(F(" MPU + TCRT ~3 s"));
    }
    {
        String msg = F("[KAL] Start");
        if (zrodlo) {
            msg += F(" (");
            msg += zrodlo;
            msg += ')';
        }
        msg += F(" — MPU + TCRT ~3 s");
        webDebugLogKategoria(WEBLOG_FSM, msg);
    }
    webDebugLogKategoria(WEBLOG_IR, "[KAL] Start kalibracji MPU + TCRT...");
    digitalWrite(LED_BUILTIN, HIGH);
}

void odswiezKalibracje() {
    if (stanKalibracji == StanKalibracji::Bezczynny) return;

    unsigned long teraz = millis();
    if (teraz < calNastepnyTickMs) return;
    calNastepnyTickMs = teraz + CAL_PRZERWA_MS;

    switch (stanKalibracji) {
        case StanKalibracji::Rownolegle: {
            int16_t ax, ay, az, gx, gy, gz;
            czujnikIMU.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
            calSumaGx += gx;
            calSumaGy += gy;
            calSumaGz += gz;
            calProbkiTCRT[calIndeks] = analogRead(PIN_TCRT_ANALOG);
            calIndeks++;

            if (calIndeks < CAL_PROBKI_ROWNOLEGLE) break;

            offsetGx = (float)calSumaGx / (float)CAL_PROBKI_ROWNOLEGLE;
            offsetGy = (float)calSumaGy / (float)CAL_PROBKI_ROWNOLEGLE;
            offsetGz = (float)calSumaGz / (float)CAL_PROBKI_ROWNOLEGLE;

            calBaselineWstepny          = obliczBaselineTCRT(calProbkiTCRT, CAL_PROBKI_ROWNOLEGLE);
            tcrtBaseline                = calBaselineWstepny;
            tcrtFiltered                = calBaselineWstepny;
            tcrtFast                    = calBaselineWstepny;
            wirtualnyStanCzujnika       = false;

            calIndeks      = 0;
            stanKalibracji = StanKalibracji::IrStabilizacja;
            webDebugLogKategoria(WEBLOG_IR, "[KAL] Stabilizacja IR (~1 s)...");
            webDebugLogKategoria(WEBLOG_FSM, "[KAL] Stabilizacja IR (~1 s)...");
            break;
        }

        case StanKalibracji::IrStabilizacja: {
            int raw = analogRead(PIN_TCRT_ANALOG);
            tcrtFiltered = EMA_ALPHA_WYSWIETLANIA * (float)raw +
                           (1.0f - EMA_ALPHA_WYSWIETLANIA) * tcrtFiltered;
            tcrtFast = EMA_ALPHA_SZYBKI * (float)raw + (1.0f - EMA_ALPHA_SZYBKI) * tcrtFast;
            digitalWrite(LED_BUILTIN, (calIndeks % 20 < 10) ? HIGH : LOW);
            calIndeks++;

            if (calIndeks < CAL_PROBKI_IR) break;

            tcrtBaseline   = tcrtFiltered;
            bool pierwszy  = !systemGotowy;
            zakonczKalibracje(pierwszy);
            break;
        }

        default:
            break;
    }
}

// =========== AKTUALIZACJA DETEKTORA ANALOGOWEGO TCRT5000 ===========

void aktualizujDetektorTCRT() {
    tcrtRaw = analogRead(PIN_TCRT_ANALOG);

    tcrtFast = EMA_ALPHA_SZYBKI * (float)tcrtRaw + (1.0f - EMA_ALPHA_SZYBKI) * tcrtFast;
    tcrtFiltered = EMA_ALPHA_WYSWIETLANIA * (float)tcrtRaw +
                   (1.0f - EMA_ALPHA_WYSWIETLANIA) * tcrtFiltered;

    // Histereza na szybkiej ścieżce — lepsze odwzorowanie krótkich mrugnięć
    if (!wirtualnyStanCzujnika) {
        if (tcrtFast < (tcrtBaseline - OFFSET_TRIGGER) &&
            tcrtFast > (tcrtBaseline - OFFSET_MAX_ZWARCIA)) {
            wirtualnyStanCzujnika = true;
        }
    } else {
        if (tcrtFast > (tcrtBaseline - OFFSET_RELEASE)) {
            wirtualnyStanCzujnika = false;
        } else if (tcrtFast < (tcrtBaseline - OFFSET_MAX_ZWARCIA)) {
            wirtualnyStanCzujnika = false;
            okoPotwierdzoneZamkniete = false;
            licznikProbekZamknietych = 0;
            licznikProbekOtwartych   = 0;
            if (czyDebugWlaczony) {
                webDebugLogKategoria(WEBLOG_IR, "[TCRT] Zdarzenie mechaniczne - reset");
            }
        }
    }

    // Baseline tylko przy stabilnie otwartym oku i poza serią mrugnięć
    if (!czyZamrozicBaselineTCRT()) {
        tcrtBaseline = EMA_ALPHA_WOLNY * tcrtFast + (1.0f - EMA_ALPHA_WOLNY) * tcrtBaseline;
    }
}

// ================= TRYB ZASILANIA + STEROWANIE BLE ======================
// Reklama BLE od bootu, gdy brak USB-HID (także podczas kalibracji). USB-HID → reklama OFF.

#if CONFIG_BT_ENABLED
static void wlaczReklameBle();
static void wylaczReklameBle(bool zPowoduBezczynnosci);
#endif

static void odswiezStatusZasilania() {
    static bool popHid = false, popKabel = false, popOgn = false, popLad = false;

    odswiezStatusZasilaniaWew();

    if (popHid != statusUsbHidAktywny || popKabel != statusUsbKabelAktywny ||
        popOgn != statusOgniwoMontowane || popLad != statusLadowanie) {
        popHid   = statusUsbHidAktywny;
        popKabel = statusUsbKabelAktywny;
        popOgn   = statusOgniwoMontowane;
        popLad   = statusLadowanie;

        if (czyDebugWlaczony) {
            Serial.printf("[ZAS] HID=%s  USB-kabel/5V=%s  Ogniwo=%s  Ladowanie=%s  BLE=%s\n",
                          statusUsbHidAktywny ? "TAK" : "NIE",
                          statusUsbKabelAktywny ? "TAK" : "NIE",
                          statusOgniwoMontowane ? "TAK" : "NIE",
                          statusLadowanie ? "TAK" : "NIE",
                          bleRadioWlaczony ? "ON" : "OFF");
        }

        String msg = "[ZAS] ";
        if (statusOgniwoMontowane) msg += "Ogniwo ";
        if (statusUsbHidAktywny)   msg += "USB-HID ";
        else if (statusUsbKabelAktywny) msg += "USB(5V) ";
        if (statusLadowanie)       msg += "lad. ";
        if (!statusUsbHidAktywny && !statusUsbKabelAktywny && statusOgniwoMontowane) {
            msg += "-> BLE";
        }
        webDebugLogKategoria(WEBLOG_BAT, msg);
    }
}

#if CONFIG_BT_ENABLED
static void wlaczReklameBle() {
    if (!bleStosUruchomiony) {
        bleMysz.begin();
        bleStosUruchomiony = true;
    }
    BLEDevice::startAdvertising();
    bleRadioWlaczony = true;
    bleUspionyBezczynnoscia = false;
    if (czyDebugWlaczony) {
        Serial.println(F("[BLE] Reklama ON"));
    }
    webDebugLogKategoria(WEBLOG_HID, "[BLE] Reklama ON");
}

static void wylaczReklameBle(bool zPowoduBezczynnosci) {
    if (!bleStosUruchomiony) {
        bleRadioWlaczony = false;
        bleUspionyBezczynnoscia = false;
        return;
    }
    BLEDevice::getAdvertising()->stop();
    bleRadioWlaczony = false;
    bleUspionyBezczynnoscia = zPowoduBezczynnosci;
    if (czyDebugWlaczony) {
        Serial.println(zPowoduBezczynnosci
                       ? F("[BLE] Uspienie (bezczynnosc)")
                       : F("[BLE] Reklama OFF (USB-HID)"));
    }
    webDebugLogKategoria(WEBLOG_HID,
        zPowoduBezczynnosci ? "[BLE] Uspienie (bezczynnosc)" : "[BLE] Reklama OFF (USB-HID)");
}
#endif

static void zarejestrujAktywnoscUrzadzenia() {
    czasOstatniejAktywnosciUrz = millis();
}

#if CONFIG_BT_ENABLED
static void probujProbudzicBleRuchiem() {
    if (!bleUspionyBezczynnoscia || czyUSBPodlaczone() || !systemGotowy) return;
    float ruch = fabsf(ostatniaPredkoscGx) + fabsf(ostatniaPredkoscGy);
    if (ruch >= PROG_PROBUDZENIA_BLE_DEG_S) {
        zarejestrujAktywnoscUrzadzenia();
    }
}
#else
static void probujProbudzicBleRuchiem() {}
#endif

void odswiezSterowanieBle() {
    odswiezStatusZasilania();

    bool usb = czyUSBPodlaczone();
    bool chceBleRadia = !usb;   // reklama od startu na ogniwie (także w kalibracji)

#if CONFIG_BT_ENABLED
    if (!chceBleRadia) {
        if (bleRadioWlaczony || bleUspionyBezczynnoscia) {
            wylaczReklameBle(false);
        }
    } else {
        bool polaczony = bleMysz.isConnected();
        if (polaczony) {
            zarejestrujAktywnoscUrzadzenia();
        }

        unsigned long teraz = millis();
        unsigned long bezczynMs = (czasOstatniejAktywnosciUrz == 0)
                                  ? 0
                                  : (teraz - czasOstatniejAktywnosciUrz);
        bool bezczynny = !polaczony &&
                         (bezczynMs >= (unsigned long)CZAS_BEZCZYNNOSCI_BLE_MS);
        bool chceReklame = polaczony || !bezczynny;

        if (chceReklame) {
            if (!bleRadioWlaczony) {
                wlaczReklameBle();
            }
        } else if (bleRadioWlaczony) {
            wylaczReklameBle(true);
        }
    }
#else
    bleRadioWlaczony = false;
    bleUspionyBezczynnoscia = false;
#endif
}

// ================ ODCZYT I FILTRACJA DANYCH IMU ====================

void odczytajIMU() {
    int16_t ax, ay, az, gx, gy, gz;
    czujnikIMU.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    ostatniaPredkoscGx = ((float)gx - offsetGx) / CZULOSC_ZYRO_LSB;
    ostatniaPredkoscGy = ((float)gy - offsetGy) / CZULOSC_ZYRO_LSB;
    ostatniaPredkoscGz = ((float)gz - offsetGz) / CZULOSC_ZYRO_LSB;
    probujProbudzicBleRuchiem();

    float predkoscOsX = ostatniaPredkoscGx;
    float predkoscOsZ = ostatniaPredkoscGz;

    float progZ = trybScrolla ? PROG_ZYRO_SKROL_DEG_S : PROG_ZYROSKOPU;
    if (fabs(predkoscOsX) < PROG_ZYROSKOPU) predkoscOsX = 0.0f;
    if (fabs(predkoscOsZ) < progZ) predkoscOsZ = 0.0f;

    if (fabs(predkoscOsX) < STREFA_MARTWA) predkoscOsX = 0.0f;
    if (fabs(predkoscOsZ) < STREFA_MARTWA) predkoscOsZ = 0.0f;

    float dX = predkoscOsX * CZULOSC_MYSZY * ODWROC_OS_X;
    float dY = predkoscOsZ * CZULOSC_MYSZY * ODWROC_OS_Y;

    kursorDeltaX = constrain((int)dX, -127, 127);
    kursorDeltaY = constrain((int)dY, -127, 127);
}

// ============== GEST Gy: przechyl w prawo → Ctrl+Win (klawiatura ekranu) =====

enum class StanGestKlaw : uint8_t {
    Bezczynny = 0,
    Wcisniety,
};

static StanGestKlaw  stanGestKlaw       = StanGestKlaw::Bezczynny;
static unsigned long gestKlawCzasStanu  = 0;
static unsigned long gestCooldownKoniecMs = 0;
static bool          gestPrzechylTrwa     = false;
static unsigned long gestPrzechylStartMs  = 0;

static void rozpocznijSkrotKlawiaturyEkranowej() {
    zarejestrujAktywnoscUrzadzenia();
    if (!czyUSBPodlaczone()) {
        if (czyDebugWlaczony) {
            Serial.println(F("[GEST] Ctrl+Win+O: tylko USB-HID (mysz dziala po BLE)"));
            webDebugLogKategoria(WEBLOG_FSM, "[GEST] OSK: tylko USB-HID");
        }
        return;
    }
    if (stanGestKlaw != StanGestKlaw::Bezczynny) return;

    usbKlaw.press(KEY_LEFT_CTRL);
    usbKlaw.press(KEY_LEFT_GUI);
    usbKlaw.press('o');
    stanGestKlaw      = StanGestKlaw::Wcisniety;
    gestKlawCzasStanu = millis();
    rozpocznijMrugnieciaLed(2, 80);
    if (czyDebugWlaczony) {
        Serial.println(F("[GEST] prawo — Ctrl+Win+O"));
        webDebugLogKategoria(WEBLOG_FSM, "[GEST] prawo Ctrl+Win+O");
    }
}

static void odswiezGestKlawiatury() {
    if (stanGestKlaw == StanGestKlaw::Bezczynny) return;
    if ((millis() - gestKlawCzasStanu) < 60UL) return;

    usbKlaw.releaseAll();
    stanGestKlaw = StanGestKlaw::Bezczynny;
}

static void obsluzGestPrzechylenia() {
#if !GEST_PRZECHYL_PRAWO_WLACZONY
    return;
#else
    if (!systemGotowy || trwaKalibracja) return;
#if WEBDEBUG_AKTYWNY
    if (webPauzaMyszy) return;
#endif
    if (przytrzymanieAktywne || okoPotwierdzoneZamkniete || wirtualnyStanCzujnika) return;
    if (hidKlikZajety()) return;
    if (stanGestKlaw != StanGestKlaw::Bezczynny) return;
    if (millis() < gestCooldownKoniecMs) return;

    if (fabs(ostatniaPredkoscGx) > GEST_BLOKADA_GX_GZ_DEG_S ||
        fabs(ostatniaPredkoscGz) > GEST_BLOKADA_GX_GZ_DEG_S) {
        gestPrzechylTrwa = false;
        return;
    }

    float gy = ostatniaPredkoscGy * (float)ODWROC_GEST_GY;
    bool przechylPrawo = (gy < -GEST_GY_PROG_DEG_S);
    float progHist = -GEST_GY_PROG_DEG_S * GEST_GY_HISTEREZA_ODSET;

    if (przechylPrawo) {
        if (!gestPrzechylTrwa) {
            gestPrzechylTrwa    = true;
            gestPrzechylStartMs = millis();
        } else if ((millis() - gestPrzechylStartMs) >= (unsigned long)GEST_GY_CZAS_TRWANIA_MS) {
            rozpocznijSkrotKlawiaturyEkranowej();
            gestCooldownKoniecMs = millis() + (unsigned long)GEST_COOLDOWN_MS;
            gestPrzechylTrwa     = false;
        }
    } else if (gestPrzechylTrwa && gy > progHist) {
        gestPrzechylTrwa = false;
    }
#endif
}

// ============== WYSYŁANIE DANYCH MYSZY (USB / BLE) =================

void wyslijRuchMyszy(int dx, int dy) {
    zarejestrujAktywnoscUrzadzenia();
    if (trybScrolla) {
        int scroll = constrain(-dy / DZIELNIK_SCROLLA, -5, 5);
        if (scroll == 0) return;
        if (czyUSBPodlaczone())         usbMysz.move(0, 0, (int8_t)scroll);
        else if (czyBleHidAktywne())      bleMysz.move(0, 0, (int8_t)scroll);
    } else {
        if (czyUSBPodlaczone())         usbMysz.move((int8_t)dx, (int8_t)dy);
        else if (czyBleHidAktywne())      bleMysz.move((int8_t)dx, (int8_t)dy);
    }
}

static bool hidKolejkaEnqueue(uint8_t przycisk, bool podwojnyLewy) {
    uint8_t nextHead = (uint8_t)((hidKolejkaHead + 1) % HID_KOLEJKA_ROZMIAR);
    if (nextHead == hidKolejkaTail) {
        webDebugLogKategoria(WEBLOG_FSM, "[HID] Kolejka pelna - odrzucono klikniecie");
        return false;
    }
    hidKolejka[hidKolejkaHead].przycisk      = przycisk;
    hidKolejka[hidKolejkaHead].podwojnyLewy = podwojnyLewy;
    hidKolejkaHead = nextHead;
    return true;
}

static void rozpocznijKlikniecieHid(uint8_t przycisk, bool podwojnyLewy) {
    if (!hidMoznaWyslac()) return;

    if (stanHidKlik != StanHidKlik::Bezczynny) {
        hidKolejkaEnqueue(przycisk, podwojnyLewy);
        return;
    }

    hidPrzycisk    = przycisk;
    hidDoubleClick = podwojnyLewy;
    hidMousePress(przycisk);
    stanHidKlik    = StanHidKlik::PierwszyPress;
    hidCzasStanu  = millis();
}

static void hidKolejkaUruchomNastepne() {
    if (stanHidKlik != StanHidKlik::Bezczynny) return;
    if (hidKolejkaHead == hidKolejkaTail) return;

    HidZadanie zad = hidKolejka[hidKolejkaTail];
    hidKolejkaTail = (uint8_t)((hidKolejkaTail + 1) % HID_KOLEJKA_ROZMIAR);
    rozpocznijKlikniecieHid(zad.przycisk, zad.podwojnyLewy);
}

void odswiezHidKlikniecia() {
    if (stanHidKlik == StanHidKlik::Bezczynny) {
        hidKolejkaUruchomNastepne();
        return;
    }

    unsigned long teraz = millis();
    StanHidKlik poprzedni = stanHidKlik;

    switch (stanHidKlik) {
        case StanHidKlik::PierwszyPress:
            if (teraz - hidCzasStanu >= (unsigned long)CZAS_KROTKIEGO_KLIKU_MS) {
                hidMouseRelease(hidPrzycisk);
                if (hidPrzycisk == MOUSE_LEFT && hidDoubleClick) {
                    stanHidKlik  = StanHidKlik::PauzaMiedzyDouble;
                    hidCzasStanu = teraz;
                } else {
                    stanHidKlik = StanHidKlik::Bezczynny;
                    ustawBlyskLed(CZAS_BLYSKU_LED_MS);
                }
            }
            break;

        case StanHidKlik::PauzaMiedzyDouble:
            if (teraz - hidCzasStanu >= (unsigned long)CZAS_MIEDZY_KLIKAMI_PODWOJNEGO_MS) {
                hidMousePress(hidPrzycisk);
                stanHidKlik  = StanHidKlik::DrugiPress;
                hidCzasStanu = teraz;
            }
            break;

        case StanHidKlik::DrugiPress:
            if (teraz - hidCzasStanu >= (unsigned long)CZAS_KROTKIEGO_KLIKU_MS) {
                hidMouseRelease(hidPrzycisk);
                stanHidKlik = StanHidKlik::Bezczynny;
                ustawBlyskLed(CZAS_BLYSKU_LED_MS);
            }
            break;

        default:
            break;
    }

    if (poprzedni != StanHidKlik::Bezczynny && stanHidKlik == StanHidKlik::Bezczynny) {
        hidKolejkaUruchomNastepne();
    }
}

void wyslijKlikniecie(uint8_t przycisk) {
    rozpocznijKlikniecieHid(przycisk, false);
}

void wyslijPodwojnyKlikLewy() {
    rozpocznijKlikniecieHid(MOUSE_LEFT, true);
}

void wyslijPrzytrzymanie(bool wcisnij) {
    zarejestrujAktywnoscUrzadzenia();
    if (wcisnij) {
        if (czyUSBPodlaczone())         usbMysz.press(MOUSE_LEFT);
        else if (czyBleHidAktywne())      bleMysz.press(MOUSE_LEFT);
        if (czyDebugWlaczony) Serial.println(F("[DRAG] Przytrzymanie ON"));
        webDebugLogKategoria(WEBLOG_HID, "[DRAG] Przytrzymanie ON");
    } else {
        if (czyUSBPodlaczone())         usbMysz.release(MOUSE_LEFT);
        else if (czyBleHidAktywne())      bleMysz.release(MOUSE_LEFT);
        ustawBlyskLed(CZAS_BLYSKU_LED_MS);
        if (czyDebugWlaczony) Serial.println(F("[DRAG] Przytrzymanie OFF"));
        webDebugLogKategoria(WEBLOG_HID, "[DRAG] Przytrzymanie OFF");
    }
}

// ============= PRZETWARZANIE ZLICZONYCH IMPULSÓW ===================

void przetworzImpulsy(uint8_t licznik) {
    zarejestrujAktywnoscUrzadzenia();
    if (licznik > MAX_MRUGNIEC_W_SERII) {
        licznik = MAX_MRUGNIEC_W_SERII;
    }

    if (licznik == 6) {
        czyDebugWlaczony = !czyDebugWlaczony;
        Serial.println(czyDebugWlaczony ? F("[DEBUG] WLACZONY") : F("[DEBUG] WYLACZONY"));
        webDebugLogKategoria(WEBLOG_FSM, czyDebugWlaczony ? "[DEBUG] WLACZONY" : "[DEBUG] WYLACZONY");
        rozpocznijMrugnieciaLed(2, 100);
        return;
    }

    if (licznik == 5) {
        rozpocznijKalibracje("5 mrug");
        return;
    }

    if (trybScrolla && licznik >= 2) {
        trybScrolla = false;
        ustawBlyskLed(CZAS_BLYSKU_LED_MS);
        if (czyDebugWlaczony) {
            Serial.println(F("[SCROLL] OFF"));
            webDebugLogKategoria(WEBLOG_FSM, "[SCROLL] OFF (>=2 mrug)");
        }
        if (licznik == 2) return;
    }

    if (licznik == 4 && !trybScrolla) {
        trybScrolla = true;
        ustawBlyskLed(CZAS_BLYSKU_LED_MS);
        if (czyDebugWlaczony) {
            Serial.println(F("[SCROLL] ON"));
            webDebugLogKategoria(WEBLOG_FSM, "[SCROLL] ON");
        }
        return;
    }

    if (trybScrolla) return;

    if (licznik == 3) {
        wyslijKlikniecie(MOUSE_RIGHT);
        licznikKlikPrawych++;
        if (czyDebugWlaczony) {
            Serial.println(F("[KLIK] PRAWY"));
            webDebugLogKategoria(WEBLOG_FSM, "[KLIK] PRAWY");
        }
    } else if (licznik == 2) {
        wyslijPodwojnyKlikLewy();
        licznikKlikLewych += 2;
        if (czyDebugWlaczony) {
            Serial.println(F("[KLIK] DOUBLE LEWY"));
            webDebugLogKategoria(WEBLOG_FSM, "[KLIK] DOUBLE LEWY");
        }
    } else if (licznik == 1) {
        wyslijKlikniecie(MOUSE_LEFT);
        licznikKlikLewych++;
        if (czyDebugWlaczony) {
            Serial.println(F("[KLIK] LEWY"));
            webDebugLogKategoria(WEBLOG_FSM, "[KLIK] LEWY");
        }
    }
}

// ================== DETEKCJA MRUGNIĘĆ =================================

static unsigned long ciszaPoLiczbieImpulsow(uint8_t n) {
    switch (n) {
        case 1: return (unsigned long)SERIA_CISZA_PO_1_MS;
        case 2: return (unsigned long)SERIA_CISZA_PO_2_MS;
        case 3: return (unsigned long)SERIA_CISZA_PO_3_MS;
        case 4: return (unsigned long)SERIA_CISZA_PO_4_MS;
        case 5: return (unsigned long)SERIA_CISZA_PO_5_MS;
        default: return (unsigned long)SERIA_CISZA_PO_6_MS;
    }
}

static bool czySeriaCzasowoPoprawna(uint8_t licznik) {
    if (licznik == 0 || czasPierwszegoImpulsuSerii == 0) return true;
    unsigned long span = czasOstatniegoImpulsu - czasPierwszegoImpulsuSerii;
    if (licznik == 3) {
        return span >= (unsigned long)SERIA_CZAS_MIN_3_MS &&
               span <= (unsigned long)SERIA_CZAS_MAX_3_MS;
    }
    if (licznik == 4) return span <= (unsigned long)SERIA_CZAS_MAX_4_MS;
    if (licznik == 5) return span <= (unsigned long)SERIA_CZAS_MAX_5_MS;
    if (licznik >= 6) return span <= (unsigned long)SERIA_CZAS_MAX_6_MS;
    return true;
}

static void zarejestrujMrugniecie(unsigned long teraz) {
    zarejestrujAktywnoscUrzadzenia();

    if (licznikImpulsow > 0) {
        ostatniaPrzerwaMiedzyImpulsamiMs = teraz - czasOstatniegoImpulsu;
    }

    if (licznikImpulsow > 0 &&
        (teraz - czasOstatniegoImpulsu) <= (unsigned long)OKNO_MIEDZY_IMPULSAMI_MS) {
        if (licznikImpulsow < MAX_MRUGNIEC_W_SERII) {
            licznikImpulsow++;
        }
    } else {
        licznikImpulsow           = 1;
        czasPierwszegoImpulsuSerii = teraz;
        ostatniaPrzerwaMiedzyImpulsamiMs = 0;
    }

    czasOstatniegoImpulsu    = teraz;
    seriaMrugniecAktywna     = true;
    if (CZAS_REFRAKTORY_PO_IMPULSIE_MS > 0) {
        czasKoniecRefrakcjiMrug = teraz + (unsigned long)CZAS_REFRAKTORY_PO_IMPULSIE_MS;
    }
    deadlineKoniecSeriiMs    = teraz + ciszaPoLiczbieImpulsow(licznikImpulsow);
}

static void obsluzZboczeOtwarciaOka(unsigned long teraz) {
    if (przytrzymanieAktywne) {
        przytrzymanieAktywne = false;
        wyslijPrzytrzymanie(false);
        return;
    }

    unsigned long czasTrwania = teraz - czasStartImpulsu;
    if (czasTrwania < (unsigned long)CZAS_MIN_MRUG_MS) {
        if (czyDebugWlaczony) {
            String msg = "[TCRT] Artefakt odrzucony (";
            msg += (int)czasTrwania;
            msg += "ms)";
            webDebugLogKategoria(WEBLOG_FSM, msg);
        }
        return;
    }

    if (czasTrwania >= (unsigned long)PROG_PRZYTRZYMANIA_MS) {
        return;
    }

    if (czasTrwania > (unsigned long)CZAS_MAX_MRUG_MS) {
        if (czyDebugWlaczony) {
            webDebugLogKategoria(WEBLOG_FSM,
                String("[TCRT] Dlugie zamkniecie (") + (int)czasTrwania
                + "ms) — bez mrugniecia (strefa drag)");
        }
        return;
    }

    zarejestrujMrugniecie(teraz);
}

static void sprawdzTimeoutSerii(unsigned long teraz) {
    if (!seriaMrugniecAktywna || licznikImpulsow == 0) return;
    if (okoPotwierdzoneZamkniete) return;
    if (deadlineKoniecSeriiMs == 0 || teraz < deadlineKoniecSeriiMs) return;

    uint8_t doPrzetworzenia = licznikImpulsow;

    if (!czySeriaCzasowoPoprawna(doPrzetworzenia)) {
        if (czyDebugWlaczony) {
            unsigned long span = czasOstatniegoImpulsu - czasPierwszegoImpulsuSerii;
            String msg = "[SERIA] ";
            msg += (int)doPrzetworzenia;
            msg += " odrzucone (czas ";
            msg += (int)span;
            msg += "ms)";
            Serial.println(msg);
            webDebugLogKategoria(WEBLOG_FSM, msg);
        }
        licznikImpulsow              = 0;
        seriaMrugniecAktywna         = false;
        deadlineKoniecSeriiMs        = 0;
        czasPierwszegoImpulsuSerii   = 0;
        ostatniaPrzerwaMiedzyImpulsamiMs = 0;
        return;
    }

    if (czyDebugWlaczony) {
        unsigned long span = (czasPierwszegoImpulsuSerii > 0)
                             ? (czasOstatniegoImpulsu - czasPierwszegoImpulsuSerii) : 0;
        String seriaMsg = "[SERIA] ";
        seriaMsg += (int)doPrzetworzenia;
        seriaMsg += " impulsow T=";
        seriaMsg += (int)span;
        seriaMsg += "ms przerwa=";
        seriaMsg += (int)ostatniaPrzerwaMiedzyImpulsamiMs;
        seriaMsg += "ms";
        Serial.println(seriaMsg);
        webDebugLogKategoria(WEBLOG_FSM, seriaMsg);
    }
    licznikImpulsow              = 0;
    seriaMrugniecAktywna         = false;
    deadlineKoniecSeriiMs        = 0;
    czasPierwszegoImpulsuSerii   = 0;
    ostatniaPrzerwaMiedzyImpulsamiMs = 0;
    przetworzImpulsy(doPrzetworzenia);
}

void obsluzKlikniecia() {
    if (!systemGotowy) return;

    unsigned long teraz = millis();

    if (wirtualnyStanCzujnika) {
        licznikProbekZamknietych++;
        licznikProbekOtwartych = 0;
    } else {
        licznikProbekOtwartych++;
        licznikProbekZamknietych = 0;
    }

    if (!okoPotwierdzoneZamkniete &&
        licznikProbekZamknietych >= PROBKI_POTWIERDZENIA_STANU) {
        if (CZAS_REFRAKTORY_PO_IMPULSIE_MS > 0 && teraz < czasKoniecRefrakcjiMrug) {
            licznikProbekZamknietych = 0;
        } else {
        okoPotwierdzoneZamkniete = true;
        czasStartImpulsu         = teraz;
        licznikProbekOtwartych   = 0;
        }
    } else if (okoPotwierdzoneZamkniete &&
               licznikProbekOtwartych >= (przytrzymanieAktywne
                   ? (uint8_t)PROBKI_POTWIERDZENIA_DRAG_OFF
                   : (uint8_t)PROBKI_POTWIERDZENIA_STANU)) {
        obsluzZboczeOtwarciaOka(teraz);
        okoPotwierdzoneZamkniete  = false;
        licznikProbekZamknietych  = 0;
    } else if (okoPotwierdzoneZamkniete && !przytrzymanieAktywne && !trybScrolla) {
        if ((teraz - czasStartImpulsu) >= (unsigned long)PROG_PRZYTRZYMANIA_MS) {
            przytrzymanieAktywne = true;
            wyslijPrzytrzymanie(true);
            licznikImpulsow              = 0;
            seriaMrugniecAktywna         = false;
            deadlineKoniecSeriiMs        = 0;
            czasPierwszegoImpulsuSerii   = 0;
            ostatniaPrzerwaMiedzyImpulsamiMs = 0;
        }
    }

    sprawdzTimeoutSerii(teraz);
}

// ======================== DIAGNOSTYKA ==============================

void diagnostyka() {
    if (!czyDebugWlaczony) return;

    unsigned long teraz = millis();
    if ((teraz - ostatniCzasDiagnostyki) < (unsigned long)OKRES_DIAGNOSTYKI_MS) return;
    ostatniCzasDiagnostyki = teraz;

    String tcrtMsg = "[TCRT] Raw=";
    tcrtMsg += tcrtRaw;
    tcrtMsg += "  Fast=";
    tcrtMsg += (int)tcrtFast;
    tcrtMsg += "  Base=";
    tcrtMsg += (int)tcrtBaseline;
    tcrtMsg += wirtualnyStanCzujnika ? "  [ZAMKN]" : "  [OTW]";
    tcrtMsg += okoPotwierdzoneZamkniete ? "  [POTW]" : "";
    Serial.println(tcrtMsg);
    webDebugLogKategoria(WEBLOG_IR, tcrtMsg);

    if (kursorDeltaX == 0 && kursorDeltaY == 0) return;

    String msg = trybScrolla ? "[SCROLL] " : "[RUCH] ";
    msg += "dX="; msg += kursorDeltaX;
    msg += "  dY="; msg += kursorDeltaY;
    msg += "  [";
    if (czyUSBPodlaczone())      msg += "USB";
    else if (czyBleHidAktywne()) msg += "BLE";
    else                         msg += "---";
    msg += "]";

    Serial.println(msg);
    webDebugLogKategoria(WEBLOG_GYRO, msg);
}

// ============================ SETUP ================================

void setup() {
    Serial.begin(PREDKOSC_SERIAL);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    delay(CZAS_STABILIZACJI_START_MS);

    if (czyDebugWlaczony) {
        Serial.println();
        Serial.println(F("============================================"));
        Serial.println(F("  HEFAS 4.0 – Air Mouse  |  Inicjalizacja..."));
        Serial.println(F("============================================"));
    }

    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(400000);
    czujnikIMU.initialize();

    if (!czujnikIMU.testConnection()) {
        if (czyDebugWlaczony) Serial.println(F("[BLAD] MPU6050 brak odpowiedzi!"));
        while (true) { mrugnijDiodaBlokujaco(5, 100); delay(500); }
    }
    if (czyDebugWlaczony) Serial.println(F("[OK] MPU6050 polaczony."));

    rozpocznijKalibracje("start");

    usbMysz.begin();
    usbKlaw.begin();
    USB.begin();
    if (czyDebugWlaczony) Serial.println(F("[OK] USB HID Mouse + Keyboard."));

#if defined(PIN_VBUS_ADC)
    pinMode(PIN_VBUS_ADC, INPUT);
#endif
    odswiezStatusZasilaniaWew();

    webDebugInit();

    odswiezSterowanieBle();
#if CONFIG_BT_ENABLED
    if (!czyUSBPodlaczone() && !bleRadioWlaczony) {
        wlaczReklameBle();
    }
#endif

    if (czyDebugWlaczony) {
        Serial.printf("[OK] Zasil: HID=%s USB=%s Ogniwo=%s\n",
                      statusUsbHidAktywny ? "TAK" : "nie",
                      statusUsbKabelAktywny ? "TAK" : "nie",
                      statusOgniwoMontowane ? "TAK" : "nie");
        Serial.println(bleRadioWlaczony
                       ? F("[OK] BLE: reklama ON")
                       : F("[OK] BLE: OFF (USB-HID aktywne)"));
    }
}

// ======================== PĘTLA GŁÓWNA =============================

void loop() {
    webDebugLoop();

#if WEBDEBUG_AKTYWNY
    if (webZadanieRekalibracji) {
        webZadanieRekalibracji = false;
        rozpocznijKalibracje("WebDebug");
    }
#endif

    odczytajIMU();
    obsluzGestPrzechylenia();
    odswiezGestKlawiatury();
    aktualizujDetektorTCRT();
    odswiezSterowanieBle();
    odswiezKalibracje();
    odswiezHidKlikniecia();

#if WEBDEBUG_AKTYWNY
    if (!webPauzaMyszy) {
#endif
        obsluzKlikniecia();
        if (!hidKlikZajety() && (kursorDeltaX != 0 || kursorDeltaY != 0)) {
            wyslijRuchMyszy(kursorDeltaX, kursorDeltaY);
        }
#if WEBDEBUG_AKTYWNY
    }
#endif

    odswiezLed();
    diagnostyka();
    delay(OKRES_PETLI_MS);
}
