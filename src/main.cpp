/**
 * ============================================================
 *  HEFAS – Head-controlled Electronic Functional Assistive System
 * ============================================================
 *  Platforma:   Seeed Studio XIAO ESP32-S3 Plus
 *  Czujnik IMU: MPU6050 (6-DoF)
 *  Detektor:    TCRT5000 + LM393 (Active LOW)
 *  Komunikacja: USB HID (priorytet) / Bluetooth Low Energy
 *
 *  Autorzy: Sebastian Sobczyk, Bartłomiej Adamczyk
 *  Kierunek: Mechatronika – Szczecin 2026
 * ============================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <MPU6050.h>
#include <BleMouse.h>

#include "USB.h"
#include "USBHIDMouse.h"

#include "hefas_config.h"
#include "hefas_webdebug.h"

// Funkcja TinyUSB – zwraca true gdy host USB zakonczyl enumeracje
// urzadzenia (kabel podpiety do komputera i system go rozpoznal).
extern "C" bool tud_mounted(void);

// ======================== OBIEKTY GLOBALNE ==========================

MPU6050            czujnikIMU;                              // Czujnik inercyjny 6-DoF (I2C)
BleMouse           bleMysz("HEFAS", "HEFAS Team", 100);    // Mysz BLE (nazwa, producent, bateria%)
USBHIDMouse        usbMysz;                                // Mysz USB HID (natywny USB ESP32-S3)

// ===================== ZMIENNE KALIBRACJI ===========================
// Offsety wyznaczane przy starcie przez usrednienie PROBKI_KALIBRACJI
// odczytow zyroskopu w spoczynku. Odejmowane od kazdego kolejnego
// odczytu, aby wyeliminowac staly blad systematyczny (bias).

float offsetGx = 0.0f;
float offsetGy = 0.0f;
float offsetGz = 0.0f;

// ==================== ZMIENNE RUCHU KURSORA =========================
// Obliczone w odczytajIMU(), konsumowane w wyslijRuchMyszy().
// Zakres: -127..127 (wymaganie protokolu HID).

int kursorDeltaX = 0;
int kursorDeltaY = 0;

// Liczniki klikniec – odczytywane przez modul WebDebug.
uint32_t licznikKlikLewych  = 0;
uint32_t licznikKlikPrawych = 0;

// =================== DEBOUNCE CZUJNIKA LM393 =======================
// Programowa eliminacja drgan – ignorujemy zmiany stanu krotsze
// niz CZAS_DEBOUNCE_MS. Dopiero po ustabilizowaniu przetwarzamy.

bool           poprzedniOdczytCzujnika = HIGH;   // Surowy odczyt z poprzedniej iteracji
bool           ostatniStabilnyOdczyt   = HIGH;   // Stan po przejsciu debounce
unsigned long  czasOstatniegoDebounce  = 0;      // Timestamp ostatniej zmiany surowego odczytu

// ================ ZLICZANIE IMPULSÓW (KLIKNIĘCIA) ==================
// Impulsy sa zliczane na zboczu opadajacym (oko otwiera sie po
// krotkim mrugnieciu). Jesli miedzy impulsami minie mniej niz
// OKNO_WIELOKLIKU_MS – naleza do tej samej serii.

uint8_t        licznikImpulsow         = 0;      // Ile impulsow w biezacej serii
unsigned long  czasOstatniegoImpulsu   = 0;      // Kiedy zakonczyl sie ostatni impuls
unsigned long  czasStartImpulsu        = 0;      // Kiedy rozpoczal sie biezacy impuls

// ===================== TRYB SCROLLA ================================
// Aktywowany 3 mrugnieciami (dioda swieci ciagle).
// Dezaktywowany 2 mrugnieciami. W tym trybie ruch glowy gora/dol
// steruje kolkiem myszy zamiast kursorem; klikniecia zablokowane.

bool           trybScrolla             = false;

// ============= PRZYTRZYMANIE LEWEGO PRZYCISKU (DRAG) ===============
// Aktywowane gdy czujnik jest aktywny nieprzerwanie dluzej niz
// PROG_PRZYTRZYMANIA_MS. Lewy przycisk pozostaje wcisniety az
// uzytkownik otworzy oko (drag & drop). Dioda swieci ciagle.

bool           przytrzymanieAktywne    = false;

// =============== NIBLOKUJĄCY BŁYSK DIODY WBUDOWANEJ ================
// Wartosc millis(), po ktorej dioda gaśnie. Ustawiana przez
// ustawBlyskLed(). W trybie scrolla/drag dioda swieci niezaleznie.

unsigned long  koniecBlyskuLedMs       = 0;

// =================== ZMIENNE DIAGNOSTYKI ============================

unsigned long  ostatniCzasDiagnostyki  = 0;

// ===================== FUNKCJE POMOCNICZE ===========================

// Ustawia czas trwania blysku diody wbudowanej [ms].
// Niblokujace – dioda gaszona automatycznie w odswiezLed().
void ustawBlyskLed(uint32_t czasMs) {
    koniecBlyskuLedMs = millis() + czasMs;
}

/**
 * Stan diody wbudowanej zależy od trybu:
 *   - tryb scrolla lub drag  → dioda świeci ciągle,
 *   - normalny tryb          → krótki błysk po kliknięciu.
 */
void odswiezLed() {
    if (trybScrolla || przytrzymanieAktywne) {
        digitalWrite(LED_BUILTIN, HIGH);
        return;
    }
    digitalWrite(LED_BUILTIN, (millis() < koniecBlyskuLedMs) ? HIGH : LOW);
}

// Blokujace migniecie dioda – uzywane TYLKO przy starcie (kalibracja, gotowość).
void mrugnijDioda(int ile, int czasMs) {
    for (int i = 0; i < ile; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(czasMs);
        digitalWrite(LED_BUILTIN, LOW);
        delay(czasMs);
    }
}

// Sprawdza czy kabel USB jest podpiety do hosta (komputer rozpoznal HID).
bool czyUSBPodlaczone() {
    return tud_mounted();
}

// =================== KALIBRACJA ŻYROSKOPU ==========================

void kalibracjaZyroskopu() {
    if (TRYB_DEBUG) {
        Serial.println(F("[KALIBRACJA] Nie ruszaj glowa..."));
    }

    int32_t sumaGx = 0, sumaGy = 0, sumaGz = 0;
    int16_t ax, ay, az, gx, gy, gz;

    for (int i = 0; i < PROBKI_KALIBRACJI; i++) {
        czujnikIMU.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
        sumaGx += gx;
        sumaGy += gy;
        sumaGz += gz;
        delay(5);
    }

    offsetGx = (float)sumaGx / PROBKI_KALIBRACJI;
    offsetGy = (float)sumaGy / PROBKI_KALIBRACJI;
    offsetGz = (float)sumaGz / PROBKI_KALIBRACJI;

    if (TRYB_DEBUG) {
        Serial.print(F("[KALIBRACJA] Gx="));
        Serial.print(offsetGx, 1);
        Serial.print(F("  Gy="));
        Serial.print(offsetGy, 1);
        Serial.print(F("  Gz="));
        Serial.println(offsetGz, 1);
    }
}

// ================ ODCZYT I FILTRACJA DANYCH IMU ====================

/**
 * Mapowanie osi (empiryczne, montaż czujnika na czole):
 *   gx → kursor X (lewo/prawo)
 *   gz → kursor Y (góra/dół)
 */
void odczytajIMU() {
    int16_t ax, ay, az, gx, gy, gz;
    czujnikIMU.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    float predkoscOsX = ((float)gx - offsetGx) / CZULOSC_ZYRO_LSB;
    float predkoscOsZ = ((float)gz - offsetGz) / CZULOSC_ZYRO_LSB;

    if (fabs(predkoscOsX) < PROG_ZYROSKOPU) predkoscOsX = 0.0f;
    if (fabs(predkoscOsZ) < PROG_ZYROSKOPU) predkoscOsZ = 0.0f;

    if (fabs(predkoscOsX) < STREFA_MARTWA) predkoscOsX = 0.0f;
    if (fabs(predkoscOsZ) < STREFA_MARTWA) predkoscOsZ = 0.0f;

    float dX = predkoscOsX * CZULOSC_MYSZY * ODWROC_OS_X;
    float dY = predkoscOsZ * CZULOSC_MYSZY * ODWROC_OS_Y;

    kursorDeltaX = constrain((int)dX, -127, 127);
    kursorDeltaY = constrain((int)dY, -127, 127);
}

// ============== WYSYŁANIE DANYCH MYSZY (USB / BLE) =================

/**
 * Normalny tryb: wysyła ruch kursora.
 * Tryb scrolla:  ruch góra/dół → kółko myszy (scroll).
 */
void wyslijRuchMyszy(int dx, int dy) {
    if (trybScrolla) {
        int scroll = constrain(-dy / DZIELNIK_SCROLLA, -5, 5);
        if (scroll == 0) return;
        if (czyUSBPodlaczone())         usbMysz.move(0, 0, (int8_t)scroll);
        else if (bleMysz.isConnected()) bleMysz.move(0, 0, (int8_t)scroll);
    } else {
        if (czyUSBPodlaczone())         usbMysz.move((int8_t)dx, (int8_t)dy);
        else if (bleMysz.isConnected()) bleMysz.move((int8_t)dx, (int8_t)dy);
    }
}

/**
 * Kliknięcie wzorcem press → delay → release + błysk diody.
 */
void wyslijKlikniecie(uint8_t przycisk) {
    if (czyUSBPodlaczone()) {
        usbMysz.press(przycisk);
        delay(CZAS_KROTKIEGO_KLIKU_MS);
        usbMysz.release(przycisk);
    } else if (bleMysz.isConnected()) {
        bleMysz.press(przycisk);
        delay(CZAS_KROTKIEGO_KLIKU_MS);
        bleMysz.release(przycisk);
    }
    ustawBlyskLed(CZAS_BLYSKU_LED_MS);
}

/**
 * Obsługa przytrzymania lewego przycisku (drag & drop).
 * wcisnij=true  → press,  dioda świeci ciągle.
 * wcisnij=false → release, dioda gaśnie.
 */
void wyslijPrzytrzymanie(bool wcisnij) {
    if (wcisnij) {
        if (czyUSBPodlaczone())         usbMysz.press(MOUSE_LEFT);
        else if (bleMysz.isConnected()) bleMysz.press(MOUSE_LEFT);
        if (TRYB_DEBUG) Serial.println(F("[DRAG] Przytrzymanie ON"));
        webDebugLog("[DRAG] Przytrzymanie ON");
    } else {
        if (czyUSBPodlaczone())         usbMysz.release(MOUSE_LEFT);
        else if (bleMysz.isConnected()) bleMysz.release(MOUSE_LEFT);
        ustawBlyskLed(CZAS_BLYSKU_LED_MS);
        if (TRYB_DEBUG) Serial.println(F("[DRAG] Przytrzymanie OFF"));
        webDebugLog("[DRAG] Przytrzymanie OFF");
    }
}

// ============= PRZETWARZANIE ZLICZONYCH IMPULSÓW ===================

/**
 * Wywoływana po upływie okna wielokliku, gdy czujnik jest nieaktywny.
 *
 *   Tryb normalny:
 *     1 impuls  → lewy klik
 *     2 impulsy → prawy klik
 *     3 impulsy → WEJŚCIE w tryb scrolla (dioda ON)
 *     4+ impulsy → rekalibracja żyroskopu
 *
 *   Tryb scrolla:
 *     2 impulsy → WYJŚCIE z trybu scrolla (dioda OFF)
 *     inne → ignorowane (brak kliknięć w trybie scrolla)
 */
void przetworzImpulsy(uint8_t licznik) {
    if (trybScrolla) {
        if (licznik >= 2) {
            trybScrolla = false;
            ustawBlyskLed(CZAS_BLYSKU_LED_MS);
            if (TRYB_DEBUG) Serial.println(F("[SCROLL] OFF"));
            webDebugLog("[SCROLL] OFF");
        }
        return;
    }

    if (licznik >= 4) {
        if (TRYB_DEBUG) Serial.println(F("[REKALIBRACJA] Start..."));
        webDebugLog("[REKALIBRACJA] Start (4 mrugniecia)...");
        kalibracjaZyroskopu();
        mrugnijDioda(3, 200);
        if (TRYB_DEBUG) Serial.println(F("[REKALIBRACJA] Gotowe."));
        webDebugLog("[REKALIBRACJA] Gotowe.");
    } else if (licznik >= 3) {
        trybScrolla = true;
        if (TRYB_DEBUG) Serial.println(F("[SCROLL] ON"));
        webDebugLog("[SCROLL] ON");
    } else if (licznik == 2) {
        wyslijKlikniecie(MOUSE_RIGHT);
        licznikKlikPrawych++;
        if (TRYB_DEBUG) Serial.println(F("[KLIK] PRAWY"));
        webDebugLog("[KLIK] PRAWY");
    } else if (licznik == 1) {
        wyslijKlikniecie(MOUSE_LEFT);
        licznikKlikLewych++;
        if (TRYB_DEBUG) Serial.println(F("[KLIK] LEWY"));
        webDebugLog("[KLIK] LEWY");
    }
}

// ================== DETEKCJA KLIKNIĘĆ (LM393) =====================

/**
 * Niblokująca maszyna stanów obsługująca czujnik optyczny.
 *
 * Detekcja oparta na zliczaniu ZAKOŃCZONYCH impulsów (zbocze
 * opadające = oko otwiera się). Dzięki temu przed zliczeniem
 * można sprawdzić czas trwania impulsu:
 *
 *   - krótki impuls (<600ms) → zliczany jako kliknięcie,
 *   - długi impuls (≥600ms)  → przytrzymanie lewego przycisku
 *     (drag), zwolniony przy otwarciu oka.
 *
 * Po upływie OKNO_WIELOKLIKU_MS bez nowego impulsu zliczone
 * impulsy przetwarzane są przez przetworzImpulsy().
 */
void obsluzKlikniecia() {
    bool odczyt = digitalRead(PIN_LM393);
    unsigned long teraz = millis();

    // --- Debounce ---
    if (odczyt != poprzedniOdczytCzujnika) {
        czasOstatniegoDebounce = teraz;
    }
    poprzedniOdczytCzujnika = odczyt;

    if ((teraz - czasOstatniegoDebounce) < CZAS_DEBOUNCE_MS) return;

    bool aktywny    = (odczyt == LM393_AKTYWNY_STAN);
    bool bylAktywny = (ostatniStabilnyOdczyt == LM393_AKTYWNY_STAN);

    // --- Zbocze narastające: czujnik staje się aktywny (oko zamyka się) ---
    if (aktywny && !bylAktywny) {
        ostatniStabilnyOdczyt = odczyt;
        czasStartImpulsu = teraz;
        return;
    }

    // --- Czujnik ciągle aktywny: sprawdź próg przytrzymania ---
    if (aktywny && bylAktywny && !przytrzymanieAktywne && !trybScrolla) {
        if ((teraz - czasStartImpulsu) >= PROG_PRZYTRZYMANIA_MS) {
            przytrzymanieAktywne = true;
            wyslijPrzytrzymanie(true);
            licznikImpulsow = 0;
        }
    }

    // --- Zbocze opadające: czujnik staje się nieaktywny (oko otwiera się) ---
    if (!aktywny && bylAktywny) {
        ostatniStabilnyOdczyt = odczyt;

        if (przytrzymanieAktywne) {
            przytrzymanieAktywne = false;
            wyslijPrzytrzymanie(false);
            return;
        }

        if (licznikImpulsow > 0 &&
            (teraz - czasOstatniegoImpulsu) <= OKNO_WIELOKLIKU_MS) {
            licznikImpulsow++;
        } else {
            licznikImpulsow = 1;
        }
        czasOstatniegoImpulsu = teraz;
    }

    // --- Timeout: okno wielokliku upłynęło → przetwórz impulsy ---
    if (licznikImpulsow > 0 && !aktywny &&
        (teraz - czasOstatniegoImpulsu) > OKNO_WIELOKLIKU_MS) {
        przetworzImpulsy(licznikImpulsow);
        licznikImpulsow = 0;
    }
}

// ======================== DIAGNOSTYKA ==============================

void diagnostyka() {
    if (!TRYB_DEBUG) return;

    unsigned long teraz = millis();
    if ((teraz - ostatniCzasDiagnostyki) < OKRES_DIAGNOSTYKI_MS) return;
    ostatniCzasDiagnostyki = teraz;

    if (kursorDeltaX == 0 && kursorDeltaY == 0) return;

    String msg = trybScrolla ? "[SCROLL] " : "[RUCH] ";
    msg += "dX="; msg += kursorDeltaX;
    msg += "  dY="; msg += kursorDeltaY;
    msg += "  ["; msg += czyUSBPodlaczone() ? "USB" : "BLE"; msg += "]";

    Serial.println(msg);
    webDebugLog(msg);
}

// ============================ SETUP ================================

void setup() {
    Serial.begin(PREDKOSC_SERIAL);

    pinMode(PIN_LM393,   INPUT_PULLUP);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    if (TRYB_DEBUG) {
        Serial.println();
        Serial.println(F("============================================"));
        Serial.println(F("  HEFAS – Air Mouse  |  Inicjalizacja..."));
        Serial.println(F("============================================"));
    }

    Wire.begin(PIN_SDA, PIN_SCL);
    czujnikIMU.initialize();

    if (!czujnikIMU.testConnection()) {
        if (TRYB_DEBUG) Serial.println(F("[BLAD] MPU6050 brak odpowiedzi!"));
        while (true) { mrugnijDioda(5, 100); delay(500); }
    }
    if (TRYB_DEBUG) Serial.println(F("[OK] MPU6050 polaczony."));

    kalibracjaZyroskopu();

    usbMysz.begin();
    USB.begin();
    if (TRYB_DEBUG) Serial.println(F("[OK] USB HID Mouse."));

    bleMysz.begin();
    if (TRYB_DEBUG) Serial.println(F("[OK] BLE Mouse."));

    webDebugInit();

    mrugnijDioda(3, 200);

    if (TRYB_DEBUG) {
        Serial.println(F("============================================"));
        Serial.println(F("  HEFAS GOTOWY"));
        Serial.println(F("  1 mrug  = lewy klik"));
        Serial.println(F("  2 mrug  = prawy klik"));
        Serial.println(F("  3 mrug  = tryb scrolla (LED ON)"));
        Serial.println(F("  2 mrug  = wyjscie ze scrolla (LED OFF)"));
        Serial.println(F("  4 mrug  = rekalibracja zyroskopu"));
        Serial.println(F("  dlugie  = przytrzymanie (drag)"));
        Serial.println(F("============================================"));
    }
}

// ======================== PĘTLA GŁÓWNA =============================

void loop() {
    webDebugLoop();

#if WEBDEBUG_AKTYWNY
    if (webZadanieRekalibracji) {
        webZadanieRekalibracji = false;
        webDebugLog("[REKALIBRACJA] Start z WWW...");
        kalibracjaZyroskopu();
        mrugnijDioda(3, 200);
        webDebugLog("[REKALIBRACJA] Gotowe.");
    }
#endif

    odczytajIMU();

#if WEBDEBUG_AKTYWNY
    if (!webPauzaMyszy) {
#endif
        obsluzKlikniecia();
        if (kursorDeltaX != 0 || kursorDeltaY != 0) {
            wyslijRuchMyszy(kursorDeltaX, kursorDeltaY);
        }
#if WEBDEBUG_AKTYWNY
    }
#endif

    odswiezLed();
    diagnostyka();
    delay(OKRES_PETLI_MS);
}

/**
 * ============================================================
 *  RAPORT KOŃCOWY
 * ============================================================
 *
 *  include/hefas_config.h
 *    Piny, czułość, progi filtracji, parametry kliknięć/scrolla.
 *
 *  src/main.cpp  (ten plik)
 *    - autokalibracja żyroskopu (200 próbek),
 *    - mapowanie: gx→X, gz→Y (empiryczne),
 *    - podwójna filtracja (próg szumu + strefa martwa),
 *    - 1 mrugnięcie = lewy klik,
 *    - 2 mrugnięcia = prawy klik,
 *    - 3 mrugnięcia = tryb scrolla (dioda świeci ciągle),
 *    - 2 mrugnięcia w trybie scrolla = wyjście,
 *    - długie zamknięcie oka (>600ms) = przytrzymanie lewego
 *      przycisku (drag & drop), dioda świeci do puszczenia,
 *    - press/release zamiast click(),
 *    - USB HID > BLE (automatyczny priorytet).
 *
 * ============================================================
 */
