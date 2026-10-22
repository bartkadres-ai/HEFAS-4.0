# Wgrywanie firmware — Nicla Vision (Windows + PlatformIO)

## Szybka procedura (zalecana na Windows)

1. Zamknij **Serial Monitor** i wszystko, co trzyma **COM** (Arduino IDE, PuTTY).
2. **Double-tap RESET** (wejście w DFU / bootloader).
3. W VS Code / Cursor: **Upload** (środowisko domyślne `nicla_vision` — **bez** otwierania COM8).

W `platformio.ini` wyłączone jest automatyczne **1200 baud na COM** — inaczej często dostajesz błąd na **COM8** (patrz niżej).

---

## Objaw: COM8 / PermissionError (31)

```text
Auto-detected: COM8
Cannot configure port ... PermissionError(13, 'Urządzenie dołączone do komputera nie działa.', None, 31)
```

**Co to znaczy:** PlatformIO próbowało użyć portu **COM8** (reset do bootloadera przez 1200 baud), a port **nie działa** — bo np. płytka jest już w **DFU** (wtedy CDC/COM często „nie działa”), port jest **zajęty**, albo sterownik szeregowy jest uszkodzony.

**Co zrobić:**

1. Zamknij monitor szeregowy.
2. **Double-tap RESET** → od razu **Upload** (bez wchodzenia w monitor).
3. Upewnij się, że masz aktualny `platformio.ini` z gałęzi `hefas-stm` (`board_upload.use_1200bps_touch = no`).
4. Opcjonalnie: jedno **RESET** (tryb aplikacji), sprawdź w Menedżerze urządzeń czy **COM8** jest bez błędu — wtedy możesz spróbować `pio run -e nicla_vision_auto -t upload` (auto-reset przez COM).

---

## Objaw: LIBUSB / DFU Error 74

```text
Cannot open DFU device 2341:035f found on devnum X (LIBUSB_ERROR_NOT_FOUND)
No DFU capable USB device available
*** [upload] Error 74
```

**Co to znaczy:** `dfu-util` **widzi** płytkę w trybie bootloadera (USB **VID:PID = 2341:035f**), ale **nie może jej otworzyć** — na Windowsie prawie zawsze to **zły / brakujący sterownik WinUSB (libusb)** dla tego interfejsu, albo port zajęty przez inny program.

---

## Krok 1 — Tryb DFU (bootloader)

1. Podłącz Nicla **bezpośrednio** do USB (najlepiej port **2.0**, z tyłu PC, bez huba).
2. **Szybko dwukrotnie** naciśnij przycisk **RESET** (dwa tapnięcia w ~0,5 s).
3. Płytka powinna być w DFU — w Menedżerze urządzeń pojawia się coś w stylu **Nicla Vision** / **2341:035F**.

Jeśli wgrywasz z normalnego trybu (bez double-reset), PlatformIO może próbować wejść w bootloader przez **1200 baud touch** — wtedy w `platformio.ini` musi być wolny port COM (zamknij Serial Monitor, Arduino IDE, inne programy).

---

## Krok 2 — Sterownik WinUSB (najważniejsze)

PlatformIO używa `dfu-util` + **libusb**. Na Windows potrzebujesz sterownika **WinUSB** dla **2341:035F** w trybie DFU.

### Opcja A — oficjalny instalator Arduino (mbed)

W folderze pakietu PlatformIO (ścieżka u Ciebie może być taka):

`C:\Users\werte\.platformio\packages\framework-arduino-mbed\drivers\`

1. **Płytka w trybie DFU** (double-reset).
2. Uruchom **jako administrator:** `dpinst-amd64.exe` (64-bit Windows).
3. Zaakceptuj instalację sterownika **Nicla Vision** / WinUSB.
4. Odłącz i podłącz USB; ponów double-reset i `pio run -t upload`.

### Opcja B — Zadig (gdy dpinst nie pomaga)

1. Pobierz [Zadig](https://zadig.akeo.ie/).
2. **Double-reset** na Nicli.
3. Zadig → **Options → List All Devices**.
4. Wybierz urządzenie z **USB ID 2341 035F** (czasem „Nicla Vision (Interface 0)”).
5. Sterownik docelowy: **WinUSB** → **Replace Driver** / **Install**.
6. Ponów upload.

**Uwaga:** Po WinUSB dla DFU, zwykły port szeregowy w trybie „aplikacji” może wymagać ponownego podłączenia lub innego wpisu w Menedżerze urządzeń — to normalne przy composite USB.

---

## Krok 3 — Upload w PlatformIO

W katalogu projektu:

```bash
pio run -e nicla_vision -t upload
```

W `platformio.ini` powinno być (już dodane w repo):

```ini
upload_protocol = dfu
```

Opcjonalnie, gdy masz kilka urządzeń USB:

```bash
pio device list
pio run -e nicla_vision -t upload --upload-port COM7
```

(`COM7` zamień na port z listy — w DFU czasem nie ma COM, wtedy upload i tak idzie po DFU 2341:035f).

---

## Krok 4 — Jeśli nadal Error 74

| Działanie | Po co |
|-----------|--------|
| Zamknij **Serial Monitor**, Arduino IDE, Edge/Chrome z WebUSB | Zwolnienie urządzenia |
| Terminal / VS Code **jako administrator** (jednorazowy test) | Uprawnienia do sterownika |
| Inny kabel USB (data, nie tylko ładowanie) | Częsty problem |
| Inny port USB | Stabilność DFU |
| W Menedżerze urządzeń: odinstaluj **2341:035F**, odłącz kabel, double-reset, dpinst/Zadig ponownie | Zepsuty binding sterownika |
| Wyłącz tymczasowo „Selective suspend” dla USB (opcje zasilania) | Rzadkie zawieszenia DFU |

---

## Alternatywa — ST-Link / CMSIS-DAP

Jeśli masz zewnętrzny debugger podłączony do SWD, w `platformio.ini` można użyć np. `upload_protocol = cmsis-dap` lub `stlink` (wymaga sprzętu i okablowania). Domyślnie Nicla Vision wgrywa się **przez USB DFU**, bez ST-Link.

---

## Po udanym uploadzie

- **Monitor:** `pio device monitor -b 115200`
- Pierwszy start po flashu: ok. **2 s** stabilizacji w firmware, potem kalibracja IMU+ToF.

Jeśli po naprawie sterownika upload przejdzie, a monitor jest pusty — sprawdź, czy wybrałeś właściwy port COM **po** wyjściu z DFU (jedno naciśnięcie RESET).
