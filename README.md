# HEFAS 4.0 — sterowanie myszką ruchem głowy

System bezdotykowej obsługi kursora myszy ruchami głowy dla osób z niepełnosprawnościami.  
**HEFAS** = Head-controlled Electronic Functional Assistive System.

Platforma: **Seeed Studio XIAO ESP32-S3 Plus** + **MPU6050** (IMU) + **TCRT5000** (mrugnięcia, sygnał analogowy) + opcjonalnie **Akyga Li-Pol 1900 mAh 1S 3,7 V** (BAT+ / BAT−).

---

## Jak to działa

### Ruch kursora (oś Gx i Gz)

- **Oś X (lewo / prawo):** prędkość kątowa **Gx** z MPU6050.
- **Oś Y (góra / dół):** prędkość kątowa **Gz** z MPU6050.
- **Oś Gy** nie steruje kursorem — wykorzystywana do **gestu przechylenia** (patrz niżej).
- Surowe odczyty → °/s przez `CZULOSC_ZYRO_LSB` (131 przy ±250°/s), minus offset z kalibracji, prog szumu i strefa martwa.
- **Rate-control:** delta HID z chwilowej prędkości, bez kumulacji pozycji (brak dryfu orientacji).
- Wzór: `dX ≈ predkosc_Gx[°/s] × CZULOSC_MYSZY (0.4)`; kierunek: `ODWROC_OS_X` / `ODWROC_OS_Y`.

### Detektor mrugnięć (TCRT5000, analogowy na A1)

1. **ADC 12-bit** → `tcrtRaw`.
2. **Dwa filtry EMA:**
   - `tcrtFast` (α = `EMA_ALPHA_SZYBKI` = 0,50) — **progi zamknięcia / otwarcia**,
   - `tcrtFiltered` (α = `EMA_ALPHA_WYSWIETLANIA` = 0,20) — logi / diagnostyka.
3. **Kalibracja async (~3 s):** 200 próbek równolegle MPU+TCRT (trimmed-mean baseline), potem ~1 s stabilizacji IR.
4. **Histereza** na `tcrtFast`: `OFFSET_TRIGGER` 320 / `OFFSET_RELEASE` 140 ADC; poniżej `OFFSET_MAX_ZWARCIA` (800) → zdarzenie mechaniczne, reset.
5. **Baseline wolny** (`EMA_ALPHA_WOLNY` = 0,003) — tylko gdy oko otwarte i **poza serią mrugnięć** (baseline zamrożony w trakcie serii).
6. **Potwierdzenie stanu:** `PROBKI_POTWIERDZENIA_STANU` = 2 (2×10 ms) zanim impuls trafi do licznika serii — anty-migotanie na progu.
7. `wirtualnyStanCzujnika` + `okoPotwierdzoneZamkniete` zasilają maszynę mrugnięć i HID.

### Gest: przechyl głowy w prawo (oś Gy)

- **Nie rusza kursora.** Wyraźne przechylenie w **prawo** (~0,28 s), próg **40 °/s**; `ODWROC_GEST_GY = -1` (nie zmieniać na 1 — u tego montażu to fizycznie prawo).
- **Akcja (tylko USB):** skrót **Ctrl + Win + O** → klawiatura ekranowa w Windows 10/11.
- Na **samym BLE** gest może się wykryć (LED), ale skrót klawiaturowy **nie jest wysyłany** (brak profilu HID Keyboard w BLE).
- Cooldown ~2,2 s między gestami.

### Mapowanie mrugnięć

Seria: kolejne mrugnięcie w tej samej serii, gdy przerwa (otwarte oko) od poprzedniego otwarcia &lt; `OKNO_MIEDZY_IMPULSAMI_MS` (600 ms). **Cisza po ostatnim impulsie** zależy od licznika: 1→350 ms, 2→450, 3→640, 4→770, 5→910, 6→1000 ms (nie wcześniejszy LPM przy szybkich 3×). Szybkie mrugnięcia blisko siebie są liczone; szum odcina `CZAS_MIN_MRUG_MS` (za krótkie zamknięcie) oraz walidacja całej serii 3× (120–1300 ms).

| Mrugnięcia | Akcja |
|------------|--------|
| 1 | **LPM** |
| 2 | **Double-click LPM** — w scrollu: **scroll OFF** (tylko wyjście) |
| 3 | **PPM** |
| 4 | **Scroll ON** (tylko gdy scroll wyłączony) |
| 5 | **Rekalibracja** MPU + TCRT (~3 s, async) |
| 6 | **Przełącznik trybu debug** (`czyDebugWlaczony`) — Serial + szczegółowe logi w WebDebug; domyślnie **wyłączony** |
| Oko zamknięte &gt; `PROG_PRZYTRZYMANIA_MS` (850 ms) | **Drag** (LPM trzymany) |

- Impulsy &lt; `CZAS_MIN_MRUG_MS` (42 ms) — odrzucone.  
- Zamknięcie **280–850 ms** — nie liczy się jako mrugnięcie (strefa przed dragiem); **&gt; 850 ms** → drag. Krótkie mrugnięcia &lt; 280 ms — jak dotąd.  
- **Scroll:** włączenie **4×**; wyłączenie przy **każdym mrugnięciu ≥2** (2× tylko OFF, 3× OFF + PPM). Mniej czuły scroll: `DZIELNIK_SCROLLA` 7, `PROG_ZYRO_SKROL_DEG_S` 3.0.  
- W trybie scrolla 1× zablokowane; drag wyłączony.

### Komunikacja HID

| Tryb | Kiedy | Co działa |
|------|--------|-----------|
| **USB** | Kabel USB-C, host widzi urządzenie (`tud_mounted`) | Mysz HID + **klawiatura HID** (skróty, gest OSK) |
| **BLE** | **Brak USB-HID** u hosta (typowo praca na ogniwie) | Mysz BLE — **reklama ON od startu** (także w kalibracji); po **5 min** bez ruchu — **uśpienie**; budzenie ruchem / mrugnięciem |

**Docelowy tryb:** ogniwo + **sparowanie BLE** — nie trzeba podłączać USB do użytkowania. Kabel USB = opcjonalnie (ładowanie, programowanie, USB-HID + klawiatura gestu). Priorytet: gdy host widzi **USB-HID**, reklama BLE jest wyłączona; po rozłączeniu od PC — BLE wraca. Mrugająca LED = **brak połączenia myszy** (sparuj BLE), nie „podłącz USB”.

**Poziom baterii w %:** firmware **nie mierzy** — na XIAO ESP32-S3 (Plus) **nie ma** podłączenia BAT+ do ADC w MCU (potwierdza [wiki Seeed – Battery Usage](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)). Ładowanie obsługuje **osobny układ** na płytce (czerwona LED).

---

## Sprzęt i piny (XIAO ESP32-S3 Plus)

| Pin | GPIO | Funkcja |
|-----|------|---------|
| D1 / A1 | 2 | TCRT5000 **AO** |
| D4 / D5 | 5 / 6 | MPU6050 **I2C** @ 400 kHz |
| LED_BUILTIN | 21 | Status |
| BAT+ / BAT− | — | Ogniwo 1S (ładowanie z USB; **NTC z JST niepodłączać**) |
| USB-C | — | Zasilanie, programowanie, USB HID |

---

## Struktura projektu

```
HEFAS 4.0/
├── platformio.ini          seeed_xiao_esp32s3, USB HID+CDC, BLE-Mouse, MPU6050
├── include/
│   ├── hefas_config.h      Stałe strojenia
│   └── hefas_webdebug.h    WiFi AP, maski logów (WEBLOG_*)
├── src/
│   ├── main.cpp            IMU, TCRT, mrugnięcia, gest, USB/BLE
│   └── hefas_webdebug.cpp  Panel 192.168.4.1
└── README.md
```

**PlatformIO:** `ARDUINO_USB_MODE=1`, `ARDUINO_USB_CDC_ON_BOOT=1` — natywny USB (mysz + klawiatura + Serial CDC).

---

## Uruchomienie

1. MPU6050: SDA→D4, SCL→D5, 3V3, GND.  
2. TCRT5000: AO→A1, 3V3, GND (DO/LM393 nieużywane).  
3. Opcjonalnie ogniwo na BAT+ / BAT− (tylko +/−).  
4. `pio run -t upload`  
5. Przy starcie: nieruchomo, oko otwarte na TCRT ~3 s (kalibracja async, LED).  
6. 3× mrugnięcie LED = gotowość.

**Parowanie BLE (główna ścieżka):** w telefonie/PC włącz Bluetooth, szukaj **„HEFAS 4.0”** (reklama startuje po włączeniu urządzenia, bez czekania na USB). Słabe/rozładowane ogniwo może nie wystartować — to ograniczenie zasilania, nie wymóg kabla USB. Jeśli panel pokazuje **SLEEP**, porusz głową lub mrugnij.

---

## Strojenie (`include/hefas_config.h`)

### Ruch

| Parametr | Domyślnie | Opis |
|----------|-----------|------|
| `CZULOSC_MYSZY` | 0.4 | Czułość kursora |
| `STREFA_MARTWA` | 2.0 °/s | Martwa strefa |
| `PROG_ZYROSKOPU` | 1.5 °/s | Filtr szumu |

### Mrugnięcia

| Parametr | Domyślnie | Opis |
|----------|-----------|------|
| `OKNO_MIEDZY_IMPULSAMI_MS` | 600 | Max. przerwa między otwarciami w serii |
| `SERIA_CISZA_PO_1_MS` … `_6_MS` | 350…1000 | Cisza po ostatnim impulsie (zależnie od N) |
| `CZAS_REFRAKTORY_PO_IMPULSIE_MS` | 0 | Wyłączone — nie blokuje kolejnych zamknięć po impulsie |
| `SERIA_CZAS_MIN_3_MS` / `MAX_3` | 120 / 1300 | Walidacja czasu serii 3× |
| `SERIA_CZAS_MAX_4_MS` … `_6_MS` | 1900 / 2500 / 3100 | Max. czas serii 4×–6× |
| `PROG_PRZYTRZYMANIA_MS` | 850 | Drag |
| `CZAS_MIN_MRUG_MS` | 42 | Min. czas impulsu |
| `CZAS_MAX_MRUG_MS` | 280 | Powyżej → strefa drag |
| `DZIELNIK_SCROLLA` | 7 | Scroll wolniejszy |
| `PROG_ZYRO_SKROL_DEG_S` | 3.0 | Wyższy próg ruchu w scrollu |

### TCRT

| Parametr | Domyślnie |
|----------|-----------|
| `EMA_ALPHA_SZYBKI` | 0.50 |
| `EMA_ALPHA_WOLNY` | 0.003 |
| `OFFSET_TRIGGER` / `RELEASE` | 320 / 140 |

### Gest Gy

| Parametr | Domyślnie |
|----------|-----------|
| `GEST_GY_PROG_DEG_S` | 40 |
| `GEST_GY_CZAS_TRWANIA_MS` | 280 |
| `GEST_COOLDOWN_MS` | 2200 |
| `ODWROC_GEST_GY` | -1 (prawo przy tym montażu) |

### BLE

| Parametr | Domyślnie | Opis |
|----------|-----------|------|
| `CZAS_BEZCZYNNOSCI_BLE_MS` | 5 min | Po tym czasie bez aktywności — reklama BLE OFF |
| `PROG_PROBUDZENIA_BLE_DEG_S` | 12 °/s | Ruch głowy (|Gx|+|Gy|) budzi reklamę ze snu |

---

## Ogniwo, ładowanie i debug — USB ≠ ogniwo

### Jak płytka ładuje ogniwo (bez ADC w ESP32)

Na XIAO ESP32-S3 jest **układ zarządzania zasilaniem i ładowania** (Li-ion/Li-Pol z USB). **ESP32 nie musi znać procentu** — charger sam kończy ładowanie po napięciu/prądzie. Sygnalizacja:

1. Bez ogniwa, USB → czerwona LED ~30 s, potem gaśnie.  
2. Ogniwo + USB → LED **miga** (ładowanie).  
3. Pełne → LED **zgaszona**.

### Co pokazuje HEFAS (bez pomiaru napięcia)

| Kropka / pole | Znaczenie |
|---------------|-----------|
| **HID** | Host widzi mysz USB-HID (`tud_mounted()`) |
| **USB** | Kabel / 5 V (`tud_connected()` lub opcjonalnie `PIN_VBUS_ADC`) |
| **Ogn.** | Ogniwo na BAT+ (montaż — `HEFAS_OGNIOWO_ZAMONTOWANE`) |
| **Ład.** | Ogniwo + USB jednocześnie (ładowanie) |
| **BLE** (kropka) | Brak USB-HID → mysz po Bluetooth |
| **Zasil.** (panel) | Np. `Ogniwo + USB + ładowanie` lub `Ogniwo + mysz→BLE` |

**Częste mylenie:** kabel USB do ładowania ≠ mysz USB-HID (kropka USB może być zgaszona).  
Kropka **BLE** ≠ „naładowane” — tylko tryb bezprzewodowy. Rozładowane ogniwo = brak zasilania MCU, nie komunikat „podłącz USB”.

- **BLE:** reklama od startu bez USB-HID; uśpienie po 5 min; budzenie ruchem / mrugnięciem.  
- **Log `[ZAS]`** (filtr **Zasilanie**): przełączenie mysz BLE ↔ USB-HID.

**Montaż ogniwa:** BAT+ / BAT− na spodzie płytki; **NTC z 3-pinowego JST nie lutować**.

**Chcesz % w przyszłości:** zewnętrzny dzielnik napięcia na wolny pin ADC (np. D0) — patrz [FAQ Seeed – battery voltage](https://wiki.seeedstudio.com/check_battery_voltage/) (dla innych modeli XIAO; idea ta sama).

---

## WebDebug (WiFi AP)

Panel diagnostyczny działa **zawsze**, gdy `WEBDEBUG_AKTYWNY true` (niezależnie od flagi Debug). Sieć: **`HEFAS-Debug`**, hasło **`hefas1234`**, **`http://192.168.4.1`**.

### Kropki statusu (góra)

| Kropka | Znaczenie |
|--------|-----------|
| **HID** | Host widzi mysz USB-HID (`tud_mounted`) |
| **USB** | Kabel / 5 V (`tud_connected()` lub opcjonalnie `PIN_VBUS_ADC`) |
| **Ogn.** | Ogniwo na BAT+ (`HEFAS_OGNIOWO_ZAMONTOWANE`) |
| **Ład.** | Ogniwo + USB (ładowanie) |
| **Scroll** | Tryb scrolla (ruch głowy = przewijanie) |
| **Drag** | Przytrzymanie LPM (drag) |
| **Pauza** | Blokada wysyłania ruchu/klików (przycisk PAUZA) |
| **Debug** | `czyDebugWlaczony` — domyślnie **OFF**; **6× mrugnięcie** = przełącznik |
| **Oko** | Oko **potwierdzone** (liczy się do serii) |
| **BLE** | Brak USB-HID → mysz po Bluetooth |
| **Kal.** | Trwa rekalibracja MPU + TCRT (~3 s) |

### Panel boczny (liczby na żywo)

| Pole | Znaczenie |
|------|-----------|
| **L / R** | Liczniki kliknięć lewych / prawych (HID) |
| **dX / dY** | Ostatnia delta kursora |
| **SERIA** | Licznik impulsów w bieżącej serii mrugnięć |
| **cisza** | Pozostały czas do commitu akcji (zależy od N: 350…1000 ms) |
| **T** | Czas całej serii od 1. do ostatniego impulsu |
| **przerwa** | Ostatnia przerwa między otwarciami |
| **Kal.** | `TRWA ~3s` podczas kalibracji, inaczej `—` |
| **R / Fs / B** | TCRT: surowy ADC / fast (progi) / baseline |
| **V:Z / V:O, POTW** | Surowy stan progu / potwierdzenie zamknięcia |
| **Zasil.** | Skład: Ogniwo, USB-HID, USB, ładowanie, mysz→BLE |
| **BLE** | ON / SLEEP / OFF |

### Wykresy

- **Ruch głowy** — dX (czerwony), dY (cyjan); joystick = wektor chwilowy.
- **TCRT** — `tcrtFast` (progi); szare/czerwone/żółte linie = baseline / trigger / release; czerwone tło = oko potwierdzone zamknięte.

### Logi tekstowe (filtry)

Domyślnie włączone: **Mrugnięcia** (`WEBLOG_FSM`) + **Zasilanie** (`WEBLOG_BAT`). Opcjonalnie: Żyroskop, Czujnik IR, BLE/USB.

| Gdy **Debug OFF** (domyślnie) | Gdy **Debug ON** (6× mrugnięcie) |
|-------------------------------|----------------------------------|
| `[KAL] …` start/stabilizacja/gotowe (także z 5× mrugnięć) | + `[SERIA]`, `[KLIK]`, `[SCROLL]`, `[GEST]`, artefakty TCRT |
| `[DEBUG] WYLACZONY` / `WLACZONY` przy 6× | `[ZAS]`, `[TCRT]` okresowy, `[RUCH]` — według zaznaczonych filtrów |
| `[ZAS]` — jeśli filtr Zasilanie | Serial Monitor (115200) — pełne tagi |

**Kalibracja:** 5× mrugnięcie lub przycisk **REKALIBRACJA** → kropka **Kal.**, wpisy `[KAL] Start (5 mrug)` … `[KAL] Gotowe` (widoczne bez włączania Debug).

### Przyciski

- **PAUZA / WZNÓW** — blokuje ruch i kliknięcia (logika mrugnięć dalej działa).
- **REKALIBRACJA** — kalibracja MPU + TCRT (~3 s, async).
- **WYCZYŚĆ** — czyści historię logów w przeglądarce.

Wyłączenie całego AP: `WEBDEBUG_AKTYWNY false` w `hefas_config.h`.

---

## Serial Monitor (115200)

Domyślnie **`czyDebugWlaczony = false`** — po starcie Serial jest cichy (mniej obciążenia CPU). **6× mrugnięcie** włącza/wyłącza szczegółowe logi; kropka **Debug** w WebDebug odzwierciedla stan.

Przykładowe tagi (gdy Debug **ON**):

- `[KAL]` — offsety Gx, Gy, Gz, baseline TCRT  
- `[TCRT]` — Raw, Fast, Base, stan (co ~500 ms w `diagnostyka()`)  
- `[SERIA]` — commit serii (T, przerwa)  
- `[ZAS]` — zmiana trybu zasilania (filtr Zasilanie w WebDebug)  
- `[GEST]` — Ctrl+Win+O (USB)  
- `[RUCH]` / `[SCROLL]` / `[KLIK]` / `[DRAG]` / `[HID]` / `[BLE]`

Wpisy trafiają też do WebDebug zgodnie z maską filtrów (szczegóły mrugnięć/klików w panelu głównie przy **Debug ON**; `[KAL]` zawsze w filtrze Mrugnięcia).

---

## Architektura

HEFAS **cały czas** sprawdza ruch głowy i mrugnięcia, a potem rusza kursorem lub klika — jak zwykła myszka. Poniżej schematy **krok po kroku**, prostym językiem (bez nazw z kodu).

**Jak czytać schemat:** owal = start lub koniec · prostokąt = coś się dzieje · romb = pytanie tak/nie · strzałka = kolejny krok.

### 1. Co składa się na urządzenie?

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'lineColor': '#475569', 'clusterBkg': '#f8fafc', 'clusterBorder': '#cbd5e1', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569' }}}%%
flowchart LR
    subgraph WEJ["Co urządzenie „widzi”"]
        IMU["Czujnik ruchu głowy"]
        IR["Czujnik przy oku<br/>wykrywa mrugnięcia"]
        WEB["Strona w telefonie<br/>WiFi HEFAS-Debug"]
        USBIN["Kabel USB<br/>prąd i połączenie z PC"]
    end

    subgraph MCU["Mózg urządzenia"]
        LOOP["Non-stop analizuje<br/>co się dzieje"]
    end

    subgraph WYJ["Co urządzenie robi"]
        HID["Rusza kursorem<br/>przez kabel USB"]
        BLE["Rusza kursorem<br/>przez Bluetooth"]
        LED["Kolorowa dioda<br/>pokazuje stan"]
        LOG["Zapisuje informacje<br/>dla użytkownika i serwisu"]
    end

    IMU --> LOOP
    IR --> LOOP
    WEB --> LOOP
    USBIN --> LOOP
    LOOP --> HID
    LOOP --> BLE
    LOOP --> LED
    LOOP --> LOG

    classDef io fill:#dcfce7,stroke:#16a34a,stroke-width:2px,color:#14532d
    classDef core fill:#e0f2fe,stroke:#0284c7,stroke-width:2px,color:#0c4a6e
    class IMU,IR,WEB,USBIN,HID,BLE,LED,LOG io
    class LOOP core
```

### 2. Co się dzieje po włączeniu?

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'lineColor': '#475569', 'clusterBkg': '#f8fafc', 'clusterBorder': '#cbd5e1', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569' }}}%%
flowchart TD
    S([Włączasz urządzenie]) --> SER[Przygotowanie wewnętrzne]
    SER --> I2C[Sprawdza czujnik głowy]
    I2C -->|nie działa| BLINK[Dioda miga — czekaj<br/>na serwis]
    I2C -->|działa| KAL0["Dopasowuje się do Ciebie<br/>siedź spokojnie ~3 s"]
    KAL0 --> USB["Udaje myszkę komputera<br/>przez kabel USB"]
    USB --> ZAS0[Sprawdza skąd bierze prąd]
    ZAS0 --> WD["Włącza WiFi do podglądu<br/>sieć HEFAS-Debug"]
    WD --> BLE0["Szuka komputera/telefonu<br/>przez Bluetooth"]
    BLE0 --> E([Zaczyna normalną pracę])

    classDef startEnd fill:#ede9fe,stroke:#7c3aed,stroke-width:2px,color:#4c1d95
    classDef process fill:#e0f2fe,stroke:#0284c7,stroke-width:2px,color:#0c4a6e
    classDef bad fill:#fee2e2,stroke:#dc2626,stroke-width:2px,color:#991b1b
    class S,E startEnd
    class SER,I2C,KAL0,USB,ZAS0,WD,BLE0 process
    class BLINK bad
```

### 3. Ciągła praca — powtarza się w kółko

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'lineColor': '#475569', 'clusterBkg': '#f8fafc', 'clusterBorder': '#cbd5e1', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569' }}}%%
flowchart TD
    START([Od początku]) --> W1

    subgraph WEBDBG["Strona w telefonie / komputerze"]
        W1[Obsługuje panel w przeglądarce]
        W2{Nacisnięto Kalibruj?}
        W3[Ponowne dopasowanie do głowy]
        W1 --> W2
        W2 -->|tak| W3
    end

    W2 -->|nie| S1
    W3 --> S1

    subgraph SENS["Głowa i oko"]
        S1[Gdzie jest głowa?]
        S2[Czy to gest — szybki przechył?]
        S3[Puść wcześniejsze klawisze]
        S4[Czy oko otwarte czy zamknięte?]
        S1 --> S2 --> S3 --> S4
    end

    S4 --> Y1

    subgraph SYS["Sprzęt i połączenie"]
        Y1[Kabel USB czy Bluetooth?]
        Y2[Trwa dopasowanie do głowy?]
        Y3[Wykonaj zaplanowane kliknięcia]
        Y1 --> Y2 --> Y3
    end

    Y3 --> PAUZA{Myszka wstrzymana<br/>z panelu?}

    PAUZA -->|nie| A1
    PAUZA -->|tak| L1

    subgraph ACT["Sterowanie kursorem"]
        A1[Liczy mrugnięcia]
        A2{Głowa się ruszyła<br/>i nie trwa kliknięcie?}
        A3[Rusz kursor na ekranie]
        A4[Bez ruchu kursora]
        A1 --> A2
        A2 -->|tak| A3
        A2 -->|nie| A4
    end

    A3 --> L1
    A4 --> L1

    subgraph KONIEC["Koniec jednego obrotu"]
        L1[Zmień kolor diody statusu]
        L2[Szczegółowy log — tylko tryb serwisowy]
        L3[Krótka pauza]
        L1 --> L2 --> L3
    end

    L3 --> START

    classDef startEnd fill:#ede9fe,stroke:#7c3aed,stroke-width:2px,color:#4c1d95
    classDef process fill:#e0f2fe,stroke:#0284c7,stroke-width:2px,color:#0c4a6e
    classDef decision fill:#fef9c3,stroke:#ca8a04,stroke-width:2px,color:#854d0e
    class START startEnd
    class W1,W3,S1,S2,S3,S4,Y1,Y2,Y3,A1,A3,A4,L1,L2,L3 process
    class W2,PAUZA,A2 decision
```

### 4. Jak ruch głowy rusza kursorem?

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'lineColor': '#475569', 'clusterBkg': '#f8fafc', 'clusterBorder': '#cbd5e1', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569' }}}%%
flowchart TD
    R([Gdzie jest głowa?]) --> OFF[Odejmij pozycję spoczynkową]
    OFF --> BLEW[Obudź Bluetooth jeśli śpi]
    BLEW --> PROGX{Wyraźny ruch w bok?}
    PROGX -->|nie| GX0[Zignoruj]
    PROGX -->|tak| GX1[Uwzględnij]
    GX0 --> PROGZ
    GX1 --> PROGZ{Wyraźne kiwanie głową?<br/>przy przewijaniu — mocniejszy ruch}
    PROGZ -->|nie| GZ0[Zignoruj]
    PROGZ -->|tak| GZ1[Uwzględnij]
    GZ0 --> MARTWA
    GZ1 --> MARTWA[Odfiltruj drobne drgania]
    MARTWA --> DELTA[Przelicz na ruch kursora]
    DELTA --> G([Gest — szybki przechył w prawo])

    G --> GOK{Urządzenie gotowe<br/>oko otwarte<br/>myszka aktywna?}
    GOK -->|nie| GKON([Koniec gestu])
    GOK -->|tak| GY{Szybko pochyl głowę<br/>w prawo i przytrzymaj<br/>chwilę?}
    GY -->|tak| OSK{Podłączony kabel USB?}
    OSK -->|tak| KBD[Otwórz klawiaturę ekranową<br/>Windows: Ctrl+Win+O]
    OSK -->|nie| BLEONLY[Tylko zapis w logu<br/>Bluetooth bez klawiatury]
    GY -->|nie| GKON
    KBD --> GKON
    BLEONLY --> GKON

    classDef startEnd fill:#ede9fe,stroke:#7c3aed,stroke-width:2px,color:#4c1d95
    classDef process fill:#e0f2fe,stroke:#0284c7,stroke-width:2px,color:#0c4a6e
    classDef decision fill:#fef9c3,stroke:#ca8a04,stroke-width:2px,color:#854d0e
    classDef io fill:#dcfce7,stroke:#16a34a,stroke-width:2px,color:#14532d
    class R,G,GKON startEnd
    class OFF,BLEW,GX0,GX1,GZ0,GZ1,MARTWA,DELTA,KBD process
    class PROGX,PROGZ,GOK,GY,OSK decision
    class BLEONLY io
```

### 5. Prąd, kabel USB i Bluetooth

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'lineColor': '#475569', 'clusterBkg': '#f8fafc', 'clusterBorder': '#cbd5e1', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569' }}}%%
flowchart TD
    Z([Sprawdź połączenie i prąd]) --> ST[Skąd jest zasilanie?]
    ST --> FL["Kabel USB, bateria,<br/>ładowanie — bez % baterii"]
    FL --> CHG{Cokolwiek się zmieniło?}
    CHG -->|tak| LOGZ[Pokaż na panelu w przeglądarce]
    CHG -->|nie| USBCHK
    LOGZ --> USBCHK{Komputer widzi myszkę<br/>przez kabel?}

    USBCHK -->|tak| OFF[Ukryj Bluetooth<br/>kabel wystarczy]
    USBCHK -->|nie| CON{Bluetooth w użyciu<br/>lub niedawno używany?}
    CON -->|tak| ON[Szukaj urządzenia<br/>przez Bluetooth]
    CON -->|nie| SLEEP[Wyłącz Bluetooth<br/>żeby oszczędzać prąd]

    classDef startEnd fill:#ede9fe,stroke:#7c3aed,stroke-width:2px,color:#4c1d95
    classDef process fill:#e0f2fe,stroke:#0284c7,stroke-width:2px,color:#0c4a6e
    classDef decision fill:#fef9c3,stroke:#ca8a04,stroke-width:2px,color:#854d0e
    class Z startEnd
    class ST,FL,LOGZ,OFF,ON,SLEEP process
    class CHG,USBCHK,CON decision
```

### 6. Dopasowanie do użytkownika (kalibracja)

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'lineColor': '#475569', 'clusterBkg': '#f8fafc', 'clusterBorder': '#cbd5e1', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569' }}}%%
flowchart TD
    KTRIG["Start: włączenie · 5 mrugnięć · przycisk Kalibruj"] -.-> K0
    K0([Zacznij dopasowanie]) --> K1["Trwa uczenie — siedź spokojnie"]
    K1 --> T1

    subgraph TICK["Trwa ok. 3 sekundy"]
        T1["Zbiera pomiary głowy i oka"]
        T2["Czeka aż sygnał z oka się uspokoi"]
        T3[Kończy uczenie]
        T1 --> T2 --> T3
    end

    T3 --> K4["Zapamiętuje Twoją pozycję<br/>dioda mruga 3× — gotowe"]
    K4 --> KON([Można normalnie korzystać])

    classDef startEnd fill:#ede9fe,stroke:#7c3aed,stroke-width:2px,color:#4c1d95
    classDef process fill:#e0f2fe,stroke:#0284c7,stroke-width:2px,color:#0c4a6e
    classDef trigger fill:#f1f5f9,stroke:#64748b,stroke-width:1px,color:#475569
    class K0,KON startEnd
    class K1,T1,T2,T3,K4 process
    class KTRIG trigger
```

### 7. Jak urządzenie wie, że mrugnąłeś?

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'lineColor': '#475569', 'clusterBkg': '#f8fafc', 'clusterBorder': '#cbd5e1', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569' }}}%%
flowchart TD
    A([Sprawdź czujnik przy oku]) --> ADC[Odczytaj sygnał]
    ADC --> EMA[Wygładź — mniej fałszywych alarmów]

    EMA --> OTW{Oko otwarte?}

    OTW -->|tak| TRIG{Sygnał słabnie — mrugnąłeś?}
    TRIG -->|tak| ZAM[Zapisz: oko zamknięte]
    TRIG -->|nie| FRZ{Czekamy na serię mrugnięć?}
    FRZ -->|nie| ADAPT[Powoli dopasuj poziom spoczynku]
    FRZ -->|tak| KON

    OTW -->|nie| REL{Sygnał rośnie — otworzyłeś oko?}
    REL -->|tak| OOTW[Zapisz: oko otwarte]
    REL -->|nie| MECH{Coś zasłoniło czujnik?}
    MECH -->|tak| MECHR[Traktuj jak otwarte oko]
    MECH -->|nie| KON

    ZAM --> KON([Koniec])
    ADAPT --> KON
    OOTW --> KON
    MECHR --> KON

    NOTE["Nie zmienia poziomu gdy:<br/>liczy serię mrugnięć, przeciągasz, oko długo zamknięte"] -.-> FRZ

    classDef startEnd fill:#ede9fe,stroke:#7c3aed,stroke-width:2px,color:#4c1d95
    classDef process fill:#e0f2fe,stroke:#0284c7,stroke-width:2px,color:#0c4a6e
    classDef decision fill:#fef9c3,stroke:#ca8a04,stroke-width:2px,color:#854d0e
    classDef note fill:#f1f5f9,stroke:#64748b,stroke-width:1px,color:#475569
    class A,KON startEnd
    class ADC,EMA,ZAM,ADAPT,OOTW,MECHR process
    class OTW,TRIG,FRZ,REL,MECH decision
    class NOTE note
```

### 8. Mrugnięcia — od sygnału do kliknięcia

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'lineColor': '#475569', 'clusterBkg': '#f8fafc', 'clusterBorder': '#cbd5e1', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569' }}}%%
flowchart TD
    M0([Obsługa mrugnięć]) --> RDY{System skalibrowany?}
    RDY -->|nie| MX([Koniec])
    RDY -->|tak| CNT["Co 10 ms: licz próbki<br/>otwarte / zamknięte"]

    CNT --> C1{Zamknięcie oka<br/>potwierdzone?}
    C1 -->|tak| CS[Zapisz czas zamknięcia]
    C1 -->|nie| C2{Otwarcie po zamknięciu?}

    C2 -->|tak| EDGE[Obsłuż otwarcie oka]
    C2 -->|nie| C3{Oko zamknięte<br/>ponad 850 ms?}
    C3 -->|tak| DRG[Przeciąganie — wciśnij LPM]
    C3 -->|nie| TM

    EDGE --> E1{Było przeciąganie?}
    E1 -->|tak| DRGOFF[Puść przycisk]
    E1 -->|nie| E2{Czas zamknięcia}
    E2 -->|mniej niż 42 ms| ART[Odrzuć szum]
    E2 -->|42 do 280 ms| REG[Dodaj mrugnięcie do serii]
    E2 -->|280 do 850 ms| STREFA[Strefa bez kliku]
    E2 -->|więcej niż 850 ms| STREFA

    REG --> REGD["Okno 600 ms między otwarciami<br/>cisza po N: 350…1000 ms"]

    CS --> TM
    DRG --> TM
    DRGOFF --> TM
    ART --> TM
    STREFA --> TM
    REGD --> TM[Sprawdź pauzę w serii]

    TM --> T1{Oko otwarte<br/>i minęła cisza?}
    T1 -->|nie| MX
    T1 -->|tak| T2{Czas serii poprawny?<br/>3×: 120–1300 ms}
    T2 -->|nie| DROP[Odrzuć serię]
    T2 -->|tak| ACT[Wykonaj akcję mrugnięcia]
    DROP --> MX
    ACT --> MX

    classDef startEnd fill:#ede9fe,stroke:#7c3aed,stroke-width:2px,color:#4c1d95
    classDef process fill:#e0f2fe,stroke:#0284c7,stroke-width:2px,color:#0c4a6e
    classDef decision fill:#fef9c3,stroke:#ca8a04,stroke-width:2px,color:#854d0e
    classDef warn fill:#fee2e2,stroke:#dc2626,stroke-width:2px,color:#991b1b
    class M0,MX startEnd
    class CNT,CS,EDGE,DRG,DRGOFF,REG,REGD,TM,ACT process
    class RDY,C1,C2,C3,E1,E2,T1,T2 decision
    class ART,STREFA,DROP warn
```

### 9. Akcje po serii mrugnięć

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'lineColor': '#475569', 'clusterBkg': '#f8fafc', 'clusterBorder': '#cbd5e1', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569' }}}%%
flowchart TD
    P([Liczba mrugnięć N]) --> S6{N = 6?}
    S6 -->|tak| DBG[Przełącz tryb Debug]
    S6 -->|nie| S5{N = 5?}
    S5 -->|tak| K5[Rekalibracja żyroskopu i czujnika]
    S5 -->|nie| SCR{Scroll włączony<br/>i N ≥ 2?}

    SCR -->|tak| SOFF[Wyłącz scroll]
    SOFF --> S2{N = 2?}
    S2 -->|tak| PEND([Koniec])
    S2 -->|nie| S4A

    SCR -->|nie| S4A{N = 4<br/>i scroll wyłączony?}
    S4A -->|tak| SON[Włącz scroll]
    S4A -->|nie| SCRC{Scroll aktywny?}

    SON --> PEND
    SCRC -->|tak| PEND
    SCRC -->|nie| N1{N = 1?}
    N1 -->|tak| LPM[Pojedyncze kliknięcie LPM]
    N1 -->|nie| N2{N = 2?}
    N2 -->|tak| DBL[Podwójne kliknięcie LPM]
    N2 -->|nie| N3{N = 3?}
    N3 -->|tak| PPM[Kliknięcie PPM]
    N3 -->|nie| PEND

    LPM --> Q[Wyślij kliknięcie]
    DBL --> Q
    PPM --> Q
    Q --> U{Połączenie USB?}
    U -->|tak| USBM[Mysz przez USB]
    U -->|nie| BLEM[Mysz przez Bluetooth]
    DBG --> PEND
    K5 --> PEND

    classDef startEnd fill:#ede9fe,stroke:#7c3aed,stroke-width:2px,color:#4c1d95
    classDef process fill:#e0f2fe,stroke:#0284c7,stroke-width:2px,color:#0c4a6e
    classDef decision fill:#fef9c3,stroke:#ca8a04,stroke-width:2px,color:#854d0e
    classDef io fill:#dcfce7,stroke:#16a34a,stroke-width:2px,color:#14532d
    class P,PEND startEnd
    class DBG,K5,SOFF,SON,LPM,DBL,PPM,Q process
    class S6,S5,SCR,S2,S4A,SCRC,N1,N2,N3,U decision
    class USBM,BLEM io
```

### 10. Wysyłanie ruchu kursora

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'lineColor': '#475569', 'clusterBkg': '#f8fafc', 'clusterBorder': '#cbd5e1', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569' }}}%%
flowchart LR
    R([Ruch głowy X/Y]) --> SC{Tryb scrolla?}
    SC -->|tak| WH["Przewijanie: ruch w górę/dół ÷ 7<br/>próg kiwania 3°/s"]
    SC -->|nie| MV[Przesunięcie kursora]
    WH --> CH{Połączenie USB?}
    MV --> CH
    CH -->|tak| UM[Wyślij przez USB]
    CH -->|nie| BM{Bluetooth połączony?}
    BM -->|tak| BL[Wyślij przez Bluetooth]
    BM -->|nie| X([Brak wysyłki])

    classDef startEnd fill:#ede9fe,stroke:#7c3aed,stroke-width:2px,color:#4c1d95
    classDef process fill:#e0f2fe,stroke:#0284c7,stroke-width:2px,color:#0c4a6e
    classDef decision fill:#fef9c3,stroke:#ca8a04,stroke-width:2px,color:#854d0e
    classDef io fill:#dcfce7,stroke:#16a34a,stroke-width:2px,color:#14532d
    class R,X startEnd
    class WH,MV process
    class SC,CH,BM decision
    class UM,BL io
```

### Legenda kolorów (wszystkie diagramy)

| Kolor | Znaczenie |
|-------|-----------|
| **Fiolet** | Start / koniec (terminator) |
| **Niebieski** | Proces — obliczenia, stany |
| **Żółty** | Decyzja — warunek tak/nie |
| **Zielony** | Wejście/wyjście — USB, Bluetooth, urządzenia |
| **Czerwony** | Odrzucenie, błąd, zatrzymanie |
| **Szary** | Notatka / wyzwalacz pomocniczy |

Tło każdego schematu: **białe** — wymuszone dyrektywą `init` na początku każdego bloku Mermaid (`theme: default`, `darkMode: false`, tło `#ffffff`; czytelne też w Dark Mode na GitHubie). Grupy mają delikatne tło `#f8fafc`.

**Podgląd:** GitHub, GitLab, VS Code / Cursor — podgląd Markdown z Mermaid. Etykiety opisują **co robi system**, nie nazwy funkcji z kodu.

---

## Autorzy

Bartłomiej Adamczyk, Sebastian Sobczyk  
Mechatronika — Szczecin 2026
