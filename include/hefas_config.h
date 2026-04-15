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
#define PIN_LM393              1      // D0 (GPIO 1) – wejście czujnika (Active LOW)
#define LM393_AKTYWNY_STAN     LOW    // Stan aktywny czujnika (impuls)

// LED_BUILTIN jest zdefiniowany przez board XIAO ESP32-S3 (GPIO 21).

// =================== PARAMETRY CZUŁOŚCI I FILTRACJI =================
#define CZULOSC_MYSZY          0.4f   // Skalowanie ruchu (0.2–1.0)
#define STREFA_MARTWA          2.0f   // Dead zone [°/s]
#define PROG_ZYROSKOPU         1.5f   // Próg szumu żyroskopu [°/s]
#define PROBKI_KALIBRACJI      200    // Próbki kalibracji
#define CZULOSC_ZYRO_LSB       131.0f // LSB/(°/s) dla ±250°/s

// =================== PARAMETRY DETEKCJI KLIKNIĘĆ ====================
#define CZAS_DEBOUNCE_MS       50     // Eliminacja drgań [ms]
#define OKNO_WIELOKLIKU_MS     400    // Max odstęp między impulsami [ms]
#define CZAS_KROTKIEGO_KLIKU_MS 30    // Czas press/release [ms]
#define CZAS_BLYSKU_LED_MS     60     // Błysk diody przy kliknięciu [ms]

// ================= PRZYTRZYMANIE (DRAG & DROP) ======================
// Jeśli czujnik jest aktywny nieprzerwanie dłużej niż ten próg,
// system wciska i trzyma lewy przycisk myszy (drag). Po zwolnieniu
// czujnika przycisk zostaje puszczony (drop).
#define PROG_PRZYTRZYMANIA_MS  600

// ========================= TRYB SCROLLA =============================
// 3 szybkie mrugnięcia → wejście w tryb scrolla (dioda świeci ciągle).
// 2 szybkie mrugnięcia → wyjście z trybu scrolla.
// W trybie scrolla ruch głowy góra/dół steruje kółkiem myszy.
#define DZIELNIK_SCROLLA       4      // Skalowanie prędkości scrolla

// ========================= KIERUNKI OSI =============================
#define ODWROC_OS_X            (1)
#define ODWROC_OS_Y            (1)

// ========================= DIAGNOSTYKA ==============================
#define PREDKOSC_SERIAL        115200
#define TRYB_DEBUG             true
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

#endif // HEFAS_CONFIG_H
