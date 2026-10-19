/**
 * ============================================================
 *  HEFAS STM – konfiguracja sprzętu i parametrów strojenia
 *  Płytka: Arduino Nicla Vision (LSM6DSOX + GC2145)
 * ============================================================
 */

#ifndef HEFAS_CONFIG_H
#define HEFAS_CONFIG_H

// ===================== WYJŚCIE HID ================================
// 0 = tylko USB HID (USBMouse), 1 = preferuj BLE HID (do implementacji)
#define HID_PREFER_BLE                 0

// =================== RUCH KURSORA (IMU) ============================
#define CZULOSC_MYSZY                  0.4f
#define STREFA_MARTWA                  2.0f
#define PROG_ZYROSKOPU                 1.5f
// LSM6DSOX @ ±250 dps → ok. 8.75 mdps/LSB → ~114 LSB/(°/s); stroić po kalibracji.
#define CZULOSC_ZYRO_LSB               114.0f

#define ODWROC_OS_X                    (1)
#define ODWROC_OS_Y                    (1)

// =================== KALIBRACJA IMU ===============================
#define CAL_PRZERWA_MS                 10
#define CAL_PROBKI_IMU                 200

// =================== WIZJA (MRUGNIĘCIA) ===========================
#define VISION_ROI_SZEROKOSC           96
#define VISION_ROI_WYSOKOSC            96
// Rozdzielczość przechwytywania (mniejsza = mniej RAM; model EI zwykle 96×96 szarość)
#define VISION_RAMKA_SZEROKOSC         160
#define VISION_RAMKA_WYSOKOSC          120
#define VISION_KLATKI_POMIN_START      5

// Progi klasyfikatora (0..1); po wgraniu modelu Edge Impulse podmienić w eye_detector.cpp
#define VISION_PROG_OKO_ZAMKNIETE      0.55f
#define VISION_PROG_OKO_OTWARTE        0.45f

// Tymczasowy tryb bez modelu ML: prosta detekcja jasności ROI (tylko prototyp / zbieranie danych)
#define VISION_TRYB_PROG_JASNOSCI      1
#define VISION_PROG_JASNOSC_ZAMKNIETE  40.0f

// =================== PARAMETRY MRUGNIĘĆ (jak HEFAS 4.0) ===========
#define CZAS_MIN_MRUG_MS               42
#define CZAS_MAX_MRUG_MS               280
#define OKNO_MIEDZY_IMPULSAMI_MS       600
#define CZAS_REFRAKTORY_PO_IMPULSIE_MS 0

#define SERIA_CISZA_PO_1_MS            350
#define SERIA_CISZA_PO_2_MS            450
#define SERIA_CISZA_PO_3_MS            640
#define SERIA_CISZA_PO_4_MS            770
#define SERIA_CISZA_PO_5_MS            910
#define SERIA_CISZA_PO_6_MS            1000
#define SERIA_CZAS_MIN_3_MS            120
#define SERIA_CZAS_MAX_3_MS            1300
#define SERIA_CZAS_MAX_4_MS            1900
#define SERIA_CZAS_MAX_5_MS            2500
#define SERIA_CZAS_MAX_6_MS            3100
#define MAX_MRUGNIEC_W_SERII           6

#define PROG_PRZYTRZYMANIA_MS          850
#define PROBKI_POTWIERDZENIA_STANU     2
#define PROBKI_POTWIERDZENIA_DRAG_OFF  5

#if CZAS_MAX_MRUG_MS >= PROG_PRZYTRZYMANIA_MS
#error CZAS_MAX_MRUG_MS musi być < PROG_PRZYTRZYMANIA_MS
#endif
#if PROBKI_POTWIERDZENIA_DRAG_OFF < PROBKI_POTWIERDZENIA_STANU
#error PROBKI_POTWIERDZENIA_DRAG_OFF musi być >= PROBKI_POTWIERDZENIA_STANU
#endif

// ========================= TRYB SCROLLA ===========================
#define DZIELNIK_SCROLLA               7
#define PROG_ZYRO_SKROL_DEG_S          3.0f

// ===================== GEST: przechyl w prawo (Gy) =================
#define GEST_PRZECHYL_PRAWO_WLACZONY   1
#define ODWROC_GEST_GY                 (-1)
#define GEST_GY_PROG_DEG_S             40.0f
#define GEST_GY_CZAS_TRWANIA_MS        280
#define GEST_GY_HISTEREZA_ODSET        0.55f
#define GEST_COOLDOWN_MS               2200
#define GEST_BLOKADA_GX_GZ_DEG_S       38.0f

// ========================= HID / LED ==============================
#define CZAS_KROTKIEGO_KLIKU_MS        80
#define CZAS_MIEDZY_KLIKAMI_PODWOJNEGO_MS 60
#define CZAS_BLYSKU_LED_MS             60
#define HID_KOLEJKA_ROZMIAR            4

// ========================= PĘTLA / SERIAL =========================
#define PREDKOSC_SERIAL                115200
#define OKRES_PETLI_MS                 10
#define OKRES_DIAGNOSTYKI_MS           500
#define CZAS_STABILIZACJI_START_MS     2000

// ========================= DEBUG ==================================
#define DEBUG_SERIAL_DOMYSLNIE         0

#endif // HEFAS_CONFIG_H
