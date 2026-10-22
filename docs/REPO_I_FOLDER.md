# Jedno repo, jeden folder — bez gałęzi `hefas-stm`

## Zasada

- **Cały firmware Nicla** → repo **`bartkadres-ai/Hefas-STM`**, gałąź **`main`**.
- **HEFAS 4.0 (ESP32)** → tylko repo **`bartkadres-ai/HEFAS-4.0`** (gałąź `main`).
- Nie pobieraj STM z gałęzi `hefas-stm` w HEFAS-4.0 — ta ścieżka była tymczasowa i jest wycofana.

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

## Jednorazowo: wgraj najnowszy kod na GitHub `Hefas-STM` (jeśli `main` jest stary)

Na swoim PC, po pobraniu aktualnego kodu z tymczasowej gałęzi (ostatni raz):

```powershell
cd "C:\Users\werte\OneDrive\Pulpit\Hefas STM"
git pull https://github.com/bartkadres-ai/HEFAS-4.0.git hefas-stm
git push origin main
```

Potem tylko: `git pull origin main`.

## Codzienna praca

```powershell
cd "C:\Users\werte\OneDrive\Pulpit\Hefas STM"
git pull origin main
pio run -e nicla_vision -t upload
```

W Cursor / VS Code otwieraj **ten folder** jako workspace.
