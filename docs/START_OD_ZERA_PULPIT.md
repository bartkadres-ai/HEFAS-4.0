# Start od zera: nowe repo + folder na Pulpicie + Cursor

Jedna ścieżka. Bez gałęzi `hefas-stm` w HEFAS-4.0 i bez mieszania dwóch repozytoriów.

## Co na końcu masz mieć

| Element | Wartość |
|--------|---------|
| GitHub | **jedno** repo (np. `bartkadres-ai/Hefas-STM` albo nowe `Hefas-Nicla`) |
| Gałąź | tylko **`main`** |
| Folder | `C:\Users\werte\OneDrive\Pulpit\Hefas STM` |
| `git remote -v` | **tylko** URL tego repo (nie HEFAS-4.0) |
| Cursor | **Otwórz folder** → Cloud Agent uruchamiaj **z tego samego repo** na GitHubie |

---

## Krok 1 — Nowe repo na GitHubie (przeglądarka)

1. https://github.com/new (konto `bartkadres-ai` lub swoje).
2. **Repository name:** np. `Hefas-Nicla` (albo wyczyść i używaj ponownie `Hefas-STM` — ważne: jedno repo na STM).
3. **Public** lub Private — jak wolisz.
4. **Nie** zaznaczaj „Add a README” (pusty start jest prostszy).
5. **Create repository**.

Zapisz URL, np. `https://github.com/bartkadres-ai/Hefas-Nicla.git`.

### Cursor musi móc pisać do tego repo

1. GitHub → **Settings** (konta) → **Applications** → **Cursor** (lub GitHub App Cursor).
2. **Repository access** → wybierz **Only select repositories** i zaznacz **to nowe repo** (albo **All repositories**).
3. W Cursor (desktop): **Settings → GitHub** — to samo konto, odśwież dostęp.

Bez tego agent w chmurze dostanie **403** przy `git push`.

---

## Krok 2 — Folder na Pulpicie (PowerShell)

Zrób backup starego folderu, potem świeży klon:

```powershell
cd "C:\Users\werte\OneDrive\Pulpit"

# backup (opcjonalnie)
if (Test-Path "Hefas STM") { Rename-Item "Hefas STM" "Hefas STM stary $(Get-Date -Format yyyy-MM-dd)" }

git clone https://github.com/bartkadres-ai/Hefas-Nicla.git "Hefas STM"
cd "Hefas STM"
git branch -M main
git remote -v
```

`origin` musi wskazywać **tylko** na nowe repo.

---

## Krok 3 — Pierwszy commit z kodem firmware (jednorazowo)

Repo jest puste — musisz wgrać pliki projektu PlatformIO **raz** ze swojego PC (masz tam push).

### Wariant A — masz folder „Hefas STM stary” z kodem

```powershell
cd "C:\Users\werte\OneDrive\Pulpit\Hefas STM"

# skopiuj z backupu wszystko OPRÓCZ .git i .pio
$src = "C:\Users\werte\OneDrive\Pulpit\Hefas STM stary 2026-10-22"   # dopasuj nazwę backupu
robocopy $src . /E /XD .git .pio .vscode /XF *.elf *.bin

git add .
git status
git commit -m "Initial HEFAS Nicla Vision firmware"
git push -u origin main
```

### Wariant B — backup jest tylko na GitHubie w starym Hefas-STM

```powershell
cd "C:\Users\werte\OneDrive\Pulpit"
git clone https://github.com/bartkadres-ai/Hefas-STM.git hefas-kod-tymczasowy
cd "Hefas STM"
robocopy "..\hefas-kod-tymczasowy" . /E /XD .git .pio
# upewnij się, że platformio.ini ma sekcję [platformio] — patrz Krok 4
git add .
git commit -m "Initial import from Hefas-STM"
git push -u origin main
```

Potem możesz usunąć folder `hefas-kod-tymczasowy`.

---

## Krok 4 — `platformio.ini` (obowiązkowe na Windows)

Na **samej górze** pliku (po komentarzu) musi być:

```ini
[platformio]
default_envs = nicla_vision
```

Bez `[platformio]` PlatformIO na Windows często zgłasza błąd INI.

Upload (jedno środowisko):

```powershell
pio run -e nicla_vision -t upload
```

Szczegóły DFU / COM8: [UPLOAD_NICLA_WINDOWS.md](UPLOAD_NICLA_WINDOWS.md).

---

## Krok 5 — Cursor: praca na co dzień

1. **File → Open Folder** → `C:\Users\werte\OneDrive\Pulpit\Hefas STM`.
2. Cloud Agent / Background Agent: w GitHubie otwórz **to samo repo** (`Hefas-Nicla` lub poprawione `Hefas-STM`) → **New Agent** — nie startuj agenta z **HEFAS-4.0**.
3. Na PC przed pracą:

```powershell
cd "C:\Users\werte\OneDrive\Pulpit\Hefas STM"
git pull origin main
```

Po zmianach agenta:

```powershell
git pull origin main
pio run -e nicla_vision
```

---

## Struktura projektu (co powinno być w repo)

```text
Hefas STM/
  platformio.ini
  src/
  include/
  docs/
  README.md
  .gitignore          # min. .pio/
```

**Nie commituj:** `.pio/`, `build/`, `*.elf`, `*.bin`.

---

## Szybki test, że wszystko jest OK

```powershell
cd "C:\Users\werte\OneDrive\Pulpit\Hefas STM"
git remote get-url origin
pio run -e nicla_vision
```

- `origin` = twoje **jedno** repo STM.
- Build kończy się **SUCCESS** (bez drugiego env naraz).

---

## Jeśli coś poszło nie tak

| Problem | Rozwiązanie |
|--------|-------------|
| `origin` wskazuje HEFAS-4.0 | `git remote set-url origin https://github.com/bartkadres-ai/TWOJE-REPO.git` |
| Agent nie pushuje (403) | Dodać repo w GitHub App Cursor + nowy agent z **tego** repo |
| Dwa FAILED przy Upload | Tylko `pio run -e nicla_vision -t upload`, `default_envs = nicla_vision` |
| PermissionError COM8 | Double-tap RESET, env bez 1200 baud — patrz upload doc |
