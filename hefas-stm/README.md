# HEFAS STM — Nicla Vision

**HEFAS** (Head-controlled Electronic Functional Assistive System) w wersji na **Arduino Nicla Vision** (STM32H747).  
Poprzednia wersja (**HEFAS 4.0** w katalogu głównym repozytorium) działa na ESP32-S3 + MPU6050 + TCRT5000 — traktuj ją jako **referencję logiki**, nie jako kod do kopiowania.

## Cele migracji

| Obszar | ESP32 (stare) | Nicla Vision (HEFAS STM) |
|--------|---------------|---------------------------|
| Ruch głowy | MPU6050 (I2C) | LSM6DSOX (on-board) |
| Mrugnięcia | TCRT5000 analog | Kamera GC2145 + klasyfikator ML |
| HID | USB + BLE Mouse | USBMouse (BLE — planowane) |
| Debug | WiFi AP WebDebug | Serial (+ ewentualnie WiFi później) |

## Struktura

```
hefas-stm/
├── platformio.ini
├── include/
│   └── config.h           # wszystkie progi i czułości
└── src/
    ├── main.cpp           # pętla, kalibracja, gest
    ├── imu/               # LSM6DSOX → delta kursora
    ├── vision/            # kamera + klasyfikator oka
    ├── click_logic/       # serie mrugnięć, drag, scroll
    └── hid/               # USB HID mysz
```

## Mapowanie mrugnięć (bez zmian względem 4.0)

| Serie | Akcja |
|-------|--------|
| 1× | LPM |
| 2× | Double-click LPM (w scrollu: wyłączenie scrolla) |
| 3× | PPM |
| 4× | Scroll ON |
| 5× | Rekalibracja IMU |
| 6× | Przełącznik logów Serial |
| Długie zamknięcie oka (~850 ms) | Drag (przytrzymanie LPM) |

Parametry czasowe: `include/config.h` (przeniesione z `include/hefas_config.h`).

## Wizja i model ML

1. Zbierz dane oka (OpenMV IDE lub skrypt zbierający ROI) w różnym oświetleniu.
2. Wytrenuj klasyfikator 2-klasowy (otwarte / zamknięte) w **Edge Impulse** (int8, wejście np. 96×96 szarość).
3. Wgraj model do `vision/` i wyłącz `VISION_TRYB_PROG_JASNOSCI` w `config.h`.

Do czasu modelu aktywny jest **progowy fallback jasności ROI** — tylko do prototypu montażu kamery.

## Kompilacja i wgranie

```bash
cd hefas-stm
pio run -e nicla_vision
pio run -e nicla_vision -t upload
```

Wymagany rdzeń **Arduino Mbed OS Nicla Boards** (instalowany przez PlatformIO / Arduino IDE).

## Otwarte decyzje (do testów na sprzęcie)

- **USB HID + kamera jednocześnie** — możliwe konflikty USB; jeśli wystąpią, przełączyć na BLE HID (`HID_PREFER_BLE`).
- **Podział M7 / M4** — inferencja na M4, IMU + HID na M7 (osobne `env:nicla_vision_m4`).
- **Klawiatura HID** (gest OSK Ctrl+Win+O) — niezaimplementowana; wymaga `USBKeyboard` obok `USBMouse`.
- **VL53L1X** — opcjonalne wykrycie obecności użytkownika (nie w szkielecie).

## Autorzy

Koło naukowe Automatyki i Robotyki, ZUT Szczecin — Bartek i Sebastian.
