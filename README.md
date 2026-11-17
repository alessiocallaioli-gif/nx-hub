# NX Hub

All-in-one homebrew manager for Nintendo Switch. It keeps apps, overlays and sysmodules up to date
from a community JSON catalog, and lets you add your own repositories.

## Use
Copy `nx-hub.nro` to `/switch/` and launch it from hbmenu (Wi-Fi required).
**A** install/update · **Y** update all · **X** reload catalog · **+** exit.
Back up your SD card before the first run: NX Hub writes into `/switch/` and `/atmosphere/`.

## Catalog
- `registry/packages.json` is the source list (edit this one, via pull request).
- A GitHub Action (`update-registry`) resolves latest versions hourly into `registry/resolved.json`,
  which is what the app downloads (one request, no GitHub rate-limit issues).
- Personal packages: put a `custom.json` in `/config/nx-hub/` on the SD, same format.

```json
{ "id": "my-app", "name": "My App", "category": "app",
  "repo": "owner/repo", "asset_pattern": "*.nro",
  "install": "file", "target": "/switch/MyApp/MyApp.nro",
  "detect": "/switch/MyApp/MyApp.nro" }
```
`install`: `file` saves the release asset to `target`; `zip` extracts the archive under `target`.
`detect` is used to recognise packages already installed by hand.

## Not included on purpose
Custom firmware (Atmosphere, Hekate) and anything piracy-oriented. You can still add them yourself in `custom.json`.

## Build
Pushing to `main` builds `nx-hub.nro` with GitHub Actions (artifact `nx-hub`).
Locally: devkitPro + `switch-curl switch-sdl2 switch-sdl2_ttf switch-zlib`, plus `json.hpp` and `miniz` in `source/`.

## Debug
Launch with `nxlink -s nx-hub.nro` to see logs on your PC.

License: GPL-3.0.
