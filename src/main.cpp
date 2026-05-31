/**
 * ============================================================
 *  HEFAS – Head-controlled Electronic Functional Assistive System
 * ============================================================
 *  Platforma:   Seeed Studio XIAO ESP32-S3 Plus
 *  Czujnik IMU: MPU6050 (6-DoF)
 *  Detektor:    TCRT5000 (wyjście analogowe AO → ADC, filtr EMA + histereza)
 *  Komunikacja: USB HID (priorytet) / Bluetooth Low Energy
 *
 *  Autorzy: Sebastian Sobczyk, Bartłomiej Adamczyk
 *  Kierunek: Mechatronika – Szczecin 2026
 * ============================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <algorithm>
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
BleMouse           bleMysz("HEFAS 4.0", "HEFAS Team", 100); // Mysz BLE (nazwa, producent, bateria%)
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

// =============== DETEKTOR TCRT5000 (SYGNAŁ ANALOGOWY) ================
// Stan wewnętrzny detektora analogowego. wirtualnyStanCzujnika pełni
// rolę dawnego digitalRead(PIN_LM393) — true = oko zamknięte (sygnał
// poniżej baseline o co najmniej OFFSET_TRIGGER, po przejściu histerezy).
// tcrtBaseline jest dynamicznie aktualizowany przy długiej bezczynności,
// kompensując zmiany oświetlenia otoczenia.

float          tcrtBaseline                = 0.0f;   // Punkt odniesienia (otwarte oko)
float          tcrtFiltered                = 0.0f;   // Sygnał po filtrze EMA
int            tcrtRaw                     = 0;      // Ostatni surowy odczyt ADC
bool           wirtualnyStanCzujnika       = false;  // true = oko zamknięte
unsigned long  czasOstatniejAktywnosciTCRT = 0;      // Do gating'u trackingu tła

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

// ================== MONITOR BATERII (D16 = GPIO10) =================
// Stan ostatniego pomiaru baterii. Aktualizowany przez
// odczytajPoziomBaterii() co OKRES_POMIARU_BATERII_MS.
// bateriaPodlaczona = false gdy napięcie ogniwa < V_BATERIA_BRAK_PROG
// (ogniwo odłączone, zła polaryzacja lub błędne podłączenie +/−).

bool           bateriaPodlaczona         = false;
float          bateriaNapiecie           = 0.0f;   // [V] po przeliczeniu dzielnika
uint8_t        bateriaProcent            = 0;      // 0-100
int            bateriaRawAdc             = 0;      // napięcie na pinie ADC [mV] (trimmed-mean)
int            bateriaRawCounts          = 0;      // surowy ADC 12-bit (diagnostyka)
unsigned long  ostatniCzasPomiaruBaterii = 0;

// Globalna flaga runtime sterująca wypisywaniem logów na Serial.
// >>> TYMCZASOWO WŁĄCZONA NA POTRZEBY TESTÓW <<<
// Po dostrojeniu progów TCRT przywróć wartość `false` — wtedy debug
// będzie wyłączony po starcie, a przełączać go będzie sekwencja 6 mrugnięć.
bool czyDebugWlaczony = false;

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
    if (czyDebugWlaczony) {
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

    if (czyDebugWlaczony) {
        Serial.print(F("[KALIBRACJA] Gx="));
        Serial.print(offsetGx, 1);
        Serial.print(F("  Gy="));
        Serial.print(offsetGy, 1);
        Serial.print(F("  Gz="));
        Serial.println(offsetGz, 1);
    }
}

// ================== KALIBRACJA TCRT5000 ============================

/**
 * Wyznaczenie linii bazowej sygnału analogowego TCRT5000 dla stanu
 * "oko otwarte". Procedura:
 *   1. Włącza 12-bitową rozdzielczość ADC ESP32-S3 (zakres 0–4095).
 *   2. Czeka 3 s, aby użytkownik mógł poprawnie założyć oprawki
 *      i ustawić oko w pozycji neutralnej (otwarte, patrzy w przód).
 *   3. Pobiera PROBKI_TCRT_KALIBRACJI próbek w odstępach 10 ms.
 *   4. Sortuje próbki rosnąco i odrzuca po 10% z dołu i z góry
 *      (trimmed-mean) — eliminuje artefakty pomiarowe i mikroruchy.
 *   5. Średnia z pozostałych 80% próbek staje się baseline.
 *   6. Inicjalizuje filtr EMA wartością baseline (brak skoku startowego).
 *
 * Sanity-check: jeśli baseline wypadnie poza sensownym zakresem
 * (czujnik niepodłączony / oświetlony bezpośrednio słońcem), system
 * sygnalizuje to ostrzeżeniem, ale kontynuuje pracę.
 */
void kalibracjaTCRT5000() {
    analogReadResolution(12);

    if (czyDebugWlaczony) {
        Serial.println(F("[TCRT] Kalibracja - trzymaj oko OTWARTE, nie ruszaj glowa..."));
    }
    webDebugLog("[TCRT] Kalibracja - 3 sekundy stabilizacji...");

    // LED świeci ciągle przez cały czas trwania kalibracji TCRT
    // — sygnał dla użytkownika, że system pracuje (a nie się zawiesił).
    digitalWrite(LED_BUILTIN, HIGH);

    delay(3000);

    int probki[PROBKI_TCRT_KALIBRACJI];
    for (int i = 0; i < PROBKI_TCRT_KALIBRACJI; i++) {
        probki[i] = analogRead(PIN_TCRT_ANALOG);
        delay(10);
    }

    digitalWrite(LED_BUILTIN, LOW);

    std::sort(probki, probki + PROBKI_TCRT_KALIBRACJI);

    int odrzuc  = PROBKI_TCRT_KALIBRACJI / 10;       // 10% z każdej strony
    int liczba  = PROBKI_TCRT_KALIBRACJI - 2 * odrzuc;
    long suma   = 0;
    for (int i = odrzuc; i < PROBKI_TCRT_KALIBRACJI - odrzuc; i++) {
        suma += probki[i];
    }

    tcrtBaseline                = (float)suma / (float)liczba;
    tcrtFiltered                = tcrtBaseline;
    wirtualnyStanCzujnika       = false;
    czasOstatniejAktywnosciTCRT = millis();

    if (czyDebugWlaczony) {
        Serial.print(F("[TCRT] Baseline = "));
        Serial.println(tcrtBaseline, 1);
    }
    webDebugLog(String("[TCRT] Baseline = ") + (int)tcrtBaseline);

    if (tcrtBaseline < 100.0f || tcrtBaseline > 3900.0f) {
        if (czyDebugWlaczony) {
            Serial.println(F("[TCRT] OSTRZEZENIE: baseline poza zakresem - sprawdz montaz czujnika"));
        }
        webDebugLog("[TCRT] OSTRZEZENIE: baseline poza zakresem");
        mrugnijDioda(5, 100);
    }
}

// ============== KALIBRACJA SYSTEMU (STARTOWA, RÓWNOLEGŁA) ============

/**
 * Pojedyncza, równoległa kalibracja MPU6050 + TCRT5000.
 * Wywoływana raz w setup() — łączy dwa pomiary w jedną pętlę,
 * skracając czas startu z ~7 s do ~2 s i dając użytkownikowi
 * jeden, czytelny sygnał LED (ciągłe świecenie podczas kalibracji).
 *
 * W każdej z 200 iteracji (co 10 ms):
 *   - 1 próbka żyroskopu (gx, gy, gz) → uśredniana do offsetów,
 *   - 1 próbka TCRT5000 (ADC) → zapisywana do tablicy
 *     do późniejszego trimmed-mean.
 *
 * Łączny czas: 200 × 10 ms = 2 s. W tym czasie żyroskop dostaje
 * dłuższy okres uśredniania niż w pojedynczej kalibracjaZyroskopu()
 * (2 s zamiast 1 s) → lepsza redukcja niskoczęstotliwościowych
 * zaburzeń biasu, kosztem dwukrotnie rzadszego próbkowania
 * (100 Hz vs 200 Hz) — bez wpływu na praktyczną dokładność.
 *
 * Funkcje kalibracjaZyroskopu() i kalibracjaTCRT5000() pozostają
 * dostępne dla rekalibracji w runtime (5 mrugnięć, przycisk WWW).
 */
void kalibracjaSystemu() {
    analogReadResolution(12);

    if (czyDebugWlaczony) {
        Serial.println(F("[KAL] Kalibracja startowa - trzymaj glowe i oko nieruchomo (~2 s)..."));
    }
    webDebugLog("[KAL] Kalibracja startowa MPU + TCRT (~2 s)...");

    // LED świeci ciągle — jeden, jednoznaczny sygnał "system pracuje"
    digitalWrite(LED_BUILTIN, HIGH);

    int32_t sumaGx = 0, sumaGy = 0, sumaGz = 0;
    int16_t ax, ay, az, gx, gy, gz;
    int     probkiTCRT[PROBKI_TCRT_KALIBRACJI];

    for (int i = 0; i < PROBKI_TCRT_KALIBRACJI; i++) {
        czujnikIMU.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
        sumaGx += gx;
        sumaGy += gy;
        sumaGz += gz;

        probkiTCRT[i] = analogRead(PIN_TCRT_ANALOG);

        delay(10);
    }

    digitalWrite(LED_BUILTIN, LOW);

    // --- Offsety żyroskopu (zwykła średnia arytmetyczna) ---
    offsetGx = (float)sumaGx / (float)PROBKI_TCRT_KALIBRACJI;
    offsetGy = (float)sumaGy / (float)PROBKI_TCRT_KALIBRACJI;
    offsetGz = (float)sumaGz / (float)PROBKI_TCRT_KALIBRACJI;

    // --- Baseline TCRT5000 (trimmed-mean 80%) ---
    std::sort(probkiTCRT, probkiTCRT + PROBKI_TCRT_KALIBRACJI);
    int odrzuc = PROBKI_TCRT_KALIBRACJI / 10;
    int liczba = PROBKI_TCRT_KALIBRACJI - 2 * odrzuc;
    long suma  = 0;
    for (int i = odrzuc; i < PROBKI_TCRT_KALIBRACJI - odrzuc; i++) {
        suma += probkiTCRT[i];
    }
    float baselineWstepny       = (float)suma / (float)liczba;
    tcrtBaseline                = baselineWstepny;
    tcrtFiltered                = baselineWstepny;
    wirtualnyStanCzujnika       = false;
    czasOstatniejAktywnosciTCRT = millis();

    // --- Stabilizacja termiczna diody IR (faza 2 kalibracji) ---
    // Dioda IR w TCRT5000 przez pierwsze 1-2 sekundy po włączeniu
    // nagrzewa się i jej jasność stabilizuje — powoduje to systematyczny
    // dryf sygnału o ~30-60 ADC. Uruchamiamy filtr EMA przez 100 dodatkowych
    // iteracji (~1s) z diodą już rozgrzaną, następnie ustawiamy baseline
    // na ustabilizowaną wartość filtru. LED miga wolno = faza stabilizacji.
    webDebugLog("[KAL] Stabilizacja IR (~1s)...");
    for (int i = 0; i < 100; i++) {
        int raw = analogRead(PIN_TCRT_ANALOG);
        tcrtFiltered = EMA_ALPHA * (float)raw + (1.0f - EMA_ALPHA) * tcrtFiltered;
        // Wolne mignięcia LED: ON przez pół iteracji, OFF przez pół
        digitalWrite(LED_BUILTIN, (i % 20 < 10) ? HIGH : LOW);
        delay(10);
    }
    digitalWrite(LED_BUILTIN, LOW);

    // Baseline końcowy = tcrtFiltered po stabilizacji — dokładniejszy
    // niż trimmed-mean z zimnej diody.
    tcrtBaseline                = tcrtFiltered;
    czasOstatniejAktywnosciTCRT = millis();

    // --- Logi diagnostyczne ---
    if (czyDebugWlaczony) {
        Serial.print(F("[KAL] Gx="));           Serial.print(offsetGx, 1);
        Serial.print(F("  Gy="));               Serial.print(offsetGy, 1);
        Serial.print(F("  Gz="));               Serial.print(offsetGz, 1);
        Serial.print(F("  | TCRT wst.="));      Serial.print(baselineWstepny, 1);
        Serial.print(F("  fin.="));             Serial.println(tcrtBaseline, 1);
    }
    webDebugLog(String("[KAL] Baseline TCRT: wst.=") + (int)baselineWstepny
                + " fin.=" + (int)tcrtBaseline);

    // --- Sanity check ---
    if (tcrtBaseline < 100.0f || tcrtBaseline > 3900.0f) {
        if (czyDebugWlaczony) {
            Serial.println(F("[KAL] OSTRZEZENIE: baseline TCRT poza zakresem - sprawdz czujnik"));
        }
        webDebugLog("[KAL] OSTRZEZENIE: baseline TCRT poza zakresem");
        mrugnijDioda(5, 100);
    }
}

// =========== AKTUALIZACJA DETEKTORA ANALOGOWEGO TCRT5000 ===========

/**
 * Wywoływana w każdej iteracji pętli głównej (~100 Hz).
 * Realizuje trzystopniowy łańcuch przetwarzania sygnału:
 *
 *   1. Filtr EMA (dolnoprzepustowy):
 *        filt(n) = α · raw(n) + (1−α) · filt(n−1)
 *      Wygładza szum kwantyzacji i fluktuacje światła otoczenia.
 *
 *   2. Komparator z histerezą (dwa progi):
 *        oko otwarte → zamknięte:  filt < baseline − OFFSET_TRIGGER
 *        oko zamknięte → otwarte:  filt > baseline − OFFSET_RELEASE
 *      OFFSET_TRIGGER > OFFSET_RELEASE zapobiega oscylacjom na granicy.
 *
 *   3. Adaptacyjny baseline (wolny EMA, bez timeoutu):
 *        tcrtBaseline aktualizowany przez EMA_ALPHA_WOLNY co cykl gdy oko otwarte.
 *        Stała czasowa ~2 s → baseline dogania zmiany oświetlenia/pozycji w ~3–5 s.
 *        Podczas mrugnięcia baseline zamrożony → nie ucieka pod sygnałem.
 */
void aktualizujDetektorTCRT() {
    tcrtRaw      = analogRead(PIN_TCRT_ANALOG);
    tcrtFiltered = EMA_ALPHA * (float)tcrtRaw + (1.0f - EMA_ALPHA) * tcrtFiltered;

    if (!wirtualnyStanCzujnika) {
        // Oko otwarte → szukamy mrugnięcia w oknie (TRIGGER, MAX_ZWARCIA).
        // Sygnał musi spaść wystarczająco głęboko (> OFFSET_TRIGGER) ale NIE
        // za głęboko (< baseline - OFFSET_MAX_ZWARCIA). Zbyt głęboki spadek
        // to zdarzenie mechaniczne (zdjęcie okularów, zasłonięcie czujnika) —
        // ignorujemy go, nie ustawiamy wirtualnyStanCzujnika.
        if (tcrtFiltered < (tcrtBaseline - OFFSET_TRIGGER) &&
            tcrtFiltered > (tcrtBaseline - OFFSET_MAX_ZWARCIA)) {
            wirtualnyStanCzujnika       = true;
            czasOstatniejAktywnosciTCRT = millis();
        }
    } else {
        // Oko zamknięte → dwa warunki wyjścia:
        //   1. Normalny: sygnał wrócił powyżej progu RELEASE → oko otwarte.
        //   2. Mechaniczny: sygnał spadł PONIŻEJ okna mrugnięcia (np. zdjęto
        //      okulary już po wykryciu zamknięcia) → resetujemy do "otwarte"
        //      żeby uniknąć fałszywego dragu.
        if (tcrtFiltered > (tcrtBaseline - OFFSET_RELEASE)) {
            wirtualnyStanCzujnika       = false;
            czasOstatniejAktywnosciTCRT = millis();
        } else if (tcrtFiltered < (tcrtBaseline - OFFSET_MAX_ZWARCIA)) {
            wirtualnyStanCzujnika       = false;
            czasOstatniejAktywnosciTCRT = millis();
            if (czyDebugWlaczony) {
                webDebugLog("[TCRT] Zdarzenie mechaniczne - reset (sygnal poza oknem)");
            }
        }
    }

    // Adaptacja baseline: ciągły wolny EMA działający wyłącznie gdy oko otwarte.
    // Nie ma timeoutu — baseline adaptuje się natychmiast (ale bardzo wolno,
    // EMA_ALPHA_WOLNY ≈ 0.005 = stała czasowa ~2 s @100 Hz).
    // Efekt: zmiana oświetlenia lub pozycji głowy → baseline dogania w ~3–5 s
    // bez żadnych false-clicks i bez ręcznego ustawiania timeoutów.
    // Mrugnięcie (oko zamknięte) → update wstrzymany → baseline nie ucieka.
    if (!wirtualnyStanCzujnika) {
        tcrtBaseline = EMA_ALPHA_WOLNY * tcrtFiltered + (1.0f - EMA_ALPHA_WOLNY) * tcrtBaseline;
    }
}

// =================== MONITOR BATERII (D16) =========================
//
// Pin D16 na płytce XIAO ESP32-S3 Plus = GPIO10 = ADC_BAT.
// Wewnętrzny dzielnik napięcia 1:11 (R9/R8) do padu BAT+ (Akyga 1900 mAh 1S, tylko +/−).
// Czytamy stąd napięcie ogniwa Li-Pol, mapujemy przez 8-punktową
// krzywą rozładowania na procent. Krzywa jest typowa dla 1S Li-Pol
// 3.0-4.2 V — pierwsze 80% pojemności rozłożone w wąskim oknie
// 3.65-4.20 V (charakterystyczne plateau Li-Pol), ostatnie 20%
// poniżej 3.55 V opada szybko.

/**
 * Mapowanie napięcia ogniwa Li-Pol 1S na procent naładowania.
 * Interpolacja liniowa między 8 punktami referencyjnymi krzywej.
 */
static uint8_t napiecieBateriiNaProcent(float v) {
    static const struct { float v; uint8_t p; } krzywa[] = {
        { 4.20f, 100 }, { 4.10f,  90 }, { 4.00f,  80 }, { 3.85f,  60 },
        { 3.75f,  40 }, { 3.65f,  20 }, { 3.50f,  10 }, { 3.30f,   0 }
    };
    const size_t N = sizeof(krzywa) / sizeof(krzywa[0]);

    if (v >= krzywa[0].v)     return 100;
    if (v <= krzywa[N-1].v)   return 0;

    for (size_t i = 1; i < N; ++i) {
        if (v >= krzywa[i].v) {
            float frac = (v - krzywa[i].v) / (krzywa[i-1].v - krzywa[i].v);
            float p = (float)krzywa[i].p + frac * (float)(krzywa[i-1].p - krzywa[i].p);
            return (uint8_t)(p + 0.5f);
        }
    }
    return 0;
}

/**
 * Inicjalizacja ADC baterii — wywoływać PO kalibracjaSystemu() i ponownie po webDebugInit()
 * (start WiFi może zresetować ustawienia ADC w niektórych wersjach core).
 * ADC_11db: zakres 0–~3100 mV — pin baterii (~330 mV) mieści się w liniowej części charakterystyki.
 */
static void initOdczytBaterii() {
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_ADC_BATERIA, ADC_11db);
    for (int i = 0; i < 16; ++i) {
        analogReadMilliVolts(PIN_ADC_BATERIA);
        delay(5);
    }
}

/**
 * Trimmed-mean napięcia na pinie GPIO10 [mV] + surowy ADC.
 * Atenuacja ADC_11db ustawiana tuż przed próbkowaniem.
 */
static int odczytPinBateriiMv(int* rawOut) {
    analogSetPinAttenuation(PIN_ADC_BATERIA, ADC_11db);

    int probkiMv[PROBKI_BATERII];
    int probkiRaw[PROBKI_BATERII];
    for (int i = 0; i < PROBKI_BATERII; ++i) {
        probkiRaw[i] = analogRead(PIN_ADC_BATERIA);
        probkiMv[i]  = analogReadMilliVolts(PIN_ADC_BATERIA);
        delayMicroseconds(300);
    }

    std::sort(probkiMv, probkiMv + PROBKI_BATERII);
    std::sort(probkiRaw, probkiRaw + PROBKI_BATERII);

    int odrzuc = PROBKI_BATERII / 10;
    int start  = odrzuc;
    int end    = PROBKI_BATERII - odrzuc;
    long sumaMv  = 0;
    long sumaRaw = 0;
    for (int i = start; i < end; ++i) {
        sumaMv  += probkiMv[i];
        sumaRaw += probkiRaw[i];
    }
    int liczba = end - start;
    if (rawOut) *rawOut = (int)(sumaRaw / liczba);
    return (int)(sumaMv / liczba);
}

/**
 * Pomiar napięcia baterii i wyliczenie stanu naładowania.
 *
 * Wywołanie wewnątrz loop() — funkcja sama dba o swój okres pracy
 * (OKRES_POMIARU_BATERII_MS), więc bezpiecznie wywoływać co iterację.
 * ADC1 jest niezależne od WiFi/BLE — odczyt jest stabilny.
 *
 * Używa analogReadMilliVolts() z ADC_11db + trimmed-mean (odrzuca 10% skrajnych próbek).
 * Pomiar działa zawsze — także przy USB; w logu dopisek (USB).
 */
void odczytajPoziomBaterii() {
    unsigned long teraz = millis();
    if ((teraz - ostatniCzasPomiaruBaterii) < OKRES_POMIARU_BATERII_MS) return;
    ostatniCzasPomiaruBaterii = teraz;

    bool usbPodlaczone = tud_mounted();

    int rawCounts = 0;
    bateriaRawAdc       = odczytPinBateriiMv(&rawCounts);
    bateriaRawCounts    = rawCounts;
    float vPin          = bateriaRawAdc / 1000.0f;
    bateriaNapiecie     = vPin * DZIELNIK_BATERII;

    bateriaPodlaczona = (bateriaNapiecie > V_BATERIA_BRAK_PROG);
    bateriaProcent    = bateriaPodlaczona ? napiecieBateriiNaProcent(bateriaNapiecie) : 0;

    // Log do WebDebug — dane do kalibracji dzielnika.
    // Kalibracja: DZIELNIK_BATERII = V_multimetr / (bateriaRawAdc / 1000.0)
    {
        String diagMsg = "[BAT] raw=";
        diagMsg += bateriaRawCounts;
        diagMsg += " pin=";
        diagMsg += bateriaRawAdc;
        diagMsg += "mV Vbat=";
        diagMsg += String(bateriaNapiecie, 2);
        diagMsg += "V";
        if (usbPodlaczone) diagMsg += " (USB)";
        if (bateriaPodlaczona) {
            diagMsg += " [";
            diagMsg += bateriaProcent;
            diagMsg += "%]";
        } else {
            diagMsg += " BRAK";
        }
        webDebugLog(diagMsg);
    }

    // BLE Battery Service (UUID 0x180F) — aktualizujemy tylko gdy procent realnie
    // się zmienił, żeby nie zaśmiecać BLE bezsensownymi notyfikacjami.
    static uint8_t ostatniBleProcent = 255;
    if (bateriaPodlaczona && bateriaProcent != ostatniBleProcent) {
        bleMysz.setBatteryLevel(bateriaProcent);
        ostatniBleProcent = bateriaProcent;
        if (czyDebugWlaczony) {
            String bleMsg = "[BLE] Battery Service = ";
            bleMsg += bateriaProcent;
            bleMsg += "%";
            Serial.println(bleMsg);
            webDebugLog(bleMsg);
        }
    }

    if (czyDebugWlaczony) {
        String msg = "[BAT] raw=";
        msg += bateriaRawCounts;
        msg += " pin=";
        msg += bateriaRawAdc;
        msg += "mV V=";
        msg += String(bateriaNapiecie, 2);
        if (usbPodlaczone) msg += " (USB)";
        if (bateriaPodlaczona) {
            msg += " [";
            msg += bateriaProcent;
            msg += "%]";
        } else {
            msg += " BRAK";
        }
        Serial.println(msg);
        webDebugLog(msg);
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
 * Podwójne kliknięcie lewym przyciskiem (double-click LPM).
 * Realizowane jako dwa szybkie press/release z odstępem
 * CZAS_MIEDZY_KLIKAMI_PODWOJNEGO_MS, mieszczącym się w domyślnym
 * progu double-click systemu Windows (500 ms).
 */
void wyslijPodwojnyKlikLewy() {
    if (czyUSBPodlaczone()) {
        usbMysz.press(MOUSE_LEFT);
        delay(CZAS_KROTKIEGO_KLIKU_MS);
        usbMysz.release(MOUSE_LEFT);
        delay(CZAS_MIEDZY_KLIKAMI_PODWOJNEGO_MS);
        usbMysz.press(MOUSE_LEFT);
        delay(CZAS_KROTKIEGO_KLIKU_MS);
        usbMysz.release(MOUSE_LEFT);
    } else if (bleMysz.isConnected()) {
        bleMysz.press(MOUSE_LEFT);
        delay(CZAS_KROTKIEGO_KLIKU_MS);
        bleMysz.release(MOUSE_LEFT);
        delay(CZAS_MIEDZY_KLIKAMI_PODWOJNEGO_MS);
        bleMysz.press(MOUSE_LEFT);
        delay(CZAS_KROTKIEGO_KLIKU_MS);
        bleMysz.release(MOUSE_LEFT);
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
        if (czyDebugWlaczony) Serial.println(F("[DRAG] Przytrzymanie ON"));
        webDebugLog("[DRAG] Przytrzymanie ON");
    } else {
        if (czyUSBPodlaczone())         usbMysz.release(MOUSE_LEFT);
        else if (bleMysz.isConnected()) bleMysz.release(MOUSE_LEFT);
        ustawBlyskLed(CZAS_BLYSKU_LED_MS);
        if (czyDebugWlaczony) Serial.println(F("[DRAG] Przytrzymanie OFF"));
        webDebugLog("[DRAG] Przytrzymanie OFF");
    }
}

// ============= PRZETWARZANIE ZLICZONYCH IMPULSÓW ===================

/**
 * Wywoływana po upływie okna wielokliku, gdy czujnik jest nieaktywny.
 *
 * Mapowanie mrugnięć na akcje (HEFAS 4.0):
 *   1 mrug.  → lewy klik (LPM)
 *   2 mrug.  → podwójny lewy klik (double-click LPM, otwieranie plików)
 *   3 mrug.  → prawy klik (PPM)
 *   4 mrug.  → toggle trybu scrolla (LED ciągły gdy aktywny)
 *   5 mrug.  → rekalibracja żyroskopu (LED mruga 3×)
 *   6 mrug.  → toggle trybu debug (LED mruga 2×)
 *
 * Akcje 4–6 (sterowanie systemem) działają zawsze, niezależnie od stanu.
 * Akcje 1–3 (kliknięcia myszą) są zablokowane w trybie scrolla, aby
 * niezamierzone mrugnięcia nie generowały klików podczas scrollowania.
 */
void przetworzImpulsy(uint8_t licznik) {
    // --- 6 mrugnięć: przełączenie trybu debug (zawsze dostępne) ---
    if (licznik >= 6) {
        czyDebugWlaczony = !czyDebugWlaczony;
        if (czyDebugWlaczony) Serial.println(F("[DEBUG] WLACZONY"));
        webDebugLog(czyDebugWlaczony ? "[DEBUG] WLACZONY" : "[DEBUG] WYLACZONY");
        mrugnijDioda(2, 100);
        return;
    }

    // --- 5 mrugnięć: rekalibracja żyroskopu (zawsze dostępne) ---
    if (licznik == 5) {
        if (czyDebugWlaczony) Serial.println(F("[REKALIBRACJA] Start..."));
        webDebugLog("[REKALIBRACJA] Start (5 mrugniec)...");
        kalibracjaZyroskopu();
        mrugnijDioda(3, 200);
        if (czyDebugWlaczony) Serial.println(F("[REKALIBRACJA] Gotowe."));
        webDebugLog("[REKALIBRACJA] Gotowe.");
        return;
    }

    // --- 4 mrugnięcia: toggle trybu scrolla (zawsze dostępne) ---
    if (licznik == 4) {
        trybScrolla = !trybScrolla;
        ustawBlyskLed(CZAS_BLYSKU_LED_MS);
        if (czyDebugWlaczony) {
            Serial.println(trybScrolla ? F("[SCROLL] ON") : F("[SCROLL] OFF"));
        }
        webDebugLog(trybScrolla ? "[SCROLL] ON" : "[SCROLL] OFF");
        return;
    }

    // --- Kliknięcia (1–3) są blokowane w trybie scrolla ---
    if (trybScrolla) return;

    if (licznik == 3) {
        wyslijKlikniecie(MOUSE_RIGHT);
        licznikKlikPrawych++;
        if (czyDebugWlaczony) Serial.println(F("[KLIK] PRAWY"));
        webDebugLog("[KLIK] PRAWY");
    } else if (licznik == 2) {
        wyslijPodwojnyKlikLewy();
        licznikKlikLewych += 2;
        if (czyDebugWlaczony) Serial.println(F("[KLIK] DOUBLE LEWY"));
        webDebugLog("[KLIK] DOUBLE LEWY");
    } else if (licznik == 1) {
        wyslijKlikniecie(MOUSE_LEFT);
        licznikKlikLewych++;
        if (czyDebugWlaczony) Serial.println(F("[KLIK] LEWY"));
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
    // Wirtualny odczyt z detektora analogowego TCRT5000 zastępuje
    // dawne digitalRead(PIN_LM393). Mapowanie:
    //   wirtualnyStanCzujnika == true  → odczyt = LM393_AKTYWNY_STAN  (oko zamknięte)
    //   wirtualnyStanCzujnika == false → odczyt = !LM393_AKTYWNY_STAN (oko otwarte)
    // Cała poniższa maszyna stanów (debounce, zliczanie wieloklików,
    // detekcja przytrzymania) pozostaje niezmieniona.
    bool odczyt = wirtualnyStanCzujnika ? LM393_AKTYWNY_STAN : !LM393_AKTYWNY_STAN;
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

        // Filtr minimalnego czasu mrugnięcia: odrzucamy impulsy krótsze niż
        // CZAS_MIN_MRUG_MS. Ruchy głowy powodują krótkie artefakty sygnału
        // (<80ms), natomiast prawdziwe mrugnięcia trwają 100-400ms.
        // Dzięki temu fałszywe kliknięcia od ruchów głowy są ignorowane.
        unsigned long czasTrwania = teraz - czasStartImpulsu;
        if (czasTrwania < CZAS_MIN_MRUG_MS) {
            if (czyDebugWlaczony) {
                String msg = "[TCRT] Artefakt odrzucony (";
                msg += (int)czasTrwania;
                msg += "ms < min ";
                msg += CZAS_MIN_MRUG_MS;
                msg += "ms)";
                webDebugLog(msg);
            }
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
    if (!czyDebugWlaczony) return;

    unsigned long teraz = millis();
    if ((teraz - ostatniCzasDiagnostyki) < OKRES_DIAGNOSTYKI_MS) return;
    ostatniCzasDiagnostyki = teraz;

    // --- 1. Sygnał detektora TCRT5000 — wypisujemy zawsze, gdy debug ---
    String tcrtMsg = "[TCRT] Raw=";
    tcrtMsg += tcrtRaw;
    tcrtMsg += "  Filt=";
    tcrtMsg += (int)tcrtFiltered;
    tcrtMsg += "  Base=";
    tcrtMsg += (int)tcrtBaseline;
    tcrtMsg += wirtualnyStanCzujnika ? "  [ZAMKN]" : "  [OTW]";
    Serial.println(tcrtMsg);
    webDebugLog(tcrtMsg);

    // --- 2. Ruch myszy — tylko gdy nie jest zerowy ---
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

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    if (czyDebugWlaczony) {
        Serial.println();
        Serial.println(F("============================================"));
        Serial.println(F("  HEFAS 4.0 – Air Mouse  |  Inicjalizacja..."));
        Serial.println(F("============================================"));
    }

    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(400000);     // I²C Fast Mode 400 kHz (MPU6050 obsługuje)
    czujnikIMU.initialize();

    if (!czujnikIMU.testConnection()) {
        if (czyDebugWlaczony) Serial.println(F("[BLAD] MPU6050 brak odpowiedzi!"));
        while (true) { mrugnijDioda(5, 100); delay(500); }
    }
    if (czyDebugWlaczony) Serial.println(F("[OK] MPU6050 polaczony."));

    // Równoległa kalibracja żyroskopu + TCRT5000 (~2 s, LED świeci ciągle).
    // analogReadResolution(12) jest wywoływane wewnątrz kalibracjaSystemu() —
    // w niektórych wersjach Arduino-ESP32 resetuje atenuację wszystkich pinów ADC.
    // Dlatego ADC_0db dla pinu baterii ustawiamy DOPIERO PO kalibracji.
    kalibracjaSystemu();

    initOdczytBaterii();

    usbMysz.begin();
    USB.begin();
    if (czyDebugWlaczony) Serial.println(F("[OK] USB HID Mouse."));

    bleMysz.begin();
    if (czyDebugWlaczony) Serial.println(F("[OK] BLE Mouse."));

    webDebugInit();
    initOdczytBaterii();   // ponownie po starcie WiFi AP

    mrugnijDioda(3, 200);

    if (czyDebugWlaczony) {
        Serial.println(F("============================================"));
        Serial.println(F("  HEFAS 4.0 GOTOWY"));
        Serial.println(F("  1 mrug  = LPM"));
        Serial.println(F("  2 mrug  = double-click LPM"));
        Serial.println(F("  3 mrug  = PPM"));
        Serial.println(F("  4 mrug  = toggle scrolla (LED ON/OFF)"));
        Serial.println(F("  5 mrug  = rekalibracja zyroskopu"));
        Serial.println(F("  6 mrug  = toggle trybu debug"));
        Serial.println(F("  dlugie  = przytrzymanie (drag & drop)"));
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
    aktualizujDetektorTCRT();
    odczytajPoziomBaterii();

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
 *  RAPORT KOŃCOWY – HEFAS 4.0
 * ============================================================
 *
 *  include/hefas_config.h
 *    Piny, czułość, progi filtracji, parametry mrugnięć/scrolla,
 *    parametry detektora analogowego TCRT5000.
 *
 *  src/main.cpp  (ten plik)
 *    - autokalibracja żyroskopu (200 próbek),
 *    - mapowanie: gx→X, gz→Y (empiryczne),
 *    - podwójna filtracja (próg szumu + strefa martwa),
 *    - detektor TCRT5000: analogReadResolution(12) + EMA + histereza
 *      + pływające tło (kompensacja dryfu optycznego),
 *    - kalibracja TCRT trimmed-mean (odrzut 10% skrajnych próbek),
 *    - mapowanie mrugnięć:
 *        1 mrug. = lewy klik (LPM),
 *        2 mrug. = double-click LPM (otwieranie plików),
 *        3 mrug. = prawy klik (PPM),
 *        4 mrug. = toggle trybu scrolla (dioda świeci ciągle),
 *        5 mrug. = rekalibracja żyroskopu (dioda mruga 3×),
 *        6 mrug. = toggle trybu debug (dioda mruga 2×),
 *    - długie zamknięcie oka (≥PROG_PRZYTRZYMANIA_MS) = drag & drop,
 *    - press/release zamiast click(),
 *    - runtime'owa flaga czyDebugWlaczony – domyślnie OFF (mniejsze
 *      zużycie energii), włączana sekwencją 6 mrugnięć,
 *    - USB HID > BLE (automatyczny priorytet).
 *
 * ============================================================
 */
