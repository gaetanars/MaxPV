# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Projet

MaxPV! : interface Web + API + MQTT pour le routeur solaire EcoPV (surplus PV vers chauffe-eau via SSR). Firmware embarqué, doc et UI en français. Pas de suite de tests ni de linter : la validation = compilation + test sur matériel.

Deux cibles matérielles, deux firmwares distincts qui dialoguent par liaison série :

- **`EcoPV3/EcoPV3.ino`** : Arduino Nano. Algorithme de routage (mesures tension/courant, pilotage SSR/triac, relais secondaire, BOOST, RelayPlus, sonde 1-Wire ECS), config en EEPROM. Flashé via l'IDE Arduino ; compilé en CI avec PlatformIO (`pio ci`, `ci/ecopv.ini`).
- **`MaxPV3/MaxPV3.ino`** : Wemos D1 mini (ESP8266). Serveur web asynchrone, API HTTP, MQTT (+ autodiscovery Home Assistant), OTA, NTP, WatchDog Wifi, relais distant en miroir, FTP optionnel. Les pages web sont dans `MaxPV3/data/` (LittleFS).

## Build (ESP8266)

```
pio run -e d1_mini                      # firmware -> .pio/build/d1_mini/MaxPV<ver>_firmware.bin
pio run --target buildfs -e d1_mini    # image LittleFS depuis MaxPV3/data -> MaxPV<ver>_filesystem.bin
pio run -e d1_mini -t upload / uploadfs
```

- `pio test -e native` : tests natifs sur l'hôte (Unity, ArduinoJson), dans `test/`.
- `platformio.ini` : `src_dir=MaxPV3`, `data_dir=MaxPV3/data` ; toutes les libs viennent de `lib_deps`, y compris la pile async esp32async (ESPAsyncWebServer / ESPAsyncTCP) et ElegantOTA (mode async via `-DELEGANTOTA_USE_ASYNC_WEBSERVER=1`).
- `extra_script.py` extrait `MAXPV_VERSION` de `MaxPV3/maxpv_defines.h` par regex pour nommer les binaires : garder le format `#define MAXPV_VERSION "x.yy"`.
- CI `.github/workflows/build.yml` (actions épinglées par SHA, PlatformIO et libs en versions exactes, Dependabot mensuel) : jobs `lint` (actionlint), `build` (ESP + fs, variante FTP, tests natifs `pio test -e native`, EcoPV `nano`/`nano_oled` via `ci/ecopv.ini`, image fusionnée `MaxPV<ver>_full.bin` via `esptool merge-bin` (firmware `0x0` + filesystem `0x300000`, taille contrôlée), `pio check` non bloquant) sur push `main`/PR/dispatch ; sur tag `v*`, job `release` (contrôle des versions, release publiée + `.bin`).
- Le firmware EcoPV se flashe via l'IDE Arduino (libs OneWire, DallasTemperature, SSD1306Ascii optionnelle pour l'écran oLed).

## Architecture

- **Protocole série Nano -> ESP** (500000 bauds, `maxpv_defines.h`) : l'Arduino émet des chaînes CSV `STATS,...,END#` (`#` = `END_OF_TRANSMIT`). `serialProcess()` dans `MaxPV3.ino` les parse dans `ecoPVStats[]` (indices `INDEX_*` et `NB_STATS` dans `maxpv_defines.h`). L'ordre des champs côté `EcoPV3.ino` (~l.764) et les constantes d'index côté ESP doivent rester synchronisés. L'ESP envoie des commandes à l'Arduino (`setParamEcoPV`, `SSRModeEcoPV`, `boostModeEcoPV`, `relayPlusEcoPV`, …).
- **MaxPV3.ino** : `setup()`/`loop()` + planificateur `TickerScheduler` (`setTsTasks`, `eachSecondTasks`, `timeScheduler` pour BOOST/RelayPlus programmés). Config ESP en JSON LittleFS (`configWrite`, taille `JSON_CONFIG_SIZE`). Routes HTTP/API dans `setWebHandlers()` (`/api/get`, `/api/set`, `/api/action`, `/api/history`) ; doc API : `Documentation API/API_MaxPV.pdf` (à tenir à jour si l'API change).
- `maxpv_defines.h` : constantes (version, ports, buffers série, défauts réseau). `maxpv_options.h` : options de compilation (`MAXPV_FTP_SERVER` à dé-commenter, flags de libs à ne pas toucher).
- **Frontend** `MaxPV3/data/*.html` + `maxpv.css` : pages statiques qui appellent l'API ; embarqué dans l'image LittleFS (un changement UI nécessite `buildfs`/`uploadfs` ou OTA filesystem).

## Versions : trois numéros indépendants

MaxPV! (`MAXPV_VERSION` / `MAXPV_VERSION_FULL` dans `maxpv_defines.h`), site Web (`MaxPV3/data/versionweb.txt`), EcoPV (`#define VERSION` dans `EcoPV3.ino:87`). Une release met à jour :
- le fichier `latest` à la racine (JSON `ecopvver`/`maxpvver`/`webver`/`whatsnew`), lu par `main.html` via raw.githubusercontent.com (dépôt `gaetanars/MaxPV`, branche `main`) pour signaler une mise à jour ; `whatsnew` est affiché en texte brut ;
- la section « Versions » du `README.md` (entrée datée, avec les trois numéros) ;
- le tag `v<MAXPV_VERSION>` (ex. `v3.62`) : la CI vérifie la cohérence avec `scripts/check_versions.py`, puis publie la release avec les `.bin` versionnés et les assets à noms fixes `MaxPV3_firmware.bin` / `MaxPV3_filesystem.bin` (OTA) et `MaxPV3_full.bin` (installation initiale USB à `0x0`, utilisée par le README). Aucun binaire n'est commité.

## Points d'attention

- Contraintes mémoire ESP8266 : réponses HTTP volumineuses du relais distant => redémarrage (cf. README) ; préférer `F()` / `PROGMEM` et éviter les gros `String` temporaires.
- Ordre de mise à jour documenté : firmware puis filesystem ; la MAJ du filesystem efface la config Wifi.
- Origine : fork de Jetblack31/MaxPV (licence LGPL 2.1+ en en-têtes, attribution conservée dans credits.html) ; ce fork est autonome, l'UI ne référence plus l'amont hors crédits et aide EcoPV.

## Contrat agentique

- test : `.venv/bin/python -I scripts/check_versions.py && .venv/bin/python -I scripts/check_blueprints.py && .venv/bin/pio test -e native`
- lint : `actionlint`
- typecheck : none
- build : `.venv/bin/pio run -e d1_mini && .venv/bin/pio run -t buildfs -e d1_mini && .venv/bin/pio ci EcoPV3/EcoPV3.ino --project-conf ci/ecopv.ini -e nano`
- security : none
- branche de base : main
- contraintes :
  - aucun push de tag, aucun changement des réglages GitHub du dépôt ;
  - `Librairies IDE/` et les sources des sketches (`*.ino`, `*.h`) ne sont modifiés que s'ils sont explicitement listés dans la SPEC en cours ;
  - `.venv/` (PlatformIO 6.2.0 et PyYAML 6.0.2 épinglés, mêmes versions que la CI) et `CLAUDE.local.md` restent hors git (`.git/info/exclude`) ;
  - validation finale d'un changement de CI = runs GitHub Actions verts sur la PR.
