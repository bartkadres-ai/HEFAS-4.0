/**
 * ============================================================
 *  HEFAS – Plik konfiguracyjny sprzętu i parametrów strojenia
 * ============================================================
 */

#ifndef HEFAS_CONFIG_H
#define HEFAS_CONFIG_H

// ========================== PINY SPRZĘTOWE ==========================
#define PIN_SDA                5
#define PIN_SCL                6
#define PIN_TCRT_ANALOG        A1

// Ogniwo Li-Po na BAT+/BAT− (projekt docelowy). Debug pokazuje „Ogniwo” osobno od USB.
#define HEFAS_OGNIOWO_ZAMONTOWANE          1

// Opcjonalnie: wykrywanie 5 V z USB na pinie ADC (dzielnik 100k+100k z wyprowadzenia 5V płytki).
// Bez tego: „USB kabel” = tud_connected() (ładowarka bez danych może nie być widoczna).
// #define PIN_VBUS_ADC                       A0
// #define VBUS_ADC_PROG_USB                  2200   // ok. 2,5 V na ADC przy 5 V na wejściu

// =================== PARAMETRY CZUŁOŚCI I FILTRACJI =================
#define CZULOSC_MYSZY          0.4f
#define STREFA_MARTWA          2.0f
#define PROG_ZYROSKOPU         1.5f
#define CZULOSC_ZYRO_LSB       131.0f

// =================== KALIBRACJA (asynchroniczna, 100 Hz) =============
#define CAL_PRZERWA_MS             10
#define CAL_PROBKI_ROWNOLEGLE      200
#define CAL_PROBKI_IR              100
#define PROBKI_TCRT_KALIBRACJI     CAL_PROBKI_ROWNOLEGLE

// =================== PARAMETRY DETEKCJI MRUGNIĘĘĆ =====================
// Seria: max przerwa między otwarciami < OKNO; po ostatnim impulsie cisza zależy od licznika (SERIA_CISZA_PO_N).
#define CZAS_MIN_MRUG_MS                   42
#define CZAS_MAX_MRUG_MS                   280
#define OKNO_MIEDZY_IMPULSAMI_MS           600   // max przerwa (otwarte oko) między impulsami w serii
// Szybkie serie (np. 3×) — bez min. przerwy między otwarciami; szum odcina CZAS_MIN_MRUG_MS + walidacja SERIA_CZAS_MIN_3_MS.
#define CZAS_REFRAKTORY_PO_IMPULSIE_MS     0     // 0 = wyłączone (nie blokuj kolejnych zamknięć po impulsie)
// Cisza i max. span serii skalowane do OKNO 600 ms (wcześniej 450 ms → ×4/3; max. span ≈ (N-1)*OKNO + 100).
#define SERIA_CISZA_PO_1_MS                350
#define SERIA_CISZA_PO_2_MS                450
#define SERIA_CISZA_PO_3_MS                640
#define SERIA_CISZA_PO_4_MS                770
#define SERIA_CISZA_PO_5_MS                910
#define SERIA_CISZA_PO_6_MS                1000
#define SERIA_CZAS_MIN_3_MS                120   // walidacja 3× (za krótka seria = szum)
#define SERIA_CZAS_MAX_3_MS                1300  // 2×OKNO + zapas
#define SERIA_CZAS_MAX_4_MS                1900
#define SERIA_CZAS_MAX_5_MS                2500
#define SERIA_CZAS_MAX_6_MS                3100
#define MAX_MRUGNIEC_W_SERII               6
#define CZAS_KROTKIEGO_KLIKU_MS            80
#define CZAS_MIEDZY_KLIKAMI_PODWOJNEGO_MS  60
#define CZAS_BLYSKU_LED_MS                 60

#define PROG_PRZYTRZYMANIA_MS              850   // drag po ~0,85 s zamkniętego oka (musi być > CZAS_MAX_MRUG_MS)
#define PROBKI_POTWIERDZENIA_DRAG_OFF      5     // 5×10 ms otwarcia, żeby drag się nie urwał na szumie progu
#if CZAS_MAX_MRUG_MS >= PROG_PRZYTRZYMANIA_MS
#error CZAS_MAX_MRUG_MS must be < PROG_PRZYTRZYMANIA_MS (inaczej strefa 800–1000 ms jest niejednoznaczna)
#endif

// ========================= TRYB SCROLLA =============================
#define DZIELNIK_SCROLLA           7
#define PROG_ZYRO_SKROL_DEG_S        3.0f

// ============== DETEKCJA TCRT5000 (SYGNAŁ ANALOGOWY) =================
// Szybka ścieżka (krawędzie mrugnięć) vs wolna (baseline) — rozdzielone, żeby EMA
// nie „zlewało” szybkich serii mrugnięć w jeden sygnał.
#define EMA_ALPHA_SZYBKI           0.50f
#define EMA_ALPHA_WYSWIETLANIA     0.20f
#define EMA_ALPHA_WOLNY            0.003f
#define OFFSET_TRIGGER             320
#define OFFSET_RELEASE             140
#define OFFSET_MAX_ZWARCIA         800
#define PROBKI_POTWIERDZENIA_STANU 2      // 2×10 ms — zbocza mrugnięć
#if PROBKI_POTWIERDZENIA_DRAG_OFF < PROBKI_POTWIERDZENIA_STANU
#error PROBKI_POTWIERDZENIA_DRAG_OFF must be >= PROBKI_POTWIERDZENIA_STANU
#endif

// ========================= KIERUNKI OSI =============================
#define ODWROC_OS_X            (1)
#define ODWROC_OS_Y            (1)

// ============== GEST: przechylenie głowy w PRAWO (oś Gy) — nie rusza kursora =====
// ODWROC_GEST_GY = -1: u tego montażu prawo = warunek (gy * ODWROC) < -PROG — NIE zmieniać na 1.
#define GEST_PRZECHYL_PRAWO_WLACZONY    1
#define GEST_PRZECHYL_LEWO_WLACZONY     GEST_PRZECHYL_PRAWO_WLACZONY
#define ODWROC_GEST_GY                  (-1)
#define GEST_GY_PROG_DEG_S              40.0f
#define GEST_GY_CZAS_TRWANIA_MS         280
#define GEST_GY_HISTEREZA_ODSET         0.55f  // cofnięcie Gy przerywa liczenie trwania
#define GEST_COOLDOWN_MS                2200
#define GEST_BLOKADA_GX_GZ_DEG_S        38.0f

// ========================= DIAGNOSTYKA ==============================
#define PREDKOSC_SERIAL        115200
#define OKRES_DIAGNOSTYKI_MS   500

// ===================== WEB DEBUG (WiFi AP) ===========================
#define WEBDEBUG_AKTYWNY       true
#define WEBDEBUG_SSID          "HEFAS-Debug"
#define WEBDEBUG_HASLO         "hefas1234"

// ===================== PARAMETRY PĘTLI GŁÓWNEJ ======================
#define OKRES_PETLI_MS         10

// ===================== HID KOLEJKA ===================================
#define HID_KOLEJKA_ROZMIAR   4

// ===================== START I SYGNALIZACJA ==========================
#define CZAS_STABILIZACJI_START_MS     2000
#define OKRES_MIG_LED_BRAK_POL_MS      2000
#define CZAS_MIG_LED_BRAK_POL_ON_MS    100

// ====================== BLE (reklama + oszczędzanie energii) =========
// Docelowo urządzenie na ogniwie: reklama BLE ON od startu (także w kalibracji), gdy host nie ma USB-HID.
// USB-HID (tud_mounted) → reklama OFF (priorytet kabla do PC). Po 5 min bezczynności — uśpienie radia.
// Pobudzenie: ruch głowy, mrugnięcie, gest, ruch myszy. Niski stan ogniwa ≠ „podłącz USB” w UI.
#define CZAS_BEZCZYNNOSCI_BLE_MS       (5UL * 60UL * 1000UL)   // 5 min
#define PROG_PROBUDZENIA_BLE_DEG_S     12.0f   // suma |Gx|+|Gy| powyżej → wznów reklamę

#endif // HEFAS_CONFIG_H
