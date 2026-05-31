# HEFAS 4.0 - sterowanie myszka ruchem glowy

System bezdotykowej obslugi kursora myszy ruchami glowy dla osob z niepelnosprawnosciami.
**HEFAS** = Head-controlled Electronic Functional Assistive System.

Platforma: **Seeed Studio XIAO ESP32-S3 Plus** + **MPU6050** (IMU) + **TCRT5000** (detektor mrugniec, sygnal analogowy) + opcjonalnie **Akyga Li-Pol 1900 mAh 1S 3,7 V** (tylko BAT+/BAT−).

---

## Jak to dziala

### Ruch kursora

- **Os X (lewo / prawo):** predkosc katowa **Gx** z MPU6050 (empirycznie: obrot glowy wokol osi pionowej).
- **Os Y (gora / dol):** predkosc katowa **Gz** z MPU6050 (empirycznie: pochylenie / ruch glowy w plaszczyznie pionowej).
- Surowe odczyty zyroskopu sa przeliczane na °/s przez `CZULOSC_ZYRO_LSB` (131 LSB/(°/s) przy zakresie ±250°/s),
  odejmowany jest offset biasu z kalibracji startowej, potem stosowany jest prog szumu i strefa martwa.
- Algorytm **rate-control** - delta HID liczona z chwilowej predkosci katowej, nie z pozycji.
  Dzieki temu system nie kumuluje bledu kalibracji (brak orientacji = brak dryfu pozycji).
- Strefa martwa + prog szumu odrzucaja mikrodrgania w spoczynku.
- Skala: 1 jednostka HID ≈ 1 piksel kursora, wzor `dX = predkosc_Gx[°/s] * CZULOSC_MYSZY (0.4)` (analogicznie Gz → dY).
  Kierunek obu osi odwracasz przez `ODWROC_OS_X` / `ODWROC_OS_Y`.

### Detektor mrugniec (TCRT5000 analogowy)

Czujnik refleksyjny IR czyta intensywnosc swiatla odbitego od oka. Pelny lancuch obrobki sygnalu:

1. **ADC 12-bit** (0-4095) z pinu A1 - surowy odczyt `tcrtRaw`.
2. **Filtr EMA** (α = 0.2) wygladza szum -> `tcrtFiltered`.
3. **Trimmed-mean kalibracja startowa** - 200 probek, odrzucenie 10% skrajnych z gory i dolu, srednia z 80% srodkowych -> `tcrtBaseline` (poziom "oko otwarte").
4. **Komparator z histereza** - dwa progi wzgledem baseline:
   - sygnal spada o `OFFSET_TRIGGER` (350 ADC) ponizej baseline → "oko zamkniete",
   - sygnal wraca powyzej `baseline - OFFSET_RELEASE` (120 ADC) → "oko otwarte",
   - sygnal spada glebiej niz `baseline - OFFSET_MAX_ZWARCIA` (800 ADC) → zdarzenie
     mechaniczne (zdjecie okularow, zaslon czujnika) — ignorowane, reset do "otwarte".
   - TRIGGER > RELEASE = histereza chroniaca przed migotaniem na granicy.
5. **Adaptacyjny baseline** (wolny EMA, α = `EMA_ALPHA_WOLNY` = 0.005) - ciagla, powolna autokorekta
   poziomu odniesienia gdy oko jest otwarte (stala czasowa ~2 s). Zmiany oswietlenia lub pozycji
   glowy sa kompensowane w ~3-5 s, bez zadnych timeoutow. Podczas mrugniec baseline jest zamrozony.
6. Wirtualna flaga `wirtualnyStanCzujnika` zasila istniejaca **maszyne stanow** wieloklikow,
   debounce'u i drag&drop - dokladnie tak jak dawniej cyfrowy LM393.

### Mapowanie mrugniec na akcje

Maszyna stanow zlicza mrugniecia w **serii** (przerwa miedzy mrugnieciami < `OKNO_WIELOKLIKU_MS`):

| Mrugniec | Akcja |
|----------|---------------------------------------------------------|
| 1        | **LPM** (lewy klik)                                     |
| 2        | **Double-click LPM** (otwieranie plikow / zaznaczanie)  |
| 3        | **PPM** (prawy klik)                                    |
| 4        | **Toggle trybu scrolla** (dioda swieci ciagle gdy ON)   |
| 5        | **Rekalibracja zyroskopu** (offset Gx/Gy/Gz, LED mrugnie 3×; bez ponownego baseline TCRT) |
| 6        | **Toggle trybu debug Serial** (LED mrugnie 2x)          |
| Trzymanie oka > `PROG_PRZYTRZYMANIA_MS` | **Drag & drop** (LPM trzymany az do otwarcia oka) |

Impulsy krotsze niz `CZAS_MIN_MRUG_MS` (80 ms) sa odrzucane jako artefakty ruchu glowy —
nie trafiaja do licznika serii ani do dragu.

Akcje 4-6 dzialaja zawsze; klikniecia 1-3 sa zablokowane w trybie scrolla zeby przypadkowe
mrugniecie nie generowalo klikow podczas przewijania. Drag & drop tez jest wylaczony w trybie scrolla.

### Tryb scrolla

W trybie scrolla pochylanie glowy gora/dol steruje kolkiem myszy zamiast kursorem.
Ruch lewo/prawo jest ignorowany. Predkosc reguluje `DZIELNIK_SCROLLA`.

### Komunikacja

- **USB HID** - priorytet, niskie opoznienia, kabel USB-C.
- **Bluetooth Low Energy (BLE) HID** - tryb bezprzewodowy, automatyczne polaczenie po
  sparowaniu z systemem operacyjnym.
- Pelna kompatybilnosc z Windows / macOS / Linux / Android - sterownik HID jest natywny.

---

## Sprzet i piny (Seeed XIAO ESP32-S3 Plus)

| Pin na plytce | GPIO | Funkcja na ESP32-S3 | Podlaczenie                                  |
|---------------|------|---------------------|----------------------------------------------|
| `D1` / `A1`   | 2    | ADC1_CH1            | **AO** (wyjscie analogowe) modulu TCRT5000   |
| `D4`          | 5    | I2C SDA             | **SDA** czujnika MPU6050                     |
| `D5`          | 6    | I2C SCL             | **SCL** czujnika MPU6050                     |
| `D16` (back)  | 10   | ADC1_CH9 / ADC_BAT  | Wewnetrzny dzielnik **1:11** (R9=100K + R8=10K) do **BAT+** (monitor baterii) |
| `LED_BUILTIN` | 21   | -                   | Dioda wbudowana (status / klikniecia)        |
| `BAT+ / BAT-` | -    | -                   | **Akyga Li-Pol 1900 mAh 1S 3,7 V** — tylko piny **+** i **−** (patrz ponizej) |
| `USB-C`       | -    | USB Serial / HID    | Zasilanie + programowanie + komunikacja      |

**TCRT5000:** zasilanie 3.3 V, wykorzystywane wyjscie **AO** (analogowe). Wyjscie DO/D0
z komparatorem LM393 jest zostawione niepodlaczone - obrabiamy caly sygnal programowo.

**MPU6050:** zasilanie 3.3 V, I2C @ 400 kHz (Fast Mode). Adres 0x68.

**Bateria:** ogniwo **Akyga Li-Pol 1S 3,7 V nominalnie, 1900 mAh**,
wymiary ok. **50 × 34 × 9,5 mm**, wylot **JST 3-pin** na kabelu. W projekcie HEFAS uzywamy
**wylacznie dwoch pinow: + (BAT+) i − (BAT−)** podlaczonych do padów **BAT+ / BAT−** na spodzie
XIAO ESP32-S3 Plus. **Srodkowy pin JST (NTC, termistor BMS)** pozostaje **niepodlaczony**.

Zakres roboczy napiecia ogniwa: ok. **3,0–4,2 V** (nominalnie 3,7 V). Ladowanie przez USB-C (PMIC SGM40567, ok. 110 mA).

---

## Struktura projektu

```
HEFAS 4.0/
├── platformio.ini              PlatformIO: board seeed_xiao_esp32s3, USB HID + CDC,
│                               lib: ESP32-BLE-Mouse, MPU6050
├── include/
│   ├── hefas_config.h          Wszystkie stale (piny, czulosc, progi, parametry)
│   └── hefas_webdebug.h        Naglowek modulu diagnostyki WiFi (opcjonalny)
├── src/
│   ├── main.cpp                Pelna logika systemu (IMU, TCRT, USB / BLE, bateria)
│   └── hefas_webdebug.cpp      Modul WebDebug (WiFi AP + WebServer + panel HTML)
└── README.md                   Ten plik
```

**PlatformIO (`platformio.ini`):** `ARDUINO_USB_MODE=1` i `ARDUINO_USB_CDC_ON_BOOT=1` wlaczaja
natywny USB HID oraz port Serial przez USB-C. Monitor: 115200 baud.

---

## Uruchomienie

1. Podlacz MPU6050: SDA -> D4, SCL -> D5, VCC -> 3V3, GND -> GND.
2. Podlacz TCRT5000: AO -> D1/A1, VCC -> 3V3, GND -> GND. (DO i regulator progu nieuzywane.)
3. (Opcjonalnie) Podlacz ogniwo **Akyga 1900 mAh 1S 3,7 V** do padów **BAT+** i **BAT−**
   na spodzie XIAO — **tylko plus i minus**, srodkowy pin **NTC** z wtyczki JST **pominij**
   (nie lutuj, nie wciskaj do zlacza). **Uwaga na polaryzacje!**
4. Podlacz XIAO ESP32-S3 Plus przez USB-C do komputera.
5. Otworz projekt w PlatformIO (VSCode).
6. Zbuduj i wgraj: `pio run -t upload` lub **Ctrl+Alt+U**.
7. Po starcie poloz plytke nieruchomo i trzymaj otwarte oko na czujniku TCRT.
   Kalibracja przebiega w **dwoch fazach** (LED swieci/miga inaczej):
   - **Faza 1 (~2 s, LED swieci CIAGLE):** rownolegly pomiar 200 probek zyroskopu
     i TCRT. Obliczane sa offsety biasu zyroskopu i wstepny baseline (trimmed-mean 80%).
   - **Faza 2 (~1 s, LED MIGA WOLNO):** filtr EMA przebiega ze stabilna dioda IR.
     Kompensuje dryf termiczny diody przez pierwsze 1-2 s po wlaczeniu (~30-60 ADC).
     Baseline koncowy = tcrtFiltered po stabilizacji — dokladniejszy niz "zimna" srednia.
8. Dioda mrugnie 3x = system gotowy do pracy (laczny czas kalibracji ~3 s).

System automatycznie wybiera tryb komunikacji:
- jest USB i host enumerowal urzadzenie -> **USB HID** (priorytet),
- inaczej -> **BLE HID** (paruj z systemem operacyjnym jako "HEFAS 4.0").

---

## Strojenie parametrow

Wszystkie parametry w `include/hefas_config.h`. Najwazniejsze do regulacji pod uzytkownika:

### Czulosc ruchu

| Parametr            | Domyslna | Opis                                                  |
|---------------------|----------|-------------------------------------------------------|
| `CZULOSC_MYSZY`     | 0.4      | Predkosc kursora. 0.2 = wolno (precyzyjnie), 1.0 = szybko |
| `CZULOSC_ZYRO_LSB`  | 131.0    | Przelicznik surowego Gx/Gz MPU6050 na °/s (±250°/s)   |
| `STREFA_MARTWA`     | 2.0 °/s  | Dead zone - zapobiega dryfowi przy nieruchomej glowie |
| `PROG_ZYROSKOPU`    | 1.5 °/s  | Prog szumu zyroskopu (pierwszy filtr)                 |
| `ODWROC_OS_X`       | 1        | Kierunek osi X z Gx (+1 lub -1, gdy montaz odwrotny)  |
| `ODWROC_OS_Y`       | 1        | Kierunek osi Y z Gz (+1 lub -1)                        |

### Detekcja mrugniec

| Parametr                  | Domyslna | Opis                                                |
|---------------------------|----------|-----------------------------------------------------|
| `OKNO_WIELOKLIKU_MS`      | 400 ms   | Max przerwa miedzy mrugnieciami w serii             |
| `PROG_PRZYTRZYMANIA_MS`   | 500 ms   | Po tym czasie zamkniete oko -> wchodzi w drag       |
| `CZAS_DEBOUNCE_MS`        | 30 ms    | Eliminacja drgan progu                              |
| `CZAS_MIN_MRUG_MS`        | 80 ms    | Min. czas zamknietego oka liczony jako mrugnięcie — ruchy glowy generuja krotkie (<80 ms) artefakty, ten prog je odrzuca. Prawdziwe mrugniecia trwaja 100-400 ms. |
| `CZAS_KROTKIEGO_KLIKU_MS` | 80 ms    | Czas press/release wysylanego do HID                |
| `CZAS_MIEDZY_KLIKAMI_PODWOJNEGO_MS` | 60 ms | Odstep release->press w double-click          |

**WAZNE:** `PROG_PRZYTRZYMANIA_MS` musi byc wyraznie wiekszy od `OKNO_WIELOKLIKU_MS`,
inaczej dlugie pojedyncze mrugniecie wpadnie w drag zamiast byc zliczone jako LPM. Zapas
zalecany min. 100 ms.

### Detektor analogowy TCRT5000

| Parametr             | Domyslna | Opis                                                   |
|----------------------|----------|--------------------------------------------------------|
| `PIN_TCRT_ANALOG`    | A1       | Pin ADC dla AO TCRT5000                                |
| `PROBKI_TCRT_KALIBRACJI` | 200  | Liczba probek startowych do wyznaczenia baseline       |
| `EMA_ALPHA`          | 0.20     | Szybki EMA: wygladzanie szumu ADC (wieksze = szybsza reakcja, wiecej szumu) |
| `EMA_ALPHA_WOLNY`    | 0.005    | Wolny EMA: adaptacyjny baseline (~2 s stala czasowa @100 Hz). Zastapil TRACKING_ALPHA + TIMEOUT_TRACKING_MS. |
| `OFFSET_TRIGGER`     | 350      | Spadek ADC ponizej baseline = wykryto zamkniecie oka   |
| `OFFSET_RELEASE`     | 120      | Powrot powyzej baseline-OFFSET_RELEASE = otwarcie oka  |
| `OFFSET_MAX_ZWARCIA` | 800      | Jesli sygnal spada glebiej niz to, ignorujemy (zdjecie okularow itp.) |

### Monitor baterii

| Parametr                   | Domyslna | Opis                                                |
|----------------------------|----------|-----------------------------------------------------|
| `PIN_ADC_BATERIA`          | 10 (D16) | GPIO10 = ADC1_CH9 = ADC_BAT na XIAO Plus            |
| `DZIELNIK_BATERII`         | **11.0** | R9=100K + R8=10K → 1:11. Skalibruj wg multimetru jesli odchylka > 0.1 V. |
| `PROBKI_BATERII`           | **64**   | Usrednianie 64 probek analogReadMilliVolts()        |
| `OKRES_POMIARU_BATERII_MS` | **3000 ms** | Co ile pobierany jest pomiar                     |
| `V_BATERIA_BRAK_PROG`      | **2.5 V** | Ponizej tego → ogniwo niepodlaczone lub blad +/−   |
| `V_BATERIA_PELNA / PUSTA`  | 4,2 / 3,2 V | Granice dla ogniwa Akyga 1S 3,7 V (3,0–4,2 V roboczo) |

### Inne

| Parametr            | Domyslna | Opis                                              |
|---------------------|----------|---------------------------------------------------|
| `DZIELNIK_SCROLLA`  | 4        | Predkosc scrollowania (wieksze = wolniej)         |
| `CZAS_BLYSKU_LED_MS`| 60 ms    | Krotki blysk diody po kliknieciu (poza scroll/drag) |
| `OKRES_PETLI_MS`    | 10 ms    | Czestotliwosc petli glownej (100 Hz)              |
| `OKRES_DIAGNOSTYKI_MS` | 500 ms | Czestotliwosc wypisywania logow diagnostycznych  |
| `PROBKI_KALIBRACJI` | 200      | Liczba probek kalibracji zyroskopu (start + rekal.) |
| `WEBDEBUG_AKTYWNY`  | true     | WiFi AP + panel 192.168.4.1 (false = modul wylaczony) |
| `WEBDEBUG_SSID`     | HEFAS-Debug | Nazwa sieci diagnostycznej                     |
| `WEBDEBUG_HASLO`    | hefas1234 | Haslo AP WebDebug                                 |

---

## Monitor baterii - dzialanie i kalibracja

**Sprzet w projekcie:** Akyga Li-Pol **1900 mAh, 1S, 3,7 V**, JST 3-pin (50 × 34 × 9,5 mm).
Podlaczenie: **BAT+ → pad BAT+**, **BAT− → pad BAT−** na XIAO; pin **NTC nieuzywany**.

XIAO ESP32-S3 Plus ma na spodzie pin **D16** (sprzetowo GPIO10 = ADC1_CH9 = `ADC_BAT`)
fabrycznie podlaczony przez **wewnetrzny dzielnik napiecia R9=100K / R8=10K (1:11)**
do bieguna **BAT+** plytki (a stad do plusa ogniwa). Dzieki temu napiecie ~4,2 V zostaje
zmniejszone do ok. **0,33–0,38 V** (mierzone na GPIO10 / D16, ADC_11db).

### Lancuch pomiaru

Po starcie firmware ustawia **ADC_11db** na GPIO10 **po** `kalibracjaSystemu()` i **ponownie po** `webDebugInit()`
(start WiFi AP moze resetowac ustawienia ADC). Pin baterii (~330 mV) miesci sie w liniowym zakresie 11 dB.

1. `analogReadMilliVolts(10)` × **64** probek, **trimmed-mean** (odrzut 10% skrajnych) → `pin` **[mV na GPIO10]**.
   W logu rownolegle `raw=` (12-bit ADC) do diagnostyki.
2. `V_pin = pin / 1000.0` → napiecie na pinie po dzielniku [V].
3. `V_bat = V_pin × DZIELNIK_BATERII` (**11.0**) → napiecie ogniwa Akyga [V].
4. Mapowanie na procent przez **8-punktowa krzywa rozładowania Li-Pol 1S**:

   | Napięcie | Procent |
   |----------|---------|
   | 4,20 V   | 100%    |
   | 4,10 V   | 90%     |
   | 4,00 V   | 80%     |
   | 3,85 V   | 60%     |
   | 3,75 V   | 40%     |
   | 3,65 V   | 20%     |
   | 3,50 V   | 10%     |
   | 3,30 V   | 0%      |

   Między punktami interpolacja liniowa. **Plateau 3,65–4,20 V** ≈ 80% pojemności —
   procent długo trzyma się wysoko, potem szybko spada.

5. Pomiar działa **zawsze** — także przy podłączonym USB. Przy ładowaniu napięcie może
   być nieznacznie wyższe (~0,05 V); w logu dopisek `(USB)`.
6. Poniżej `V_BATERIA_BRAK_PROG` (**2,5 V**) → **BRAK** (ogniwo odłączone, zła polaryzacja
   lub plus podłączony przez pin NTC zamiast BAT+). Przy poprawnym ogniwie oczekuj **pin ≈ 300–380 mV**
   i **Vbat ≈ 3,3–4,2 V**.

### Wyswietlanie poziomu baterii na podlaczonych urzadzeniach

W **trybie BLE** stan baterii jest wystawiany jako **standardowy BLE Battery Service**
(UUID `0x180F`, charakterystyka Battery Level `0x2A19`). System operacyjny pokazuje
go w panelu Bluetooth obok nazwy urzadzenia `HEFAS 4.0`:

- **Windows 10/11** - Settings -> Bluetooth & devices -> wybrane u"Batterrzadzenie -> procent obok ikonki.
- **macOS** - Pasek menu Bluetooth -> rozwiniecie urzadzenia -> y Level".
- **iOS / iPadOS** - Ustawienia -> Bluetooth -> "i" obok urzadzenia.
- **Android** - Ustawienia -> Polaczone urzadzenia -> wybrane urzadzenie (status bar nie zawsze, sekcja Bluetooth tak).

Aktualizacja BAS odbywa sie **tylko gdy procent realnie sie zmieni** (nie co 2 s jak
sam pomiar), zeby nie zasmiecac BLE niepotrzebnymi notyfikacjami. Log w Serial:
`[BLE] Battery Service = 82%`.

W **trybie USB** host nie zobaczy poziomu baterii - standard USB HID Mouse nie ma
mechanizmu raportowania baterii. Apple Magic Mouse i niektore Logitechy pokazuja to
w panelu systemowym przez wlasne sterowniki, ale jest to poza standardem HID i wymaga
firmware z osobna klasa USB - co byloby duzo pracy dla bardzo niskiej wartosci (na
USB plytka i tak jest podpieta do zasilania kablem). Stan baterii w trybie USB
ogladasz w panelu **WebDebug** (`192.168.4.1`).

### Kalibracja dzielnika (jednorazowa)

Schemat XIAO Plus: R9=100K + R8=10K → podzial **1:11** (nominalnie). Tolerancja
rezystorow +/-1% daje odchylenie do ~0.1 V od rzeczywistego napiecia. Jak sprawdzic:

1. Wgraj firmware, podłącz ogniwo Akyga (+/− na BAT+/BAT−).
2. Połącz się z `HEFAS-Debug` WiFi, otwórz `192.168.4.1` (USB może być podpięty).
3. W logach WebDebug co ~3 s szukaj: `[BAT] raw=1234 pin=382mV Vbat=4.20V [95%]` lub `... BRAK`.
4. Zmierz multimetrem napięcie między padami **BAT+** i **BAT−** na XIAO.
5. Jeśli różnica > 0,05 V, popraw (użyj `pin` z logu w mV):
   ```
   DZIELNIK_BATERII = V_multimetr / (pin_mV / 1000.0)
   ```
   Przykład: multimetr 4,15 V, log `pin=370mV` → `DZIELNIK = 4.15 / 0.370 ≈ 11.22`
6. Wpisz nową wartość w `hefas_config.h` i wgraj ponownie.

### Zlacze JST 3-pin (Akyga) — co podlaczyc

Wtyczka na kablu ogniwa ma **3 piny** (typowo: **+ | NTC | −**). W HEFAS:

| Pin JST | Podlaczenie w projekcie |
|---------|-------------------------|
| **+**   | Pad **BAT+** na spodzie XIAO |
| **NTC** | **Nic** — zostaw odizolowany / nieuzywany |
| **−**   | Pad **BAT−** na spodzie XIAO |

**Nie** podlaczaj plusa przez pin NTC — termistor w linii BAT+ zaniża odczyt ADC
(symptom: WebDebug pokazuje ~1,5–2,2 V zamiast ~4,1 V przy naladowanym ogniwie).

---

## Diagnostyka przez WiFi (WebDebug)

ESP32 stawia wlasna siec WiFi i serwuje panel diagnostyczny w przegladarce.
**Nie wymaga internetu, routera ani dodatkowego sprzetu.**

### Jak wejsc na panel:

1. Wgraj firmware na plytke.
2. Na telefonie lub laptopie wejdz w **ustawienia WiFi**.
3. Polacz sie z siecia **`HEFAS-Debug`** (haslo: **`hefas1234`**).
4. Otworz przegladarke (Chrome, Safari, Firefox).
5. Wpisz adres: **`192.168.4.1`** i wcisnij Enter.

### Co jest na panelu:

**Naglowek - kropki statusu (na zywo, co 160 ms):**

| Kropka  | Znaczenie                                                  |
|---------|------------------------------------------------------------|
| USB     | mysz HID widoczna po USB-C; zgaszone = aktywne BLE         |
| Scroll  | wlaczony tryb scrolla                                      |
| Drag    | trwa przytrzymanie LPM (drag & drop)                       |
| Pauza   | mysz spauzowana przyciskiem ponizej                        |
| Debug   | aktywne logowanie na Serial (mocniej grzeje)               |
| Oko     | detektor TCRT widzi zamkniete oko (czerwona)               |
| BAT     | ogniwo Akyga — zielona > 40%, zolta 20–40%, czerwona < 20%; szara = BRAK (< 2,5 V) |

**Lewy panel - dane numeryczne:**

- Schematyczna mysz SVG (blyska LPM/PPM, kolko swieci w trybie scrolla)
- Joystick - chwilowy wektor ruchu glowy
- Liczniki **L** / **R** - ile LPM/PPM wykonano od startu
- **dX** / **dY** - aktualne delty HID (~piksele na 10 ms)
- **SERIA** - ile mrugniec w trwajacej serii (zolta), czas od ostatniego mrugniecia,
  okno timeoutu - widoczne na zywo zliczanie mrugniec
- **TCRT R / F / B** - surowy ADC / po filtrze / baseline
- **BAT** — napięcie ogniwa [V], procent, `pin` [mV] na GPIO10 (do kalibracji dzielnika)

**Wykres ruchu glowy (gorny):**

- Linia **czerwona** = predkosc kursora X (lewo/prawo)
- Linia **cyjan** = predkosc kursora Y (gora/dol)
- Skala automatyczna, pokazana w rogach
- Historia ostatnich 32 sekund

**Wykres sygnalu TCRT (dolny):**

- Linia **cyjan** = `tcrtFiltered` (sygnal po filtracji EMA)
- Pozioma **szara kreskowana** = baseline (oko otwarte)
- Pozioma **czerwona kreskowana** = prog TRIGGER (zamkniecie oka)
- Pozioma **zolta kreskowana** = prog RELEASE (otwarcie oka)
- **Czerwone tlo** = chwile w ktorych wykryto zamkniete oko
- Niesamowicie pomocne przy strojeniu progow TCRT - widzisz na zywo czy mrugniecia
  zanurkowuja wystarczajaco glęboko ponizej czerwonej linii.

**Logi na zywo** - tekstowe komunikaty z `Serial.println` (ostatnie 80 wpisow).

**Przyciski:**

- **PAUZA / WZNOW** - blokuje wysylanie ruchu i klikniec (logika dalej dziala, ale nic
  nie idzie do hosta - bezpieczny debug bez szalonego kursora).
- **REKALIBRACJA** - uruchamia `kalibracjaZyroskopu()` zdalnie (tylko offsety Gx/Gy/Gz, bez ponownego baseline TCRT).
- **WYCZYSC** - czysci historie logow w panelu.

**Legenda** - rozwijana sekcja "Legenda - jak czytac panel" z pelnym opisem kazdego
elementu, przelicznikami jednostek (1 dX ≈ 1 piksel, 1 ADC ≈ 0.8 mV itd.) oraz
wzorami matematycznymi calej obrobki sygnalu.

### Wylaczanie modulu WiFi

Jesli WebDebug nie jest potrzebny (np. produkcyjne wdrozenie, oszczednosc energii),
w `include/hefas_config.h` ustaw `WEBDEBUG_AKTYWNY` na `false` (domyslnie `true`):
```c
#define WEBDEBUG_AKTYWNY    false
```
Modul sie nie skompiluje - zero wplywu na reszte kodu, zero zuzycia RAM / WiFi /
prądu radia.

---

## Diagnostyka przez Serial Monitor

Domyslnie logowanie na Serial jest **wylaczone** - aktywujesz je **6 mrugnieciami**
(toggle `czyDebugWlaczony`). Powod: WiFi i intensywne `Serial.print` zauwazalnie nagrzewaja
chip ESP32-S3. Mozesz tez wlaczyc na stale w kodzie:
```cpp
bool czyDebugWlaczony = true;
```

Po wlaczeniu Serial Monitor (115200 baud) wypisuje (co `OKRES_DIAGNOSTYKI_MS`, gdy debug ON):

- `[KAL]` / `[KALIBRACJA]` - offsety zyroskopu i baseline TCRT po starcie / rekalibracji
- `[RUCH]` / `[SCROLL]` - dane ruchu (tylko gdy dX lub dY ≠ 0) + `[USB]` / `[BLE]`
- `[TCRT] Raw=X Filt=Y Base=Z [OTW]` lub `[ZAMKN]` - stan detektora analogowego
- `[TCRT] Artefakt odrzucony (Xms < min 80ms)` - zbyt krotki impuls (ruch glowy)
- `[BAT] raw=N pin=382mV Vbat=4.20V [95%]` lub `... BRAK` — co 3 s w WebDebug; `(USB)` gdy kabel podpiety
- `[BLE] Battery Service = Z%` - aktualizacja poziomu baterii w BLE (gdy sie zmieni)
- `[KLIK] LEWY` / `DOUBLE LEWY` / `PRAWY` - wykryte klikniecia
- `[DRAG] Przytrzymanie ON` / `OFF` - wcisniecie i puszczenie LPM (drag & drop)
- `[SCROLL] ON` / `OFF` - przelaczanie trybu scrolla (4 mrugniecia)
- `[REKALIBRACJA]` - start / gotowe (5 mrugniec lub przycisk WWW)
- `[DEBUG] WLACZONY` / `WYLACZONY` - sekwencja 6 mrugniec

Te same komunikaty trafiaja rownolegle do panelu WebDebug (bufor 80 wpisow).

---

## Architektura wewnetrzna (skrot)

```
setup():
  Serial / LED / I2C @ 400 kHz / MPU6050 init + testConnection
  kalibracjaSystemu()              <- faza 1: rownolegla zyro+TCRT 200×10 ms (~2 s, LED ON)
                                    faza 2: stabilizacja diody IR 100×10 ms (~1 s, LED miga)
  initOdczytBaterii()             <- ADC_11db na GPIO10; ponownie po webDebugInit (WiFi)
  USB HID + BLE HID begin
  webDebugInit()                    <- WiFi AP (WEBDEBUG_SSID) + WebServer :80
  mrugnijDioda(3)                   <- gotowosc (~3 s po starcie)

loop() (co OKRES_PETLI_MS = 10 ms, ~100 Hz):
  webDebugLoop()                    <- HTTP; opcjonalnie kalibracjaZyroskopu z /rekalibracja
  odczytajIMU()                     <- Gx/Gz -> predkosc -> dX, dY (rate-control)
  aktualizujDetektorTCRT()          <- ADC -> EMA -> histereza -> adaptacyjny baseline
  odczytajPoziomBaterii()           <- co 3 s; 64× analogReadMilliVolts, krzywa Li-Pol, BLE BAS
  [jesli !webPauzaMyszy]
    obsluzKlikniecia()              <- debounce, seria mrugniec, drag
    wyslijRuchMyszy(dX, dY)         <- USB (tud_mounted) priorytet, inaczej BLE
  odswiezLed()                      <- ciagly ON w scroll/drag, krotki blysk po kliku
  diagnostyka()                     <- co 500 ms na Serial + WebDebug (gdy debug ON)
```

Wszystko nieblokujace - `millis()` + flagi czasu zamiast `delay()`. Petla jest stabilnie
przy 100 Hz, kalibracja startowa to jedyne miejsce z `delay`.

---

## Autorzy

Bartlomiej Adamczyk, Sebastian Sobczyk
Mechatronika - Szczecin 2026
