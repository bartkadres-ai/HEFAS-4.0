# HEFAS STM

Firmware **HEFAS** na **Arduino Nicla Vision** (IMU + ToF + HID USB/BLE + WiFi WebDebug).

| Repo (jedyny) | Gałąź |
|----------------|--------|
| **https://github.com/bartkadres-ai/Hefas-STM** | **`main`** |

Wersja na ESP32 (HEFAS 4.0) jest tylko w [HEFAS-4.0](https://github.com/bartkadres-ai/HEFAS-4.0) — **bez** osobnej gałęzi STM tam.

## Folder na PC (Windows)

```powershell
git clone https://github.com/bartkadres-ai/Hefas-STM.git "C:\Users\werte\OneDrive\Pulpit\Hefas STM"
cd "C:\Users\werte\OneDrive\Pulpit\Hefas STM"
pio run -e nicla_vision
```

Aktualizacja: `git pull origin main` (nie szukaj gałęzi `hefas-stm` — jej tu nie ma).

Szczegóły: [docs/REPO_I_FOLDER.md](docs/REPO_I_FOLDER.md) · upload Nicla: [docs/UPLOAD_NICLA_WINDOWS.md](docs/UPLOAD_NICLA_WINDOWS.md)

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
