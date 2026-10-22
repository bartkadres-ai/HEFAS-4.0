# Jedno repo, jeden folder

**Pełna instrukcja od zera (nowe repo + Pulpit + Cursor):** [START_OD_ZERA_PULPIT.md](START_OD_ZERA_PULPIT.md)

## Zasada

- **Cały firmware Nicla** → **jedno** repo na GitHubie, gałąź **`main`**.
- Folder na PC: `C:\Users\werte\OneDrive\Pulpit\Hefas STM` — `origin` = to samo repo.
- **HEFAS 4.0 (ESP32)** to osobny projekt — nie mieszaj `git remote` z Nicla.

## Pierwszy raz (Pulpit)

```powershell
cd "C:\Users\werte\OneDrive\Pulpit"
git clone https://github.com/bartkadres-ai/Hefas-STM.git "Hefas STM"
cd "Hefas STM"
git remote -v
```

Powinno być:

```text
origin  https://github.com/bartkadres-ai/Hefas-STM.git (fetch)
origin  https://github.com/bartkadres-ai/Hefas-STM.git (push)
```

## Masz już folder „Hefas STM” z błędnym remote

```powershell
cd "C:\Users\werte\OneDrive\Pulpit\Hefas STM"
git remote -v
```

Jeśli `origin` wskazuje na **HEFAS-4.0** albo coś innego — popraw:

```powershell
git remote set-url origin https://github.com/bartkadres-ai/Hefas-STM.git
git fetch origin
git pull origin main
```

Jeśli `pull` ma konflikty — backup i świeży klon:

```powershell
cd "C:\Users\werte\OneDrive\Pulpit"
Rename-Item "Hefas STM" "Hefas STM stary"
git clone https://github.com/bartkadres-ai/Hefas-STM.git "Hefas STM"
```

## Codzienna praca

```powershell
cd "C:\Users\werte\OneDrive\Pulpit\Hefas STM"
git pull origin main
pio run -e nicla_vision -t upload
```

W Cursor / VS Code otwieraj **ten folder** jako workspace.
