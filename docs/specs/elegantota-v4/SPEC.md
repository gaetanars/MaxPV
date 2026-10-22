# SPEC : migration AsyncElegantOTA -> ElegantOTA 4.0.0 (elegantota-v4)

## Contexte
Le firmware ESP (`MaxPV3/MaxPV3.ino`) sert la page OTA `/update` avec `ayushsharma82/AsyncElegantOTA` 2.2.x, une bibliothèque dépréciée et archivée (ROADMAP P3-6). Usages : `MaxPV3/MaxPV3.ino:58` (`#include <AsyncElegantOTA.h>`), `:1932-1933` (`AsyncElegantOTA.setID(MAXPV_VERSION_FULL)` et `AsyncElegantOTA.begin(&webServer)`), `platformio.ini` (`lib_deps`), `MaxPV3/data/credits.html:78` (lien de crédit).
Le remplaçant est `ayushsharma82/ElegantOTA` 4.0.0 (sept. 2026), en mode serveur async. Il dépend de `esp32async/ESPAsyncWebServer ^3.12.0`, incompatible avec les copies vendorisées de `Librairies IDE/` (me-no-dev ESPAsyncWebServer 1.2.3, ESPAsyncTCP 1.2.2).
ElegantOTA 3.x/4.x est sous licence AGPL-3.0 (AsyncElegantOTA était MIT). MaxPV! est en LGPL 2.1+ : les binaires distribués deviennent de fait AGPL-3.0, ce qui est accepté.
Le build local est impossible sur macOS arm64 (toolchain xtensa x86_64). La compilation est validée par la CI de la PR.

## Objectif
Le firmware ESP compile et sert la page OTA `/update` via ElegantOTA 4.0.0 en mode async, sur la pile esp32async (ESPAsyncWebServer + ESPAsyncTCP) déclarée en versions exactes dans `lib_deps`. AsyncElegantOTA et `Librairies IDE/` disparaissent.

## Fichiers autorisés (dérogation au contrat)
- `MaxPV3/MaxPV3.ino` (seul sketch modifiable) ;
- `Librairies IDE/` (suppression intégrale) ;
- `platformio.ini`, `MaxPV3/data/credits.html`, `README.md`, `CLAUDE.md`.
Aucun autre `*.ino` ni `*.h` n'est modifié (`EcoPV3/`, `amorceESP/`, `MaxPV3/*.h` restent intacts).

## Hors périmètre
- Authentification sur `/update` (ROADMAP P1, sujet dédié) : `ElegantOTA.begin` est appelé sans identifiants.
- `amorceESP/` et `ci/amorce.ini` (mode synchrone, déjà sur ElegantOTA).
- Fonctions Pro d'ElegantOTA (Direct Download, Auto Update).
- Montée des numéros de version (`MAXPV_VERSION`, `versionweb.txt`, `latest`), tag et release : faits par l'utilisateur.
- Mise à jour de `docs/ROADMAP.md` et de `Documentation API/` (l'API HTTP de MaxPV ne change pas).
- Modification de `.github/workflows/`.

## Exigences
1. **OTA** : dans `MaxPV3/MaxPV3.ino`, `#include <AsyncElegantOTA.h>` est remplacé par `#include <ElegantOTA.h>`. Dans `startWeb()`, l'appel `AsyncElegantOTA.setID(...)` est supprimé (absent de l'API 4.0.0) et `AsyncElegantOTA.begin(&webServer)` devient `ElegantOTA.begin(&webServer)`. `ElegantOTA.loop()` est appelé à chaque itération de `loop()` (requis en mode async pour le redémarrage après la mise à jour). Le reboot automatique reste actif (valeur par défaut, pas d'appel `setAutoReboot`).
2. **Dépendances** (`platformio.ini`, env `d1_mini`) :
   - la ligne AsyncElegantOTA est supprimée ;
   - ajout de `ayushsharma82/ElegantOTA@4.0.0`, `esp32async/ESPAsyncWebServer@X.Y.Z` (dernière version publiée satisfaisant `^3.12.0`) et `esp32async/ESPAsyncTCP@X.Y.Z` (dernière version publiée compatible), en versions exactes, sans `^` ni `~` ;
   - `build_flags = -DELEGANTOTA_USE_ASYNC_WEBSERVER=1` ;
   - la ligne `lib_dir = Librairies IDE` est supprimée et le dossier `Librairies IDE/` aussi ;
   - le graphe de dépendances résolu contient une seule bibliothèque nommée `ESPAsyncWebServer` et une seule nommée `ESPAsyncTCP`, celles d'esp32async (via `lib_ignore` si une lib tierce tire une autre implémentation) ;
   - `lib_ignore = STM32AsyncTCP` est conservé.
3. **Compatibilité des libs existantes** : si `alanswx/ESPAsyncWiFiManager`, `khoih-prog/AsyncMQTT_Generic`, `khoih-prog/AsyncHTTPRequest_Generic` ou `akajes/AsyncPing(esp8266)` ne compile pas avec la pile esp32async, appliquer dans l'ordre :
   a. adaptation de `MaxPV3.ino` ou de `platformio.ini` (flags, `lib_ignore`, appels d'API) ;
   b. sinon, remplacement de la lib par un fork ou une alternative qui remplit toutes les conditions suivantes : publiée dans le registre PlatformIO, version exacte, licence compatible LGPL/GPL/AGPL (MIT, BSD, Apache-2.0, LGPL, GPL), commit ou release de moins de 18 mois, API équivalente qui ne demande que des changements dans `MaxPV3.ino`. Chaque remplacement est consigné dans `progress.md` et la PR (lib, raison, licence, version) ;
   c. si aucune option ne marche : arrêt, PR draft qui documente le blocage (erreur de compilation complète).
   Aucune fonctionnalité existante n'est retirée (portail WiFiManager, MQTT, relais distant, ping WatchDog Wifi, FTP optionnel).
4. **Variante FTP** : `PLATFORMIO_BUILD_FLAGS=-DMAXPV_FTP_SERVER pio run -e d1_mini` compile toujours (étape CI existante).
5. **Licence et crédits** :
   - `MaxPV3/data/credits.html` : le lien AsyncElegantOTA est remplacé par `https://github.com/ayushsharma82/ElegantOTA` avec le libellé `ElegantOTA (AGPL-3.0)`. Les liens ESPAsyncWebServer et ESPAsyncTCP, s'il y en a, pointent vers les dépôts esp32async ;
   - `README.md` : nouvelle section `## Licence` en fin de document, avec exactement ces trois points : le code MaxPV! est sous LGPL 2.1+ ; le firmware Wemos embarque ElegantOTA (AGPL-3.0) ; les binaires firmware distribués sont donc soumis à l'AGPL-3.0.
6. **Documentation projet** (`CLAUDE.md`) : la ligne « `platformio.ini` » de la section Build ne mentionne plus `lib_dir` ni `Librairies IDE`. Elle indique que la pile async (esp32async ESPAsyncWebServer/ESPAsyncTCP) et ElegantOTA (mode async via `-DELEGANTOTA_USE_ASYNC_WEBSERVER=1`) viennent de `lib_deps`. Plus aucune occurrence de `Librairies IDE` dans `CLAUDE.md`, hormis la contrainte du contrat agentique.
7. **Mesure** : les tailles flash et RAM du firmware avant (main) et après, lues dans le log CI (`RAM:` / `Flash:` de PlatformIO), sont reportées dans la PR.

## Exigences de sécurité
- Versions exactes depuis le registre PlatformIO pour toutes les nouvelles libs ; aucune dépendance git non épinglée.
- Aucune régression d'exposition : `/update` reste la seule route OTA, aucune nouvelle route n'est ajoutée par MaxPV (les routes internes d'ElegantOTA sont listées dans la PR).
- La PR liste les ressources externes chargées par le portail ElegantOTA 4.0.0 (CDN, télémétrie), relevées dans les sources de la lib. Si le portail contacte un service tiers (télémétrie), le signaler en tête de la PR.

## Décisions prises
- ElegantOTA 4.0.0 avec la pile esp32async (plutôt que 3.1.1 sur le serveur vendorisé).
- Licence AGPL-3.0 acceptée et mentionnée (credits.html, README).
- `Librairies IDE/` supprimé ; dépendances async via `lib_deps` en versions exactes.
- Lib incompatible : adaptation, sinon remplacement encadré (exigence 3), sinon arrêt documenté.
- `setID` abandonné : la version reste affichée dans l'UI MaxPV.
- Pas d'authentification OTA dans cette livraison.
- Contrat : `*.ino`/`*.h`/`Librairies IDE/` modifiables seulement s'ils sont listés par la SPEC en cours.
- Branche `agent/elegantota-v4` depuis `main`, après le merge de ci-hardening.

## Risques
- **OTA cassée sur matériel** : une régression de `/update` ou du reboot oblige à reflasher en USB. Le test matériel est fait par l'utilisateur avant le merge : OTA firmware puis filesystem par `/update`, redémarrage, portail WiFiManager (192.168.4.1), publication MQTT, relais distant. Cette checklist figure dans la PR et n'est pas vérifiable par l'agent.
- La pile esp32async 3.x consomme plus de RAM et de flash que me-no-dev 1.2.x sur un ESP8266 déjà contraint (cf. reboots du relais distant) : mesurés (exigence 7), pas de seuil bloquant.
- Les libs khoih-prog et ESPAsyncWiFiManager sont archivées et peuvent ne pas compiler, d'où des remplacements possibles (exigence 3) au comportement runtime non vérifié.
- Le portail ElegantOTA 4 est plus lourd en flash (PROGMEM) ; un dépassement fait échouer le build PlatformIO.
- L'installation initiale via l'IDE Arduino (README) n'a plus de dossier de libs fourni : seul l'amorçage `amorceESP` + OTA des binaires de release est supporté.

## Taille retenue
L : remplacement de la pile serveur web async (dépendance transverse à WiFiManager, MQTT, HTTP), suppression du vendoring, zone sensible (capacité de mise à jour de l'appareil).
