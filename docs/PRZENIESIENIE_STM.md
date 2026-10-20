# Przeniesienie HEFAS STM do Hefas-STM

Kod STM jest na gałęzi `hefas-stm` w tym repo (tylko Nicla, bez ESP32).

## Opcja A — Actions (zalecane)

1. Utwórz [fine-grained PAT](https://github.com/settings/tokens?type=beta) z **Contents: Read and write** na repo `bartkadres-ai/Hefas-STM`.
2. W **HEFAS-4.0 → Settings → Secrets → Actions** dodaj `HEFAS_STM_PUSH_TOKEN`.
3. **Actions → Publish to Hefas-STM → Run workflow**.

## Opcja B — lokalnie

```bash
git clone --branch hefas-stm --single-branch https://github.com/bartkadres-ai/HEFAS-4.0.git Hefas-STM
cd Hefas-STM
git remote set-url origin https://github.com/bartkadres-ai/Hefas-STM.git
git push -u origin HEAD:main
```

## Cursor Cloud Agent

Dodaj repo **Hefas-STM** w ustawieniach aplikacji GitHub **Cursor** (Repository access), żeby agent mógł pushować bez PAT.
