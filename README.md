# HEFAS STM

**HEFAS** na **Arduino Nicla Vision** — jedna płytka (IMU + ranging + HID). Logika sterowania jak [HEFAS 4.0](https://github.com/bartkadres-ai/HEFAS-4.0); referencja ESP32 zostaje w tamtym repozytorium.

## Firmware

| Moduł | Rola |
|--------|------|
| `src/imu/` | Kursor z LSM6DSOX |
| `src/tof/` | Zdarzenia „zamknięte / otwarte” z onboard ranging (histereza jak TCRT w 4.0) |
| `src/click_logic/` | Serie impulsów, scroll, drag |
| `src/hid/` | USB mysz |
| `include/config.h` | Strojenie |

## Mapowanie impulsów (domyślne)

1× LPM · 2× double · 3× PPM · 4× scroll ON · 5× kalibracja · 6× debug Serial · długie trzymanie ≈ drag

## Build

```bash
pio run -e nicla_vision
pio run -e nicla_vision -t upload
```

Rdzeń: **Arduino Mbed OS Nicla Boards**.

## Autorzy

ZUT Szczecin, koło Automatyki i Robotyki — Bartek i Sebastian.
