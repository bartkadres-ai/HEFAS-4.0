/**
 * ============================================================
 *  HEFAS – Plik konfiguracyjny sprzętu i parametrów strojenia
 * ============================================================
 *  Wszystkie stałe sterujące zachowaniem systemu zebrane w jednym
 *  miejscu, aby umożliwić szybkie dostrajanie bez modyfikacji logiki.
 */

#ifndef HEFAS_CONFIG_H
#define HEFAS_CONFIG_H

// ========================== PINY SPRZĘTOWE ==========================
#define PIN_SDA                5      // D4 (GPIO 5) – linia danych I2C
#define PIN_SCL                6      // D5 (GPIO 6) – linia zegara I2C

// Wyjście AO (analogowe) modułu TCRT5000 podłączone do ADC.
// Wyjście DO (cyfrowe) z komparatora LM393 NIE jest już używane —
// detekcja oparta jest w pełni o pomiar analogowy + filtrację EMA
// + komparator z histerezą + pływające tło (patrz aktualizujDetektorTCRT).
#define PIN_TCRT_ANALOG        A1     // D1 (GPIO 2) – wejście ADC dla AO TCRT5000

// Semantyka stanu wirtualnego dla maszyny stanów wieloklików:
// "oko zamknięte" jest mapowane na LM393_AKTYWNY_STAN. Zachowane
// dla kompatybilności z istniejącą logiką obsluzKlikniecia(),
// która historycznie operowała na cyfrowym sygnale Active LOW.
#define LM393_AKTYWNY_STAN     LOW

// LED_BUILTIN jest zdefiniowany przez board XIAO ESP32-S3 (GPIO 21).

// =================== PARAMETRY CZUŁOŚCI I FILTRACJI =================
#define CZULOSC_MYSZY          0.4f   // Skalowanie ruchu (0.2–1.0)
#define STREFA_MARTWA          2.0f   // Dead zone [°/s]
#define PROG_ZYROSKOPU         1.5f   // Próg szumu żyroskopu [°/s]
#define PROBKI_KALIBRACJI      200    // Próbki kalibracji
#define CZULOSC_ZYRO_LSB       131.0f // LSB/(°/s) dla ±250°/s

// =================== PARAMETRY DETEKCJI MRUGNIĘĆ =====================
// Strojenie czasów detekcji — kompromis między responsywnością
// pojedynczego LPM (krótkie okno = mniejsze opóźnienie) a wygodą
// długich serii do 6 mrugnięć (toggle debug). Wartości delikatnie
// zwiększone po testach na żywym użytkowniku — wcześniejsze 300/305
// były zbyt agresywne (3 mrugnięcia często zliczane jako 3× LPM lub
// przypadkowy drag przy odrobinę dłuższym zamknięciu powieki).
#define CZAS_DEBOUNCE_MS                   30    // Eliminacja drgań [ms]
#define CZAS_MIN_MRUG_MS                   80    // Min. czas zamknięcia oka żeby liczyło jako mrugnięcie [ms]
                                                 // Ruchy głowy powodują krótkie (<80ms) artefakty — ten próg je odrzuca.
                                                 // Prawdziwe mrugnięcie trwa 100-400 ms. Zwiększ jeśli nadal są fałszywe kliknięcia.
#define OKNO_WIELOKLIKU_MS                 400   // Max odstęp między mrugnięciami w serii [ms]
#define CZAS_KROTKIEGO_KLIKU_MS            80    // Czas press/release [ms]
#define CZAS_MIEDZY_KLIKAMI_PODWOJNEGO_MS  60    // Odstęp release→press w double-click [ms]
#define CZAS_BLYSKU_LED_MS                 60    // Błysk diody przy kliknięciu [ms]

// ================= PRZYTRZYMANIE (DRAG & DROP) ======================
// Jeśli czujnik jest aktywny nieprzerwanie dłużej niż ten próg,
// system wciska i trzyma lewy przycisk myszy (drag). Po zwolnieniu
// czujnika przycisk zostaje puszczony (drop).
// WAŻNE: PROG_PRZYTRZYMANIA_MS musi być wyraźnie większy od
// OKNO_WIELOKLIKU_MS, inaczej długie pojedyncze mrugnięcie wpadałoby
// w drag zamiast być zliczone jako LPM. Aktualny zapas: 100 ms.
#define PROG_PRZYTRZYMANIA_MS  500

// ========================= TRYB SCROLLA =============================
// 4 mrugnięcia → toggle trybu scrolla (wejście / wyjście).
// W trybie scrolla dioda świeci ciągle, ruch głowy góra/dół steruje
// kółkiem myszy, a kliknięcia (1–3 mrugnięcia) są zablokowane.
#define DZIELNIK_SCROLLA       4      // Skalowanie prędkości scrolla

// ============== DETEKCJA TCRT5000 (SYGNAŁ ANALOGOWY) =================
// Algorytm trzystopniowy:
//   1. Filtr EMA (dolnoprzepustowy) wygładza szum ADC w czasie rzeczywistym.
//   2. Komparator z dwiema progami (histereza) zapobiega oscylacjom
//      przy powolnych zmianach sygnału — generuje wirtualny stan
//      "oko otwarte / zamknięte".
//   3. Pływające tło (bardzo wolny EMA) kompensuje dryf optyczny
//      (zmiana oświetlenia w pomieszczeniu, zmęczenie diody IR).
//
// Wartości baseline wyznaczane są przy starcie metodą średniej
// uciętej (trimmed-mean) z 80% środkowych próbek po sortowaniu —
// odporne na pojedyncze artefakty pomiarowe.
#define PROBKI_TCRT_KALIBRACJI     200       // Liczba próbek startowych
#define EMA_ALPHA                  0.20f     // Filtr szybki: wygładza szum ADC, śledzi rzeczywisty sygnał.
                                             // Zbyt małe (< 0.15) = wolny powrót między mrugnięciami = drag!
#define EMA_ALPHA_WOLNY            0.005f    // Filtr wolny: adaptacyjny baseline śledzący światło otoczenia
                                             // i pozycję głowy. Stała czasowa ≈ 200 próbek = 2 s @100 Hz.
                                             // Aktualizowany TYLKO gdy oko otwarte — blink go nie przesuwa.
                                             // Zastępuje TRACKING_ALPHA + TIMEOUT_TRACKING_MS: brak timeoutu,
                                             // adaptacja natychmiastowa (ale wolna) po każdej iteracji.
                                             // Zmiana oświetlenia → baseline dogania w ~3–5 s.
#define OFFSET_TRIGGER             350       // ADC: sygnał musi spaść o tyle poniżej baseline = oko zamknięte.
#define OFFSET_RELEASE             120       // ADC: sygnał musi wrócić powyżej (baseline - OFFSET_RELEASE)
                                             // żeby system uznał otwarcie oka.
#define OFFSET_MAX_ZWARCIA         800       // ADC: jeśli sygnał spada GŁĘBIEJ niż (baseline - ta wartość),
                                             // to NIE jest mrugnięcie — zdarzenie mechaniczne (zdjęcie okularów,
                                             // zasłonięcie czujnika itp.). Normalne mrugnięcie: 350–700 ADC.

// ========================= KIERUNKI OSI =============================
#define ODWROC_OS_X            (1)
#define ODWROC_OS_Y            (1)

// ========================= DIAGNOSTYKA ==============================
// Flaga sterująca wypisywaniem logów na Serial została przeniesiona
// do zmiennej runtime `czyDebugWlaczony` (w main.cpp) — domyślnie
// wyłączona, włączana sekwencją 6 mrugnięć.
#define PREDKOSC_SERIAL        115200
#define OKRES_DIAGNOSTYKI_MS   500

// ===================== WEB DEBUG (WiFi AP) ===========================
// Wlacz/wylacz modul diagnostyczny WiFi. Gdy true, ESP32 stawia
// wlasna siec WiFi i serwuje strone z logami na 192.168.4.1.
// Gdy false – modul sie nie kompiluje, zero wplywu na reszte kodu.
#define WEBDEBUG_AKTYWNY       true
#define WEBDEBUG_SSID          "HEFAS-Debug"
#define WEBDEBUG_HASLO         "hefas1234"

// ===================== PARAMETRY PĘTLI GŁÓWNEJ ======================
#define OKRES_PETLI_MS         10     // ~100 Hz

// ====================== MONITOR BATERII (XIAO Plus) ==================
// Ogniwo w projekcie: Akyga Li-Pol 1900 mAh 1S 3,7 V (50×34×9,5 mm), JST 3-pin.
// Używamy wyłąnie pinów + (BAT+) i − (BAT−) → pady BAT+/BAT− na spodzie XIAO.
// Środkowy pin JST (NTC) NIE jest podłączany.
//
// Pin D16 (GPIO10 = ADC1_CH9 = ADC_BAT), wewnętrzny dzielnik 1:11 (R9=100K + R8=10K).
// ADC_11db (0–~3100 mV): pin ≈ 330–380 mV przy naładowanym ogniwie 3,7–4,2 V.
// analogReadMilliVolts() + trimmed-mean 64 próbek; atenuacja ustawiana przed każdym pomiarem.
//
// Kalibracja DZIELNIK_BATERII (gdy Vbat w logu odbiega od multimetru o > 0,1 V):
//   DZIELNIK_BATERII = V_multimetr / (pin_mV / 1000.0)
#define PIN_ADC_BATERIA              10        // D16 = GPIO10 = ADC_BAT (ADC1_CH9)
#define DZIELNIK_BATERII             11.0f     // R9=100K + R8=10K → 1:11; skoryguj wg multimetru
#define PROBKI_BATERII               64        // Liczba próbek do trimmed-mean
#define OKRES_POMIARU_BATERII_MS     3000      // Okres pomiaru [ms]
#define V_BATERIA_BRAK_PROG          2.5f      // < 2.5 V → ogniwo niepodłączone (Li-Pol nigdy nie pracuje poniżej ~3.0 V)
#define V_BATERIA_PELNA              4.20f     // Pełna 1S Li-Pol
#define V_BATERIA_PUSTA              3.20f     // Praktyczne odcięcie 1S Li-Pol

#endif // HEFAS_CONFIG_H
