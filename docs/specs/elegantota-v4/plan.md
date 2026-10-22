# Plan : elegantota-v4

Migration OTA vers ElegantOTA 4.0.0 (mode async) et pile esp32async, sans vendoring. Spec : `SPEC.md`, critères : `features.json`.

## T1 : migration OTA et dépendances (F1, F2, F3)

Fichiers : `platformio.ini`, `MaxPV3/MaxPV3.ino`, suppression de `Librairies IDE/`.

- `MaxPV3.ino` : `#include <ElegantOTA.h>` ; `startWeb()` : suppression de `AsyncElegantOTA.setID(...)`, `ElegantOTA.begin(&webServer);` ; un seul `ElegantOTA.loop();` dans `void loop()`, pas de `setAutoReboot`.
- `platformio.ini` : retirer `lib_dir` et la ligne git AsyncElegantOTA ; ajouter en versions exactes `ayushsharma82/ElegantOTA@4.0.0`, `esp32async/ESPAsyncWebServer@X.Y.Z`, `esp32async/ESPAsyncTCP@X.Y.Z` ; `build_flags = -DELEGANTOTA_USE_ASYNC_WEBSERVER=1` ; conserver `lib_ignore = STM32AsyncTCP` (à compléter si une lib tierce tire une autre implémentation async). Aucun `@^` / `@~`.
- Supprimer `Librairies IDE/`. Aucune modif de `EcoPV3`, `amorceESP`, `MaxPV3/*.h`.
- Libs incompatibles : ordre a/b/c de l'exigence 3 de la SPEC ; chaque remplacement consigné dans `progress.md` (lib, raison, licence, version). Aucune fonctionnalité retirée.
- Sécurité : aucune nouvelle route MaxPV ; relever routes internes et ressources externes d'ElegantOTA pour la PR.

## T2 : crédits et licence (F4)

Fichiers : `MaxPV3/data/credits.html`, `README.md`.

- credits.html : lien ElegantOTA `https://github.com/ayushsharma82/ElegantOTA`, libellé `ElegantOTA (AGPL-3.0)` ; liens ESPAsyncWebServer / ESPAsyncTCP vers ESP32Async.
- README : section `## Licence` en fin de document, trois points (MaxPV! LGPL 2.1+ ; ElegantOTA AGPL-3.0 embarqué ; binaires distribués soumis à l'AGPL-3.0).

## T3 : CLAUDE.md (F5)

Fichier : `CLAUDE.md`.

- Ligne `platformio.ini` de la section Build sans `lib_dir` ni `Librairies IDE`, mentionnant esp32async et `-DELEGANTOTA_USE_ASYNC_WEBSERVER=1`.
- Contrainte du contrat : « ne sont modifiés que s'ils sont explicitement listés dans la SPEC en cours » ; `Librairies IDE` n'apparaît qu'une fois.

## Risques

- Build local impossible sur macOS arm64 (toolchain xtensa x86_64) : validation par la CI de la PR draft ; consigner dans `progress.md`.
- Disponibilité de `ElegantOTA@4.0.0` et des versions esp32async à confirmer via `pio pkg install`.
- Doublons async transitifs (WiFiManager, AsyncMQTT_Generic, AsyncHTTPRequest_Generic, AsyncPing), y compris sous un autre nom : vérifier le graphe à la main.
- Écart SPEC/features : la contrainte du contrat agentique est modifiée (Décision « Contrat »), à signaler dans la PR.
- Licence : binaires AGPL-3.0 ; tout remplacement de lib doit être compatible.
- Mémoire ESP8266 : mesures Flash/RAM avant/après.
- `/update` reste sans authentification (hors périmètre) ; signaler toute télémétrie du portail en tête de PR.
- Test matériel (OTA firmware puis filesystem, reboot, portail 192.168.4.1, MQTT, relais distant) à faire par l'utilisateur.
- `README` « Programmation du Wemos » inchangé (amorceESP).

## Plan de test

- `scripts/test_elegantota_v4.py` : critères F1, F2, F4, F5 de `features.json` (greps, `pio pkg install/list`), lancé via `.venv/bin/python -I scripts/test_elegantota_v4.py`.
- Contrat : test `check_versions.py`, lint `actionlint`, build (sinon consigné).
- CI de la PR draft : `gh pr checks --watch --fail-fast`, étape FTP en succès.
- Relever `RAM:` / `Flash:` sur main et la PR (F6) ; corps de PR : tailles, routes internes, ressources externes, remplacements de libs, checklist test matériel.
