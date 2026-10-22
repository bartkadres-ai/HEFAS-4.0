# HEFAS STM

Firmware **HEFAS** na **Arduino Nicla Vision** (IMU + ToF + HID USB/BLE + WiFi WebDebug).

| Co | Repo | Gałąź |
|----|------|--------|
| **Nicla (ten kod)** | [HEFAS-4.0](https://github.com/bartkadres-ai/HEFAS-4.0) | **`hefas-stm`** |
| ESP32 | ten sam repo | `main` |
| Kopia Nicla (opcjonalnie) | [Hefas-STM](https://github.com/bartkadres-ai/Hefas-STM) | `main` |

## Folder na PC (Windows)

**Zalecane:** [docs/PULPIT_WINDOWS.md](docs/PULPIT_WINDOWS.md)

```powershell
git clone -b hefas-stm https://github.com/bartkadres-ai/HEFAS-4.0.git "C:\Users\werte\OneDrive\Pulpit\Hefas STM"
cd "C:\Users\werte\OneDrive\Pulpit\Hefas STM"
pio run -e nicla_vision
```

Aktualizacja: `git pull origin hefas-stm`

Upload: [docs/UPLOAD_NICLA_WINDOWS.md](docs/UPLOAD_NICLA_WINDOWS.md)

## Moduły

| Ścieżka | Rola |
|---------|------|
| `src/imu/` | Kursor (LSM6DSOX) |
| `src/tof/` | Mrugnięcia / ToF |
| `src/click_logic/` | Serie, scroll, drag |
| `src/hid/` | USB + BLE mysz |
| `src/web/` | WebDebug WiFi |
| `src/ble/` | BLE HID |
| `include/config.h` | Parametry |

## Build / upload

```bash
pio run -e nicla_vision
pio run -e nicla_vision -t upload
```

Upload na Windows: **double-tap RESET**, potem upload (domyślny env bez COM8). Patrz `docs/UPLOAD_NICLA_WINDOWS.md`.

## Autorzy

ZUT Szczecin, koło Automatyki i Robotyki — Bartek i Sebastian.
