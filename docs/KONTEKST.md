# Kontekst (Nicla Vision)

Płytka = kompletny węzeł: **STM32H747** (M7+M7), **LSM6DSOX**, **GC2145**, **VL53L1** (moduł pod kamerą), **MP34DT06**, **Murata 1DX** (Wi‑Fi/BT), **SE050**, **PF1550** + Li‑Po, **16 MB** flash, USB, ESLOV.

HEFAS STM w C++/mbed: IMU → kursor; onboard ranging → impulsy kliknięć (docelowo np. zacisk szczęki / policzek — montaż okularów, nie osobny czujnik). Kamera i ML nie są wymagane na tej ścieżce.

Otwarte: BLE HID, USBKeyboard (gest), podział M7/M4, strojenie `config.h` na ciele.
