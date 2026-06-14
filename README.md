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

Poniżej **diagramy przepływu** systemu HEFAS — styl **bezosobowy i funkcjonalny** (zgodny z dobrymi praktykami dokumentacji inżynieryjskiej). Opisują zachowanie systemu i stany obiektów, bez zwrotów do użytkownika w 2. osobie.

**Konwencja:** owal = START / KONIEC · prostokąt = proces · romb = warunek (TAK / NIE) · strzałka = kolejny krok. **Kolory** — legenda na końcu sekcji.

### 1. Struktura systemu HEFAS

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'secondaryTextColor': '#334155', 'lineColor': '#64748b', 'primaryBorderColor': '#94a3b8', 'clusterBkg': '#f1f5f9', 'clusterBorder': '#94a3b8', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569', 'fontFamily': 'arial' }}}%%
flowchart LR
    subgraph WEJ["Wejścia"]
        IMU["Czujnik żyroskopowy<br/>ruch głowy"]
        IR["Czujnik optyczny oka<br/>mrugnięcia"]
        WEB["Interfejs WiFi<br/>WebDebug"]
        USBIN["Interfejs USB-C<br/>zasilanie, HID"]
    end

    subgraph MCU["Sterownik centralny"]
        LOOP["Pętla przetwarzania<br/>głównego"]
    end

    subgraph WYJ["Wyjścia"]
        HID["HID USB — mysz"]
        BLE["BLE — mysz"]
        LED["Wskaźnik LED statusu"]
        LOG["Log diagnostyczny<br/>Serial, WebDebug"]
    end

    IMU --> LOOP
    IR --> LOOP
    WEB --> LOOP
    USBIN --> LOOP
    LOOP --> HID
    LOOP --> BLE
    LOOP --> LED
    LOOP --> LOG

    classDef input fill:#d1fae5,stroke:#059669,stroke-width:2px,color:#065f46
    classDef core fill:#e0e7ff,stroke:#4f46e5,stroke-width:2.5px,color:#312e81
    classDef output fill:#ffedd5,stroke:#ea580c,stroke-width:2px,color:#9a3412
    class IMU,IR,WEB,USBIN input
    class LOOP core
    class HID,BLE,LED,LOG output
    style WEJ fill:#ecfdf5,stroke:#6ee7b7,stroke-width:2px,color:#065f46
    style MCU fill:#eef2ff,stroke:#a5b4fc,stroke-width:2px,color:#312e81
    style WYJ fill:#fff7ed,stroke:#fdba74,stroke-width:2px,color:#9a3412
```

### 2. Sekwencja inicjalizacji (START)

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'secondaryTextColor': '#334155', 'lineColor': '#64748b', 'primaryBorderColor': '#94a3b8', 'clusterBkg': '#f1f5f9', 'clusterBorder': '#94a3b8', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569', 'fontFamily': 'arial' }}}%%
flowchart TD
    S([START — inicjalizacja]) --> SER[Inicjalizacja komunikacji wewnętrznej]
    SER --> I2C[Test połączenia czujnika żyroskopowego]
    I2C -->|NIE| BLINK[Sygnalizacja błędu — zatrzymanie]
    I2C -->|TAK| KAL0["Uruchomienie kalibracji<br/>~3 s"]
    KAL0 --> USB[Inicjalizacja HID USB — mysz]
    USB --> ZAS0[Odczyt stanu zasilania]
    ZAS0 --> WD["Uruchomienie AP WiFi<br/>HEFAS-Debug"]
    WD --> BLE0["Reklama BLE<br/>gdy brak USB-HID"]
    BLE0 --> E([Wejście do pętli głównej])

    classDef startEnd fill:#f3e8ff,stroke:#9333ea,stroke-width:2px,color:#581c87
    classDef process fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a8a
    classDef bad fill:#fecaca,stroke:#dc2626,stroke-width:2px,color:#7f1d1d
    classDef setup fill:#e0e7ff,stroke:#6366f1,stroke-width:2px,color:#3730a3
    class S,E startEnd
    class SER,I2C,USB,ZAS0,WD,BLE0 process
    class KAL0 setup
    class BLINK bad
```

### 3. Pętla główna — jeden cykl

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'secondaryTextColor': '#334155', 'lineColor': '#64748b', 'primaryBorderColor': '#94a3b8', 'clusterBkg': '#f1f5f9', 'clusterBorder': '#94a3b8', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569', 'fontFamily': 'arial' }}}%%
flowchart TD
    START([START cyklu]) --> W1

    subgraph WEBDBG["Moduł WebDebug"]
        W1[Obsługa panelu HTTP]
        W2{Czy żądanie rekalibracji?}
        W3[Uruchomienie kalibracji]
        W1 --> W2
        W2 -->|TAK| W3
    end

    W2 -->|NIE| S1
    W3 --> S1

    subgraph SENS["Moduł sensory i gest"]
        S1[Odczyt żyroskopu]
        S2[Detekcja gestu przechyłu]
        S3[Zwolnienie klawiszy gestu]
        S4[Odczyt czujnika oka]
        S1 --> S2 --> S3 --> S4
    end

    S4 --> Y1

    subgraph SYS["Moduł systemowy"]
        Y1[Obsługa USB i Bluetooth]
        Y2[Aktualizacja kalibracji]
        Y3[Wykonanie kolejki kliknięć HID]
        Y1 --> Y2 --> Y3
    end

    Y3 --> PAUZA{Czy sterowanie myszą<br/>wstrzymane?}

    PAUZA -->|NIE| A1
    PAUZA -->|TAK| L1

    subgraph ACT["Sterowanie kursorem"]
        A1[Obliczenia serii mrugnięć]
        A2{Czy wykryto ruch głowy<br/>i brak aktywnego kliknięcia?}
        A3[Wysłanie ruchu kursora]
        A4[Brak ruchu kursora]
        A1 --> A2
        A2 -->|TAK| A3
        A2 -->|NIE| A4
    end

    A3 --> L1
    A4 --> L1

    subgraph KONIEC["Zakończenie cyklu"]
        L1[Aktualizacja LED statusu]
        L2[Log diagnostyczny — tryb serwisowy]
        L3[Opóźnienie cyklu]
        L1 --> L2 --> L3
    end

    L3 --> START

    classDef startEnd fill:#f3e8ff,stroke:#9333ea,stroke-width:2px,color:#581c87
    classDef process fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a8a
    classDef decision fill:#fef3c7,stroke:#d97706,stroke-width:2.5px,color:#78350f
    classDef success fill:#bbf7d0,stroke:#16a34a,stroke-width:2px,color:#14532d
    class START startEnd
    class W1,W3,S1,S2,S3,S4,Y1,Y2,Y3,A1,A4,L1,L2,L3 process
    class W2,PAUZA,A2 decision
    class A3 success
    style WEBDBG fill:#faf5ff,stroke:#c4b5fd,stroke-width:2px
    style SENS fill:#ecfdf5,stroke:#6ee7b7,stroke-width:2px
    style SYS fill:#fff7ed,stroke:#fdba74,stroke-width:2px
    style ACT fill:#eff6ff,stroke:#93c5fd,stroke-width:2px
    style KONIEC fill:#f8fafc,stroke:#cbd5e1,stroke-width:2px
```

### 4. Przetwarzanie ruchu głowy (IMU)

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'secondaryTextColor': '#334155', 'lineColor': '#64748b', 'primaryBorderColor': '#94a3b8', 'clusterBkg': '#f1f5f9', 'clusterBorder': '#94a3b8', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569', 'fontFamily': 'arial' }}}%%
flowchart TD
    R([START — odczyt żyroskopu]) --> OFF[Korekcja offsetu kalibracji]
    OFF --> BLEW[Aktywacja BLE ruchem]
    BLEW --> PROGX{Czy |Gx| > próg?}
    PROGX -->|NIE| GX0[Gx := 0]
    PROGX -->|TAK| GX1[Zachowanie Gx]
    GX0 --> PROGZ
    GX1 --> PROGZ{Czy |Gz| > próg?<br/>tryb scroll: próg podwyższony}
    PROGZ -->|NIE| GZ0[Gz := 0]
    PROGZ -->|TAK| GZ1[Zachowanie Gz]
    GZ0 --> MARTWA
    GZ1 --> MARTWA[Filtr strefy martwej]
    MARTWA --> DELTA[Obliczenie delty kursora]
    DELTA --> G([Detekcja gestu przechyłu])

    G --> GOK{Czy system gotowy<br/>i oko otwarte<br/>i sterowanie aktywne?}
    GOK -->|NIE| GKON([KONIEC gestu])
    GOK -->|TAK| GY{Czy przechył Gy w prawo<br/>powyżej progu i czasu?}
    GY -->|TAK| OSK{Czy połączenie USB-HID?}
    OSK -->|TAK| KBD[Wysłanie skrótu Ctrl+Win+O]
    OSK -->|NIE| BLEONLY[Zapis do logu — BLE bez klawiatury]
    GY -->|NIE| GKON
    KBD --> GKON
    BLEONLY --> GKON

    classDef startEnd fill:#f3e8ff,stroke:#9333ea,stroke-width:2px,color:#581c87
    classDef process fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a8a
    classDef decision fill:#fef3c7,stroke:#d97706,stroke-width:2.5px,color:#78350f
    classDef connect fill:#ccfbf1,stroke:#0d9488,stroke-width:2px,color:#115e59
    classDef success fill:#bbf7d0,stroke:#16a34a,stroke-width:2px,color:#14532d
    class R,G,GKON startEnd
    class OFF,BLEW,GX0,GX1,GZ0,GZ1,MARTWA,DELTA process
    class PROGX,PROGZ,GOK,GY,OSK decision
    class KBD success
    class BLEONLY connect
```

### 5. Zasilanie i łączność USB/BLE

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'secondaryTextColor': '#334155', 'lineColor': '#64748b', 'primaryBorderColor': '#94a3b8', 'clusterBkg': '#f1f5f9', 'clusterBorder': '#94a3b8', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569', 'fontFamily': 'arial' }}}%%
flowchart TD
    Z([START — zasilanie i BLE]) --> ST[Odczyt stanu zasilania]
    ST --> FL[Aktualizacja flag zasilania]
    FL --> CHG{Czy stan się zmienił?}
    CHG -->|TAK| LOGZ[Zapis zdarzenia ZAS — WebDebug]
    CHG -->|NIE| USBCHK
    LOGZ --> USBCHK{Czy host widzi USB-HID?}

    USBCHK -->|TAK| OFF[Wyłączenie reklamy BLE]
    USBCHK -->|NIE| CON{Czy BLE aktywny<br/>lub aktywność &lt; 5 min?}
    CON -->|TAK| ON[Włączenie reklamy BLE]
    CON -->|NIE| SLEEP[Tryb uśpienia BLE]

    classDef startEnd fill:#f3e8ff,stroke:#9333ea,stroke-width:2px,color:#581c87
    classDef process fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a8a
    classDef decision fill:#fef3c7,stroke:#d97706,stroke-width:2.5px,color:#78350f
    class Z startEnd
    class ST,FL,LOGZ,OFF,ON,SLEEP process
    class CHG,USBCHK,CON decision
```

### 6. Procedura kalibracji

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'secondaryTextColor': '#334155', 'lineColor': '#64748b', 'primaryBorderColor': '#94a3b8', 'clusterBkg': '#f1f5f9', 'clusterBorder': '#94a3b8', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569', 'fontFamily': 'arial' }}}%%
flowchart TD
    KTRIG["Wyzwalacze: START, 5× mrugnięcie, WebDebug"] -.-> K0
    K0([START kalibracji]) --> K1[Ustawienie flagi: kalibracja trwa]
    K1 --> T1

    subgraph TICK["Faza pomiarowa ~3 s"]
        T1[Zbieranie próbek IMU i czujnika oka]
        T2[Stabilizacja sygnału oka]
        T3[Zakończenie kalibracji]
        T1 --> T2 --> T3
    end

    T3 --> K4["Zapis poziomów · system gotowy<br/>sygnalizacja LED 3×"]
    K4 --> KON([KONIEC kalibracji])

    classDef startEnd fill:#f3e8ff,stroke:#9333ea,stroke-width:2px,color:#581c87
    classDef process fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a8a
    classDef trigger fill:#f8fafc,stroke:#64748b,stroke-width:1px,color:#475569
    class K0,KON startEnd
    class K1,T1,T2,T3,K4 process
    class KTRIG trigger
    style TICK fill:#eff6ff,stroke:#93c5fd,stroke-width:2px
```

### 7. Detekcja mrugnięć — czujnik oka

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'secondaryTextColor': '#334155', 'lineColor': '#64748b', 'primaryBorderColor': '#94a3b8', 'clusterBkg': '#f1f5f9', 'clusterBorder': '#94a3b8', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569', 'fontFamily': 'arial' }}}%%
flowchart TD
    A([START — odczyt czujnika oka]) --> ADC[Pobranie sygnału z czujnika]
    ADC --> EMA[Filtracja sygnału — wygładzanie]

    EMA --> OTW{Czy oko jest otwarte?}

    OTW -->|TAK| TRIG{Czy sygnał maleje — mrugnięcie?}
    TRIG -->|TAK| ZAM[Zapisz stan: Oko zamknięte]
    TRIG -->|NIE| FRZ{Czy zablokowana adaptacja?}
    FRZ -->|NIE| ADAPT[Adaptacja poziomu spoczynku]
    FRZ -->|TAK| KON

    OTW -->|NIE| REL{Czy sygnał rośnie — otwarcie?}
    REL -->|TAK| OOTW[Zapisz stan: Oko otwarte]
    REL -->|NIE| MECH{Czy wykryto zasłonięcie czujnika?}
    MECH -->|TAK| MECHR[Ustaw stan: Oko otwarte]
    MECH -->|NIE| KON

    ZAM --> KON([KONIEC])
    ADAPT --> KON
    OOTW --> KON
    MECHR --> KON

    NOTE["Blokada adaptacji poziomu spoczynku:<br>- zliczanie serii mrugnięć<br>- tryb przeciągania drag<br>- długie zamknięcie oka"] -.-> FRZ

    classDef startEnd fill:#f3e8ff,stroke:#9333ea,stroke-width:2px,color:#581c87
    classDef process fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a8a
    classDef decision fill:#fef3c7,stroke:#d97706,stroke-width:2.5px,color:#78350f
    classDef note fill:#f1f5f9,stroke:#94a3b8,stroke-width:1px,color:#334155
    class A,KON startEnd
    class ADC,EMA,ZAM,ADAPT,OOTW,MECHR process
    class OTW,TRIG,FRZ,REL,MECH decision
    class NOTE note
```

### 8. Logika mrugnięć — od detekcji do akcji

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'secondaryTextColor': '#334155', 'lineColor': '#64748b', 'primaryBorderColor': '#94a3b8', 'clusterBkg': '#f1f5f9', 'clusterBorder': '#94a3b8', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569', 'fontFamily': 'arial' }}}%%
flowchart TD
    M0([START — obliczenia mrugnięć]) --> RDY{Czy system skalibrowany?}
    RDY -->|NIE| MX([KONIEC])
    RDY -->|TAK| CNT[Próbkowanie stanu oka — cykl]

    CNT --> C1{Czy potwierdzone zamknięcie oka?}
    C1 -->|TAK| CS[Zapis czasu zamknięcia]
    C1 -->|NIE| C2{Czy wykryto otwarcie<br/>po zamknięciu?}

    C2 -->|TAK| EDGE[Obsługa zbocza otwarcia]
    C2 -->|NIE| C3{Czy oko zamknięte &gt; 850 ms?}
    C3 -->|TAK| DRG[Aktywacja trybu drag — LPM]
    C3 -->|NIE| TM

    EDGE --> E1{Czy tryb drag aktywny?}
    E1 -->|TAK| DRGOFF[Dezaktywacja drag]
    E1 -->|NIE| E2{Klasyfikacja czasu zamknięcia}
    E2 -->|&lt; 42 ms| ART[Odrzucenie impulsu — szum]
    E2 -->|42–280 ms| REG[Rejestracja mrugnięcia w serii]
    E2 -->|&gt; 280 ms| STREFA[Strefa bez akcji kliknięcia]

    REG --> REGD[Ocena czasu serii mrugnięć]

    CS --> TM
    DRG --> TM
    DRGOFF --> TM
    ART --> TM
    STREFA --> TM
    REGD --> TM[Sprawdzenie timeout serii]

    TM --> T1{Czy oko otwarte<br/>i minął odstęp?}
    T1 -->|NIE| MX
    T1 -->|TAK| T2{Czy czas serii<br/>w zakresie dopuszczalnym?}
    T2 -->|NIE| DROP[Odrzucenie serii]
    T2 -->|TAK| ACT[Wykonanie akcji przypisanej do serii]
    DROP --> MX
    ACT --> MX

    classDef startEnd fill:#f3e8ff,stroke:#9333ea,stroke-width:2px,color:#581c87
    classDef process fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a8a
    classDef decision fill:#fef3c7,stroke:#d97706,stroke-width:2.5px,color:#78350f
    classDef warn fill:#fed7aa,stroke:#c2410c,stroke-width:2px,color:#7c2d12
    classDef bad fill:#fecaca,stroke:#dc2626,stroke-width:2px,color:#7f1d1d
    classDef success fill:#bbf7d0,stroke:#16a34a,stroke-width:2px,color:#14532d
    class M0,MX startEnd
    class CNT,CS,EDGE,DRG,DRGOFF,REG,REGD,TM process
    class RDY,C1,C2,C3,E1,E2,T1,T2 decision
    class ACT success
    class ART,STREFA warn
    class DROP bad
```

### 9. Mapowanie liczby mrugnięć na akcje

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'secondaryTextColor': '#334155', 'lineColor': '#64748b', 'primaryBorderColor': '#94a3b8', 'clusterBkg': '#f1f5f9', 'clusterBorder': '#94a3b8', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569', 'fontFamily': 'arial' }}}%%
flowchart TD
    P([START — licznik mrugnięć N]) --> S6{Czy N = 6?}
    S6 -->|TAK| DBG[Przełączenie trybu serwisowego]
    S6 -->|NIE| S5{Czy N = 5?}
    S5 -->|TAK| K5[Uruchomienie rekalibracji]
    S5 -->|NIE| SCR{Czy scroll aktywny<br/>i N ≥ 2?}

    SCR -->|TAK| SOFF[Wyłączenie trybu scroll]
    SOFF --> S2{Czy N = 2?}
    S2 -->|TAK| PEND([KONIEC])
    S2 -->|NIE| S4A

    SCR -->|NIE| S4A{Czy N = 4<br/>i scroll wyłączony?}
    S4A -->|TAK| SON[Włączenie trybu scroll]
    S4A -->|NIE| SCRC{Czy scroll aktywny?}

    SON --> PEND
    SCRC -->|TAK| PEND
    SCRC -->|NIE| N1{Czy N = 1?}
    N1 -->|TAK| LPM[Kliknięcie LPM]
    N1 -->|NIE| N2{Czy N = 2?}
    N2 -->|TAK| DBL[Podwójne kliknięcie LPM]
    N2 -->|NIE| N3{Czy N = 3?}
    N3 -->|TAK| PPM[Kliknięcie PPM]
    N3 -->|NIE| PEND

    LPM --> Q[Kolejkowanie kliknięcia HID]
    DBL --> Q
    PPM --> Q
    Q --> U{Czy połączenie USB-HID?}
    U -->|TAK| USBM[Wysłanie przez USB]
    U -->|NIE| BLEM[Wysłanie przez BLE]
    DBG --> PEND
    K5 --> PEND

    classDef startEnd fill:#f3e8ff,stroke:#9333ea,stroke-width:2px,color:#581c87
    classDef process fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a8a
    classDef decision fill:#fef3c7,stroke:#d97706,stroke-width:2.5px,color:#78350f
    classDef connect fill:#ccfbf1,stroke:#0d9488,stroke-width:2px,color:#115e59
    classDef success fill:#bbf7d0,stroke:#16a34a,stroke-width:2px,color:#14532d
    classDef setup fill:#e0e7ff,stroke:#6366f1,stroke-width:2px,color:#3730a3
    class P,PEND startEnd
    class SOFF,SON,Q process
    class DBG,K5 setup
    class LPM,DBL,PPM success
    class S6,S5,SCR,S2,S4A,SCRC,N1,N2,N3,U decision
    class USBM,BLEM connect
```

### 10. Wysyłka ruchu kursora

```mermaid
%%{init: { 'theme': 'default', 'themeVariables': { 'darkMode': false, 'background': '#ffffff', 'mainBkg': '#ffffff', 'primaryColor': '#ffffff', 'secondaryColor': '#ffffff', 'tertiaryColor': '#ffffff', 'primaryTextColor': '#1e293b', 'secondaryTextColor': '#334155', 'lineColor': '#64748b', 'primaryBorderColor': '#94a3b8', 'clusterBkg': '#f1f5f9', 'clusterBorder': '#94a3b8', 'edgeLabelBackground': '#ffffff', 'noteBkgColor': '#f8fafc', 'noteTextColor': '#475569', 'fontFamily': 'arial' }}}%%
flowchart LR
    R([START — delta ruchu]) --> SC{Czy tryb scroll aktywny?}
    SC -->|TAK| WH[Obliczenie scroll — oś Z]
    SC -->|NIE| MV[Obliczenie przesunięcia kursora]
    WH --> CH{Czy połączenie USB-HID?}
    MV --> CH
    CH -->|TAK| UM[Wysłanie ruchu — USB]
    CH -->|NIE| BM{Czy BLE połączony?}
    BM -->|TAK| BL[Wysłanie ruchu — BLE]
    BM -->|NIE| X([KONIEC — brak transmisji])

    classDef startEnd fill:#f3e8ff,stroke:#9333ea,stroke-width:2px,color:#581c87
    classDef process fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a8a
    classDef decision fill:#fef3c7,stroke:#d97706,stroke-width:2.5px,color:#78350f
    classDef connect fill:#ccfbf1,stroke:#0d9488,stroke-width:2px,color:#115e59
    class R,X startEnd
    class WH,MV process
    class SC,CH,BM decision
    class UM,BL connect
```

### Legenda kolorów

| Kolor | Znaczenie (typ bloku) |
|-------|------------------------|
| **Fiolet** | START / KONIEC |
| **Niebieski** | Proces — operacja systemu |
| **Bursztyn** | Warunek — odpowiedź TAK / NIE |
| **Zielony jasny** | Wejście — czujniki, interfejsy |
| **Pomarańczowy** | Wyjście — HID, LED, log |
| **Indygo** | Przetwarzanie centralne |
| **Indygo jasny** | Kalibracja, tryb serwisowy |
| **Miętowy** | Transmisja USB / BLE |
| **Zielony** | Akcja wykonana pomyślnie |
| **Czerwony** | Błąd lub odrzucenie |
| **Pomarańczowy jasny** | Ostrzeżenie — brak akcji |
| **Szary** | Notatka, wyzwalacz |

Tło schematów: **białe** (`darkMode: false` w dyrektywie `init`).

**Podgląd:** GitHub, GitLab, VS Code / Cursor. Parametry liczbowe (ms, progi) — w sekcjach technicznych powyżej.

---

## Autorzy

Bartłomiej Adamczyk, Sebastian Sobczyk  
Mechatronika — Szczecin 2026
