/**
 * HEFAS STM – konfiguracja (Arduino Nicla Vision: LSM6DSOX + VL53L1X ToF)
 */

#ifndef HEFAS_CONFIG_H
#define HEFAS_CONFIG_H

#define HID_PREFER_BLE                 0

// =================== RUCH KURSORA (IMU) ============================
#define CZULOSC_MYSZY                  0.4f
#define STREFA_MARTWA                  2.0f
#define PROG_ZYROSKOPU                 1.5f
#define CZULOSC_ZYRO_LSB               114.0f
#define ODWROC_OS_X                    (1)
#define ODWROC_OS_Y                    (1)

// =================== KALIBRACJA ===================================
#define CAL_PRZERWA_MS                 10
#define CAL_PROBKI_ROWNOLEGLE          200
#define CAL_PROBKI_IMU                 CAL_PROBKI_ROWNOLEGLE
#define CAL_PROBKI_STABILIZACJI        100

// =================== VL53L1X (MRUGNIĘCIA) =========================
// Montaż: czujnik skierowany na oko/powiekę; zamknięcie zwykle ZMNIEJSZA odległość (mm).
#define TOF_ODCZYT_OKRES_MS            20
#define TOF_ODCZYT_MIN_MM              20
#define TOF_ODCZYT_MAX_MM              3500
#define TOF_BRAK_POMIARU_MM            8191

#define EMA_ALPHA_SZYBKI               0.50f
#define EMA_ALPHA_WYSWIETLANIA         0.20f
#define EMA_ALPHA_WOLNY                0.003f
// Histereza w mm względem baseline (analogia do TCRT w HEFAS 4.0)
#define TOF_OFFSET_TRIGGER_MM          18
#define TOF_OFFSET_RELEASE_MM          8
#define TOF_OFFSET_BLISKIE_MM          5

// =================== PARAMETRY MRUGNIĘĆ ===========================
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

#define DZIELNIK_SCROLLA               7
#define PROG_ZYRO_SKROL_DEG_S          3.0f

#define GEST_PRZECHYL_PRAWO_WLACZONY   1
#define ODWROC_GEST_GY                 (-1)
#define GEST_GY_PROG_DEG_S             40.0f
#define GEST_GY_CZAS_TRWANIA_MS        280
#define GEST_GY_HISTEREZA_ODSET        0.55f
#define GEST_COOLDOWN_MS               2200
#define GEST_BLOKADA_GX_GZ_DEG_S       38.0f

#define CZAS_KROTKIEGO_KLIKU_MS        80
#define CZAS_MIEDZY_KLIKAMI_PODWOJNEGO_MS 60
#define CZAS_BLYSKU_LED_MS             60
#define HID_KOLEJKA_ROZMIAR            4

#define PREDKOSC_SERIAL                115200
#define OKRES_PETLI_MS                 10
#define OKRES_DIAGNOSTYKI_MS           500
#define CZAS_STABILIZACJI_START_MS     2000

#define DEBUG_SERIAL_DOMYSLNIE         0

#endif
