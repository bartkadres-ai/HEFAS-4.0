/**
 * ============================================================
 *  HEFAS – Head-controlled Electronic Functional Assistive System
 * ============================================================
 *  Platforma:   Seeed Studio XIAO ESP32-S3 Plus
 *  Czujnik IMU: MPU6050 (6-DoF: akcelerometr + żyroskop)
 *  Detektor:    TCRT5000 + LM393 (czujnik optyczny, Active LOW)
 *  Komunikacja: USB HID (priorytet) / Bluetooth Low Energy
 *
 *  Plik realizuje pełną logikę Air Mouse sterowanej ruchami głowy:
 *  - odczyt danych inercyjnych z żyroskopu MPU6050,
 *  - filtracja (strefa martwa + próg szumu) eliminująca dryf i drżenie,
 *  - mapowanie: Yaw głowy → oś X kursora, Pitch → oś Y kursora,
 *  - wykrywanie kliknięć mrugnięciem/ruchem policzka przez czujnik optyczny,
 *  - dualna komunikacja USB HID / BLE z automatycznym priorytetem USB,
 *  - autokalibracja żyroskopu przy starcie (200 próbek),
 *  - tryb diagnostyczny przez Serial Monitor (115200 baud).
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

extern "C" bool tud_mounted(void);

// ======================== OBIEKTY GLOBALNE ==========================

MPU6050            czujnikIMU;
BleMouse           bleMysz("HEFAS", "HEFAS Team", 100);
USBHIDMouse        usbMysz;

// ===================== ZMIENNE KALIBRACJI ===========================

float offsetGx = 0.0f;
float offsetGy = 0.0f;
float offsetGz = 0.0f;

// ==================== ZMIENNE RUCHU KURSORA =========================

int kursorDeltaX = 0;
int kursorDeltaY = 0;

// =============== MASZYNA STANÓW DETEKCJI KLIKNIĘĆ ===================

enum StanKlikniecia { BEZCZYNNY, OCZEKIWANIE_NA_DWUKLIK };

StanKlikniecia stanKlikniecia         = BEZCZYNNY;
bool           poprzedniOdczytCzujnika = HIGH;
bool           ostatniStabilnyOdczyt   = HIGH;
unsigned long  czasOstatniegoDebounce  = 0;
unsigned long  czasPierwszegoImpulsu   = 0;

// =============== NIBLOKUJĄCY BŁYSK DIODY WBUDOWANEJ ================
// Wzorzec z poprzedniego projektu: dioda świeci się przez
// CZAS_BLYSKU_LED_MS po każdym kliknięciu, gaszona w pętli głównej.

unsigned long koniecBlyskuLedMs = 0;

// =================== ZMIENNE DIAGNOSTYKI ============================

unsigned long ostatniCzasDiagnostyki  = 0;

// ===================== FUNKCJE POMOCNICZE ===========================

/**
 * Ustawia czas zakończenia błysku wbudowanej diody.
 * Niblokujące – dioda jest gaszona w odswiezLed().
 */
void ustawBlyskLed(uint32_t czasMs) {
    koniecBlyskuLedMs = millis() + czasMs;
}

/**
 * Odświeża stan wbudowanej diody LED. Jeśli czas błysku jeszcze
 * nie upłynął – dioda świeci. Po upłynięciu – gaśnie.
 * Wywoływana co iterację pętli głównej.
 */
void odswiezLed() {
    digitalWrite(LED_BUILTIN, (millis() < koniecBlyskuLedMs) ? HIGH : LOW);
}

/**
 * Sygnalizacja wizualna diodą wbudowaną (blokująca).
 * Używana TYLKO przy starcie systemu (kalibracja, gotowość).
 */
void mrugnijDioda(int ileMrugniecie, int czasMs) {
    for (int i = 0; i < ileMrugniecie; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(czasMs);
        digitalWrite(LED_BUILTIN, LOW);
        delay(czasMs);
    }
}

bool czyUSBPodlaczone() {
    return tud_mounted();
}

// =================== KALIBRACJA ŻYROSKOPU ==========================

/**
 * Autokalibracja żyroskopu MPU6050.
 *
 * Pobiera PROBKI_KALIBRACJI odczytów przy nieruchomym czujniku,
 * uśrednia je i zapamiętuje jako offsety. Każdy kolejny odczyt
 * jest pomniejszany o te offsety, co eliminuje bias czujnika.
 *
 * WAŻNE: Podczas kalibracji urządzenie MUSI leżeć nieruchomo!
 */
void kalibracjaZyroskopu() {
    if (TRYB_DEBUG) {
        Serial.println(F("[KALIBRACJA] Rozpoczynam – nie ruszaj glowa..."));
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
        Serial.print(F("[KALIBRACJA] Offsety: Gx="));
        Serial.print(offsetGx, 1);
        Serial.print(F("  Gy="));
        Serial.print(offsetGy, 1);
        Serial.print(F("  Gz="));
        Serial.println(offsetGz, 1);
        Serial.println(F("[KALIBRACJA] Zakonczona pomyslnie."));
    }
}

// ================ ODCZYT I FILTRACJA DANYCH IMU ====================

/**
 * Odczytuje surowe dane z żyroskopu, odejmuje offsety kalibracyjne,
 * przelicza na °/s, a następnie stosuje podwójną filtrację:
 *
 *   1) PROG_ZYROSKOPU – zeruje szum czujnika poniżej progu
 *   2) STREFA_MARTWA  – zeruje mikroruchy poniżej minimum
 *
 * Mapowanie osi (ustalono empirycznie dla montażu na czole):
 *   oś X żyroskopu (gx) → oś X kursora (Yaw – lewo/prawo)
 *   oś Z żyroskopu (gz) → oś Y kursora (Pitch – góra/dół)
 *
 * Mnożniki ODWROC_OS_X / ODWROC_OS_Y w hefas_config.h pozwalają
 * odwrócić kierunek bez przebudowy kodu (ustaw na +1 lub -1).
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
 * Wysyła wektor ruchu kursora przez kanał o wyższym priorytecie.
 * USB HID ma pierwszeństwo – jeśli kabel jest podłączony do hosta,
 * dane lecą natywnym HID. W przeciwnym razie – Bluetooth LE.
 */
void wyslijRuchMyszy(int dx, int dy) {
    if (czyUSBPodlaczone()) {
        usbMysz.move((int8_t)dx, (int8_t)dy);
    } else if (bleMysz.isConnected()) {
        bleMysz.move((int8_t)dx, (int8_t)dy);
    }
}

/**
 * Wysyła kliknięcie przycisku myszy wzorcem press → delay → release,
 * co daje bardziej niezawodną detekcję po stronie systemu operacyjnego
 * niż pojedyncze click(). Jednocześnie wyzwala krótki błysk
 * wbudowanej diody LED jako informację zwrotną dla użytkownika.
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

    if (TRYB_DEBUG) {
        Serial.print(F("[KLIK] >>> "));
        Serial.print(przycisk == MOUSE_LEFT ? F("LEWY") : F("PRAWY"));
        Serial.println(F(" KLIK <<<"));
    }
}

// ================== DETEKCJA KLIKNIĘĆ (LM393) =====================

/**
 * Niblokująca maszyna stanów obsługująca sygnały z czujnika
 * optycznego TCRT5000 + LM393.
 *
 * Czujnik pracuje w logice Active LOW:
 *   - stan HIGH = brak detekcji (spoczynek)
 *   - stan LOW  = wykryto impuls (mrugnięcie / ruch policzka)
 *
 * Algorytm detekcji:
 *   1. Wykrycie stabilnego zbocza opadającego (HIGH → LOW)
 *      po odczekaniu CZAS_DEBOUNCE_MS (eliminacja szumów).
 *   2. Jeśli w ciągu OKNO_DWUKLIKU_MS pojawi się drugi impuls
 *      → PRAWY KLIK (MOUSE_RIGHT).
 *   3. Jeśli okno minie bez drugiego impulsu
 *      → LEWY KLIK (MOUSE_LEFT).
 */
void obsluzKlikniecia() {
    bool odczytCzujnika = digitalRead(PIN_LM393);
    unsigned long teraz = millis();

    if (odczytCzujnika != poprzedniOdczytCzujnika) {
        czasOstatniegoDebounce = teraz;
    }
    poprzedniOdczytCzujnika = odczytCzujnika;

    if ((teraz - czasOstatniegoDebounce) < CZAS_DEBOUNCE_MS) {
        return;
    }

    bool impulsAktywny = (odczytCzujnika == LM393_AKTYWNY_STAN);
    bool poprzednioAktywny = (ostatniStabilnyOdczyt == LM393_AKTYWNY_STAN);
    bool zboczeNarastajace = (impulsAktywny && !poprzednioAktywny);

    ostatniStabilnyOdczyt = odczytCzujnika;

    if (zboczeNarastajace) {
        switch (stanKlikniecia) {
            case BEZCZYNNY:
                stanKlikniecia        = OCZEKIWANIE_NA_DWUKLIK;
                czasPierwszegoImpulsu = teraz;
                if (TRYB_DEBUG) Serial.println(F("[KLIK] Impuls #1"));
                break;

            case OCZEKIWANIE_NA_DWUKLIK:
                if ((teraz - czasPierwszegoImpulsu) <= OKNO_DWUKLIKU_MS) {
                    wyslijKlikniecie(MOUSE_RIGHT);
                    stanKlikniecia = BEZCZYNNY;
                } else {
                    czasPierwszegoImpulsu = teraz;
                }
                break;
        }
    }

    if (stanKlikniecia == OCZEKIWANIE_NA_DWUKLIK &&
        (teraz - czasPierwszegoImpulsu) > OKNO_DWUKLIKU_MS) {
        wyslijKlikniecie(MOUSE_LEFT);
        stanKlikniecia = BEZCZYNNY;
    }
}

// ======================== DIAGNOSTYKA ==============================

void diagnostyka() {
    if (!TRYB_DEBUG) return;

    unsigned long teraz = millis();
    if ((teraz - ostatniCzasDiagnostyki) < OKRES_DIAGNOSTYKI_MS) return;
    ostatniCzasDiagnostyki = teraz;

    if (kursorDeltaX == 0 && kursorDeltaY == 0) return;

    Serial.print(F("[RUCH] dX="));
    Serial.print(kursorDeltaX);
    Serial.print(F("  dY="));
    Serial.print(kursorDeltaY);
    Serial.print(F("  ["));
    Serial.print(czyUSBPodlaczone() ? F("USB") : F("BLE"));
    Serial.println(F("]"));
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
        if (TRYB_DEBUG) {
            Serial.println(F("[BLAD] MPU6050 nie odpowiada na 0x68!"));
            Serial.println(F("[BLAD] Sprawdz polaczenia SDA/SCL."));
        }
        while (true) {
            mrugnijDioda(5, 100);
            delay(500);
        }
    }
    if (TRYB_DEBUG) Serial.println(F("[OK] MPU6050 polaczony."));

    kalibracjaZyroskopu();

    usbMysz.begin();
    USB.begin();
    if (TRYB_DEBUG) Serial.println(F("[OK] USB HID Mouse zarejestrowany."));

    bleMysz.begin();
    if (TRYB_DEBUG) Serial.println(F("[OK] BLE Mouse – oglaszanie aktywne."));

    mrugnijDioda(3, 200);

    if (TRYB_DEBUG) {
        Serial.println(F("============================================"));
        Serial.println(F("  HEFAS GOTOWY – priorytet: USB > BLE"));
        Serial.println(F("============================================"));
    }
}

// ======================== PĘTLA GŁÓWNA =============================

void loop() {
    odczytajIMU();

    obsluzKlikniecia();

    if (kursorDeltaX != 0 || kursorDeltaY != 0) {
        wyslijRuchMyszy(kursorDeltaX, kursorDeltaY);
    }

    odswiezLed();

    diagnostyka();

    delay(OKRES_PETLI_MS);
}

/**
 * ============================================================
 *  RAPORT KOŃCOWY
 * ============================================================
 *
 *  PLIKI PROJEKTU:
 *
 *  platformio.ini
 *    Konfiguracja środowiska PlatformIO: board XIAO ESP32-S3,
 *    flagi natywnego USB HID, biblioteki BLE-Mouse i MPU6050.
 *
 *  include/hefas_config.h
 *    Centralne repozytorium stałych konfiguracyjnych:
 *    piny I2C/LM393, czułość myszy, progi filtracji,
 *    parametry debounce/dwukliku, kierunki osi, flaga DEBUG.
 *    Jedyne miejsce, które trzeba edytować przy strojeniu.
 *
 *  src/main.cpp  (ten plik)
 *    Pełna logika systemu HEFAS:
 *    - autokalibracja żyroskopu (200 próbek, ~1 s),
 *    - odczyt i podwójna filtracja danych IMU,
 *    - mapowanie gx→X, gz→Y (empiryczne, pod montaż na czole),
 *    - maszyna stanów kliknięć (1 impuls = lewy, 2 = prawy),
 *    - press/release zamiast click (niezawodniejsze),
 *    - wbudowana dioda LED miga przy każdym kliknięciu,
 *    - dualna komunikacja USB HID / BLE z priorytetem USB,
 *    - diagnostyka przez Serial Monitor 115200 baud.
 *
 *  JAK ZACZĄĆ:
 *
 *  1. Podłącz MPU6050:  SDA → D4 (GPIO 5), SCL → D5 (GPIO 6).
 *  2. Podłącz LM393:   OUT → D0 (GPIO 1), zasilanie 3.3 V.
 *  3. Wgraj firmware:   PlatformIO → Upload (Ctrl+Alt+U).
 *  4. Po starcie nie ruszaj głową przez ~1 s (kalibracja).
 *  5. Dioda mrugnij 3× = system gotowy.
 *  6. Rusz głową – kursor powinien się poruszać.
 *  7. Mrugnij raz → lewy klik, dwa razy szybko → prawy klik.
 *  8. Jeśli kierunek jest odwrócony, zmień ODWROC_OS_X / _Y
 *     w hefas_config.h na +1 lub -1.
 *  9. Jeśli kursor drży, zwiększ STREFA_MARTWA lub PROG_ZYROSKOPU.
 *
 * ============================================================
 */
