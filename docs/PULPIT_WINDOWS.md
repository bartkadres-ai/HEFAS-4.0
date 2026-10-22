# Pulpit „Hefas STM” — kod z HEFAS-4.0 (gałąź `hefas-stm`)

Agent Cursor **zapisuje** firmware Nicla tutaj:

**https://github.com/bartkadres-ai/HEFAS-4.0** → gałąź **`hefas-stm`**

(`platformio.ini` ma sekcję `[platformio]` — build na Windows działa.)

---

## Opcja A — masz folder podpięty pod `Hefas-STM` (zostaw `origin`)

```powershell
cd "C:\Users\werte\OneDrive\Pulpit\Hefas STM"
git pull https://github.com/bartkadres-ai/HEFAS-4.0.git hefas-stm
pio run -e nicla_vision
git push origin main
```

Ostatnia linia opcjonalnie — wrzuca ten sam kod na GitHub `Hefas-STM`, jeśli chcesz tam mieć kopię.

---

## Opcja B — tylko HEFAS-4.0 (najprościej dla agenta)

```powershell
cd "C:\Users\werte\OneDrive\Pulpit"
Rename-Item "Hefas STM" "Hefas STM backup" -ErrorAction SilentlyContinue
git clone -b hefas-stm https://github.com/bartkadres-ai/HEFAS-4.0.git "Hefas STM"
cd "Hefas STM"
pio run -e nicla_vision
```

Aktualizacja:

```powershell
cd "C:\Users\werte\OneDrive\Pulpit\Hefas STM"
git pull origin hefas-stm
```

---

## Upload

```powershell
pio run -e nicla_vision -t upload
```

Double-tap **RESET**, zamknięty Serial Monitor. Szczegóły: [UPLOAD_NICLA_WINDOWS.md](UPLOAD_NICLA_WINDOWS.md).
