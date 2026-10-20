# Publikacja repozytorium na GitHub

Katalog `HEFAS-STM` jest **osobnym** projektem git (nie jest częścią HEFAS-4.0).

```bash
cd HEFAS-STM
gh repo create bartkadres-ai/HEFAS-STM --public --source=. --remote=origin --push
```

Jeśli repozytorium już istnieje:

```bash
git remote add origin https://github.com/bartkadres-ai/HEFAS-STM.git
git push -u origin main
```
