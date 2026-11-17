# Switch AIO Manager

Updater per homebrew/overlay Switch basato su un catalogo JSON (`registry/packages.json`).

## Preparazione
1. Installa devkitPro e: `dkp-pacman -S switch-dev switch-curl switch-sdl2 switch-sdl2_ttf switch-zlib`
2. Metti in `source/`: `json.hpp` (nlohmann/json, single header) e `miniz.c` + `miniz.h` (release amalgamata).
3. Copia il Makefile da `/opt/devkitpro/examples/switch/templates/application/Makefile` e imposta:
   - `TARGET := switch-aio-manager`
   - `CFLAGS += -DMINIZ_NO_TIME`
   - `LIBS := $(shell aarch64-none-elf-pkg-config --libs sdl2 SDL2_ttf libcurl) -lnx`
     (se il link fallisce, aggiungi le librerie mancanti segnalate dal linker)
4. In `source/main.cpp` sostituisci `TUO_UTENTE` in `REGISTRY_URL` con il tuo utente GitHub.
5. `make` -> ottieni `switch-aio-manager.nro` da copiare in `/switch/`.

## Catalogo
- Ufficiale: `packages.json` nel repo del registro (scaricato all'avvio, con cache su SD).
- Personale: `/config/switch-aio/custom.json` sulla SD, stesso formato.
- Campi: `install` = `zip` (estrae in `target`) oppure `file` (salva l'asset in `target`).
  Opzionali `latest` e `url` per evitare le chiamate all'API GitHub (si possono generare con una GitHub Action).

## Compilare senza installare nulla (GitHub Actions)
Carica il progetto in un repo GitHub: la Action in `.github/workflows/build.yml` scarica da sola
json.hpp e miniz, compila e pubblica `switch-aio-manager.nro` tra gli *Artifacts* della run (scheda Actions).
