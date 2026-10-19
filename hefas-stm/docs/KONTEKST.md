# HEFAS STM — kontekst dla Cursora

Ten plik jest skrótem wymagań projektu (migracja z ESP32 na Nicla Vision). Szczegóły implementacji: `README.md` i `include/config.h`.

## Zasady

- Kod pod **Arduino Nicla Vision** (mbed), bez ESP-IDF / bibliotek ESP32.
- Moduły: `imu/`, `vision/`, `click_logic/`, `hid/`, `config.h`.
- Komentarze po polsku.
- Niepewne zachowania sprzętowe — oznaczać `TODO` i proponować test.

## Funkcje do zachowania

- Kursor z ruchu głowy (martwa strefa, kalibracja neutralnej pozycji).
- Mrugnięcia celowe vs odruchowe (progi czasowe w `config.h`).
- HID mysz bez sterowników na hoście.
- Serie mrugnięć, scroll, drag, rekalibracja, debug.
- Gest przechylenia (Gy) — klawiatura ekranowa (do dokończenia na USB).

## Sprzęt Nicla Vision

- IMU: LSM6DSOX  
- Kamera: GC2145  
- ToF: VL53L1X (opcjonalnie)  
- USB + Wi-Fi/BLE Murata 1DX  

## Następne kroki inżynierskie

1. Model Edge Impulse + integracja TFLite Micro w `vision/`.
2. Test USB HID + `grabFrame()` na docelowym montażu okularów.
3. BLE HID jako tryb bezprzewodowy (odpowiednik HEFAS 4.0).
4. Ewentualny wątek mbed / rdzeń M4 na inferencję (~30 Hz kamera vs 100 Hz IMU).
