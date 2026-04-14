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
// Magistrala I2C – czujnik inercyjny MPU6050
#define PIN_SDA                5      // D4 (GPIO 5) – linia danych I2C
#define PIN_SCL                6      // D5 (GPIO 6) – linia zegara I2C

// Czujnik optyczny TCRT5000 + komparator LM393 (Active LOW)
#define PIN_LM393              1      // D0 (GPIO 1) – wejście cyfrowe

// Wyjście pomocnicze (przekaźnik lub dioda sygnalizacyjna kliknięcia)
#define PIN_WYJSCIE            3      // D2 (GPIO 3)

// Dioda wbudowana na płytce XIAO ESP32-S3
#ifndef LED_BUILTIN
  #define LED_BUILTIN          21
#endif

// =================== PARAMETRY CZUŁOŚCI I FILTRACJI =================
// Współczynnik skalowania prędkości kątowej na piksele ruchu kursora.
// Większa wartość = szybszy kursor. Zalecany zakres: 0.2 – 1.0
#define CZULOSC_MYSZY          0.4f

// Strefa martwa (dead zone) – minimalna prędkość kątowa [°/s],
// poniżej której ruch kursora jest zerowany. Zapobiega dryfowi.
#define STREFA_MARTWA          2.0f

// Próg szumu żyroskopu [°/s] – wartości poniżej traktowane jako szum
// czujnika i zerowane przed dalszym przetwarzaniem.
#define PROG_ZYROSKOPU         1.5f

// Liczba próbek uśrednianych podczas autokalibracji żyroskopu.
// Więcej próbek = dokładniejsza kalibracja, ale dłuższy czas startu.
#define PROBKI_KALIBRACJI      200

// Czułość żyroskopu MPU6050 [LSB/(°/s)] dla zakresu ±250°/s
#define CZULOSC_ZYRO_LSB       131.0f

// =================== PARAMETRY DETEKCJI KLIKNIĘĆ ====================
// Czas eliminacji drgań styków (debounce) [ms]
#define CZAS_DEBOUNCE_MS       50

// Maksymalny odstęp między dwoma impulsami [ms], aby rozpoznać
// podwójne kliknięcie (prawy przycisk myszy)
#define OKNO_DWUKLIKU_MS       400

// Czas impulsu wyjścia pomocniczego przy kliknięciu [ms]
#define CZAS_IMPULSU_WYJSCIA   80

// ========================= KIERUNKI OSI =============================
// Mnożniki pozwalające odwrócić kierunek ruchu kursora bez
// przebudowy kodu. Ustaw na -1, jeśli kursor jedzie w złą stronę.
// Zależne od fizycznej orientacji montażu MPU6050 na głowie.
#define ODWROC_OS_X            (-1)
#define ODWROC_OS_Y            (-1)

// ========================= DIAGNOSTYKA ==============================
#define PREDKOSC_SERIAL        115200
#define TRYB_DEBUG             true

// Interwał wypisywania danych diagnostycznych ruchu [ms]
#define OKRES_DIAGNOSTYKI_MS   500

// ===================== PARAMETRY PĘTLI GŁÓWNEJ ======================
// Okres jednej iteracji pętli loop() [ms]. 10 ms ≈ 100 Hz.
#define OKRES_PETLI_MS         10

#endif // HEFAS_CONFIG_H
