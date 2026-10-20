# Osobne repozytorium `bartkadres-ai/HEFAS-STM`

Firmware STM jest na tej gałęzi (`hefas-stm`) w repo HEFAS-4.0 — **osobna historia**, tylko Nicla Vision:

https://github.com/bartkadres-ai/HEFAS-4.0/tree/hefas-stm

Integracja Cloud Agent **nie ma uprawnienia** `createRepository`, więc nowe repo zakłada **właściciel organizacji**.

## Szybko (gh CLI)

```bash
git clone --branch hefas-stm --single-branch https://github.com/bartkadres-ai/HEFAS-4.0.git HEFAS-STM
cd HEFAS-STM
gh repo create bartkadres-ai/HEFAS-STM --public --description "HEFAS na Arduino Nicla Vision" --source=. --remote=origin --push
```

## Albo puste repo w przeglądarce

1. GitHub → New → `HEFAS-STM` (public, bez README).
2. W sklonowanym katalogu z powyższego:

```bash
git remote set-url origin https://github.com/bartkadres-ai/HEFAS-STM.git
git push -u origin HEAD:main
```
