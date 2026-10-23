# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Projet

MaxPV! : Home Assistant via MQTT (autodiscovery) + interface Web de configuration pour le routeur solaire EcoPV (surplus PV vers chauffe-eau via SSR). Firmware embarqué, doc et UI en français. Tests natifs (`pio test -e native`) pour la logique pure ; le reste se valide par compilation + test sur matériel.

Deux cibles matérielles, deux firmwares distincts qui dialoguent par liaison série :

- **`EcoPV/EcoPV.ino`** : Arduino Nano. Algorithme de routage (mesures tension/courant, pilotage SSR/triac, relais secondaire, BOOST, RelayPlus, sonde 1-Wire ECS), config en EEPROM. Flashé via l'IDE Arduino ; compilé en CI avec PlatformIO (`pio ci`, `ci/ecopv.ini`).
- **`MaxPV/MaxPV.ino`** : Wemos D1 mini (ESP8266). Serveur web asynchrone (backend HTTP interne), MQTT (+ autodiscovery Home Assistant), OTA, WatchDog Wifi et watchdog MQTT. L'UI (une page à 5 onglets) est dans `MaxPV/data/` (LittleFS). BOOST programmé, RelayPlus et relais miroir sont des blueprints HA (`blueprints/automation/maxpv/`, contrôlés par `scripts/check_blueprints.py`).

## Build (ESP8266)

```
pio run -e d1_mini                      # firmware -> .pio/build/d1_mini/MaxPV<ver>_firmware.bin
pio run --target buildfs -e d1_mini    # image LittleFS depuis MaxPV/data -> MaxPV<ver>_filesystem.bin
pio run -e d1_mini -t upload / uploadfs
```

- `pio test -e native` : tests natifs sur l'hôte (Unity, ArduinoJson), dans `test/`.
- `platformio.ini` : `src_dir=MaxPV`, `data_dir=MaxPV/data` ; toutes les libs viennent de `lib_deps`, y compris la pile async esp32async (ESPAsyncWebServer / ESPAsyncTCP) et ElegantOTA (mode async via `-DELEGANTOTA_USE_ASYNC_WEBSERVER=1`).
- `extra_script.py` extrait `MAXPV_VERSION` de `MaxPV/maxpv_defines.h` par regex pour nommer les binaires : garder le format `#define MAXPV_VERSION "x.y.z"`.
- CI `.github/workflows/build.yml` (actions épinglées par SHA, PlatformIO et libs en versions exactes, Dependabot mensuel) : jobs `lint` (actionlint, `check_blueprints.py` avec `pyyaml==6.0.3`), `build` (ESP + fs, tests natifs `pio test -e native`, EcoPV `nano`/`nano_oled` via `ci/ecopv.ini`, image fusionnée `MaxPV<ver>_full.bin` via `esptool merge-bin` (firmware `0x0` + filesystem `0x300000`, taille contrôlée), `pio check` non bloquant) sur push `main`/PR/dispatch ; sur tag `v*`, job `release` (contrôle des versions, release publiée + `.bin`).
- Le firmware EcoPV se flashe via l'IDE Arduino (libs OneWire, DallasTemperature, SSD1306Ascii optionnelle pour l'écran oLed).

## Architecture

- **Protocole série Nano -> ESP** (500000 bauds, `maxpv_defines.h`) : l'Arduino émet des chaînes CSV `STATS,...,END#` (`#` = `END_OF_TRANSMIT`). `serialProcess()` dans `MaxPV.ino` les parse dans `ecoPVStats[]` (indices `INDEX_*` et `NB_STATS` dans `maxpv_defines.h`). L'ordre des champs côté `EcoPV.ino` (~l.764) et les constantes d'index côté ESP doivent rester synchronisés. L'ESP envoie des commandes à l'Arduino (`setParamEcoPV`, `SSRModeEcoPV`, `relayModeEcoPV`, `boostModeEcoPV`, …).
- **MaxPV.ino** : `setup()`/`loop()` + planificateur `TickerScheduler` (`setTsTasks`, `eachSecondTasks`). Config ESP en JSON LittleFS (16 clés, `configWrite`, `configBounded`, taille `JSON_CONFIG_SIZE`). MQTT : base `maxpv/<id>` (`<id>` = `maxpv` + chip id), `/status` (LWT), `/state` et `/params` (JSON retain), commandes `maxpv/<id>/<clé>/set`.
- **`MaxPV/maxpv_mqtt.h`** : fonctions pures testées en natif (`test/test_mqtt`) : table PROGMEM des entités HA, discovery (`mqttDiscoveryTopic`/`mqttDiscoveryPayload`), `buildStateJson`, `buildParamsJson` et `handleCommand` (validation unique des commandes, appelée par `onMqttMessage` et `POST /cmd` ; `format` et `eraseconfigesp` HTTP seulement ; hors bornes = rejeté).
- **Backend HTTP interne** (`setWebHandlers()`, non documenté) : fichiers statiques, `GET /status`, `/params`, `/config` (sans `mqtt_pass`), `/backup`, `POST /config` et `POST /cmd` (en-tête `X-MaxPV` obligatoire, sinon 403 ; GET sur une route POST = 405 ; autre route = 404 `text/plain`), `/update` (ElegantOTA).
- `maxpv_defines.h` : constantes (version, buffers série, watchdogs, défauts réseau). `maxpv_options.h` : flags de libs à ne pas toucher.
- **Frontend** `MaxPV/data/` : `index.html` (5 sections `tab-*`), `app.js` (JS vanilla, fonction `post()` unique), `pico.min.css` (Pico 2.1.1, copie exacte, ne pas modifier), `maxpv.css`. Aucune ressource externe ; embarqué dans l'image LittleFS (un changement UI nécessite `buildfs`/`uploadfs` ou OTA filesystem).

## Versions : trois numéros indépendants

MaxPV! (`MAXPV_VERSION` / `MAXPV_VERSION_FULL` dans `maxpv_defines.h`), site Web (`MaxPV/data/versionweb.txt`), EcoPV (`#define VERSION` dans `EcoPV.ino:87`). Une release met à jour :
- le fichier `latest` à la racine (JSON `ecopvver`/`maxpvver`/`webver`/`whatsnew`), lu par `main.html` via raw.githubusercontent.com (dépôt `gaetanars/MaxPV`, branche `main`) pour signaler une mise à jour ; `whatsnew` est affiché en texte brut ;
- `CHANGELOG.md` (entrée datée, avec les trois numéros) ;
- le tag `v<MAXPV_VERSION>` (ex. `v4.0.0`) : la CI vérifie la cohérence avec `scripts/check_versions.py`, puis publie la release avec les `.bin` versionnés et les assets à noms fixes `MaxPV_firmware.bin` / `MaxPV_filesystem.bin` (OTA) et `MaxPV_full.bin` (installation initiale USB à `0x0`, utilisée par le README). Aucun binaire n'est commité.

## Points d'attention

- Contraintes mémoire ESP8266 : discovery de 59 entités publiée une par passage de `loop()` avec un buffer unique ; préférer `F()` / `PROGMEM` et éviter les gros `String` temporaires.
- Ordre de mise à jour documenté : firmware puis filesystem ; la MAJ du filesystem efface la config Wifi.
- Origine : fork de Jetblack31/MaxPV (licence LGPL 2.1+ en en-têtes, attribution conservée dans l'onglet Système de l'UI et le README) ; ce fork est autonome, l'UI ne référence plus l'amont hors crédits et aide EcoPV.

## Contrat agentique

- test : `.venv/bin/python -I scripts/check_versions.py && .venv/bin/python -I scripts/check_blueprints.py && .venv/bin/pio test -e native`
- lint : `actionlint`
- typecheck : none
- build : `.venv/bin/pio run -e d1_mini && .venv/bin/pio run -t buildfs -e d1_mini && .venv/bin/pio ci EcoPV/EcoPV.ino --project-conf ci/ecopv.ini -e nano`
- security : none
- branche de base : main
- contraintes :
  - aucun push de tag, aucun changement des réglages GitHub du dépôt ;
  - `Librairies IDE/` et les sources des sketches (`*.ino`, `*.h`) ne sont modifiés que s'ils sont explicitement listés dans la SPEC en cours ;
  - `.venv/` (PlatformIO 6.2.0 et PyYAML 6.0.3 épinglés, mêmes versions que la CI) et `CLAUDE.local.md` restent hors git (`.git/info/exclude`) ;
  - validation finale d'un changement de CI = runs GitHub Actions verts sur la PR.
