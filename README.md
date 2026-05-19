# HEFAS 4.0 – sterowanie myszka ruchem glowy

System bezdotykowej obslugi kursora myszy ruchami glowy dla osob z niepelnosprawnosciami.
**HEFAS** = Head-controlled Electronic Functional Assistive System.

## Jak to dziala

- **Ruch kursora X (lewo/prawo):** obrot glowy (os gx zyroskopu).
- **Ruch kursora Y (gora/dol):** pochylenie glowy (os gz zyroskopu).
- **Klikniecia (LM393 DO, pin D0):**
  - 1 mrugniecie → **lewy klik**,
  - 2 szybkie mrugniecia (< 400 ms) → **prawy klik**,
  - 3 szybkie mrugniecia → **wejscie w tryb scrolla** (dioda swieci ciagle),
  - 2 mrugniecia w trybie scrolla → **wyjscie z trybu scrolla**,
  - 4 szybkie mrugniecia → **rekalibracja zyroskopu** (dioda mrugnie 3×),
  - dlugie zamkniecie oka (> 600 ms) → **przytrzymanie lewego przycisku** (drag & drop).
- **Tryb scrolla:** ruch glowy gora/dol steruje kolkiem myszy zamiast kursorem.
- **Komunikacja:** USB HID (priorytet, po kablu) / Bluetooth Low Energy (na baterii).

## Sprzet i piny (Seeed XIAO ESP32-S3 Plus)

| Pin na plytce | GPIO | Podlaczenie                        |
|---------------|------|------------------------------------|
| `D0`          | 1    | **D0/DO** z modulu LM393 (klikniecia) |
| `D4`          | 5    | **SDA** czujnika MPU6050           |
| `D5`          | 6    | **SCL** czujnika MPU6050           |
| `LED_BUILTIN` | 21   | Dioda wbudowana (status/klikniecia)|

Czujnik LM393 zasilany z 3.3 V. Wyjscie A0 (analogowe) nie jest uzywane.

## Struktura projektu

```
HEFAS 4.0/
├── platformio.ini              Konfiguracja PlatformIO (board, flagi USB, biblioteki)
├── include/
│   ├── hefas_config.h          Wszystkie stale konfiguracyjne (piny, czulosc, progi)
│   └── hefas_webdebug.h        Naglowek modulu diagnostyki WiFi
├── src/
│   ├── main.cpp                Pelna logika systemu (IMU, klikniecia, USB/BLE)
│   └── hefas_webdebug.cpp      Modul diagnostyczny WiFi (AP + WebServer)
└── README.md                   Ten plik
```

## Uruchomienie

1. Podlacz MPU6050: SDA → D4, SCL → D5.
2. Podlacz LM393: DO → D0, zasilanie 3.3 V.
3. Podlacz XIAO ESP32-S3 Plus przez USB.
4. Otworz projekt w PlatformIO.
5. Zbuduj i wgraj: `pio run -t upload` lub Ctrl+Alt+U.
6. Po starcie trzymaj glowe nieruchomo ~1 s (kalibracja zyroskopu).
7. Dioda mrugnie 3× = system gotowy.

## Strojenie parametrow

Wszystkie parametry do regulacji sa w `include/hefas_config.h`:

| Parametr            | Domyslna | Opis                                       |
|---------------------|----------|---------------------------------------------|
| `CZULOSC_MYSZY`     | 0.4      | Predkosc kursora (0.2 = wolno, 1.0 = szybko) |
| `STREFA_MARTWA`     | 2.0 °/s  | Dead zone – zapobiega dryfowi w spoczynku   |
| `PROG_ZYROSKOPU`    | 1.5 °/s  | Prog szumu zyroskopu                        |
| `ODWROC_OS_X`       | 1        | Kierunek osi X (+1 lub -1)                  |
| `ODWROC_OS_Y`       | 1        | Kierunek osi Y (+1 lub -1)                  |
| `OKNO_WIELOKLIKU_MS`| 400 ms   | Okno czasowe na podwojne/potrojne klikniecie|
| `PROG_PRZYTRZYMANIA_MS` | 600 ms | Prog dlugiego zamkniecia oka (drag)      |
| `DZIELNIK_SCROLLA`  | 4        | Predkosc scrollowania (wiecej = wolniej)    |

## Diagnostyka przez WiFi (WebDebug)

ESP32 stawia wlasna siec WiFi i serwuje strone z logami na zywo.
Nie wymaga internetu, routera ani dodatkowego sprzetu.

### Jak wejsc na strone diagnostyczna:

1. Wgraj firmware na plytke.
2. Na telefonie lub laptopie wejdz w **ustawienia WiFi**.
3. Polacz sie z siecia **`HEFAS-Debug`** (haslo: **`hefas1234`**).
4. Otworz przegladarke (Chrome, Safari, Firefox).
5. Wpisz adres: **`192.168.4.1`** i wcisnij Enter.
6. Zobaczysz panel diagnostyczny z logami na zywo.

### Co widac na stronie:

- **Logi na zywo** – klikniecia, ruch kursora, zdarzenia scrolla/draga.
- **Status** – czy mysz jest aktywna, tryb scrolla, drag.
- **Przycisk PAUZA** – zatrzymuje ruch myszy i klikniecia (debug bez szalonego kursora).
- **Przycisk REKALIBRACJA** – uruchamia kalibracje zyroskopu zdalnie.

### Wylaczanie modulu WiFi:

Jesli WebDebug nie jest potrzebny, w `include/hefas_config.h` zmien:
```
#define WEBDEBUG_AKTYWNY    false
```
Modul sie nie skompiluje – zero wplywu na reszte kodu, zero zuzycia RAM/WiFi.

## Diagnostyka przez Serial Monitor

Alternatywnie, podlacz Serial Monitor (115200 baud). System wypisuje:
- `[KALIBRACJA]` – offsety zyroskopu,
- `[RUCH]` / `[SCROLL]` – dane ruchu co 500 ms,
- `[KLIK]` – wykryte klikniecia,
- `[DRAG]` – przytrzymanie/zwolnienie przycisku,
- `[SCROLL] ON/OFF` – przelaczanie trybu scrolla.

## Autorzy

Bartlomiej Adamczyk, Sebastian Sobczyk,
Mechatronika – Szczecin 2026
