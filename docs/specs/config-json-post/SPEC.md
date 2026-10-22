# SPEC : taille de la config JSON et transmission en POST (config-json-post)

## Contexte
ROADMAP P0-3, P0-4, P0-5.
- P0-3 : `JSON_CONFIG_SIZE` vaut 1100 (`MaxPV3/maxpv_defines.h:50`). `configRead()` (`MaxPV3/MaxPV3.ino:677`) désérialise un `const String&`, donc ArduinoJson copie clés et valeurs dans le pool. L'UI envoie tous les nombres sous forme de chaînes (`admin.html`). Avec les champs à longueur max (utilisateur et mot de passe MQTT 39, commandes ON/OFF du relais 127, 7 IP de 15), on estime environ 1,4 Ko sur ESP8266, au-delà de la limite (`NoMemory`). Conséquences :
  - au boot, `setup()` (`MaxPV3.ino:331-336`) efface `config.json` et repart en mode AP ;
  - `/api/set?configmaxpv` (`MaxPV3.ino:2183-2186`) répond « Request successfully processed » même quand `configRead()` échoue ;
  - `configWrite()` (`:736`) copie aussi les `String` et peut déborder sans le signaler.
- P0-4 : `saveConfigMaxPV()` (`admin.html:393-437`) met `JSON.stringify(doc)` brut dans l'URL GET. Un `&`, `#`, `+` ou `%` dans un champ casse ou altère la config. Le formulaire (`admin.html:62-65`) et la restauration (`admin.html:481-482`) lancent la sauvegarde et `rebootesp` en parallèle, et annoncent le succès sans attendre la réponse (P1-9).
- P0-5 : déjà livré par `1b1e1c0` (`urlVersion` et `latest` vers gaetanars/MaxPV, `CLAUDE.md`). GitHub Actions est actif sur le fork (`gh run list` : runs verts). Les liens d'aide EcoPV (`configuration.html:64`, `README.md:8`) et `credits.html` sont conservés volontairement (contenu documentaire et attribution).

## Objectif
Une config aux longueurs maximales de l'UI est lue, écrite et relue sans erreur. Elle est transmise en POST, encodée, quels que soient les caractères. Un échec de lecture est remonté (HTTP 400) et l'UI ne redémarre l'ESP qu'après une sauvegarde réussie.

## Hors périmètre
- P1-2 complet : les autres mutations (`/api/set?param|relaymode|ssrmode`, `/api/action`) restent en GET.
- Comportement au boot si `config.json` est illisible (effacement + mode AP) : inchangé (cf. P1-8).
- Bug `containsKey("mqtt_user")` pour `mqtt_pass` (P2-8), `/DLconfig`, masquage du mot de passe (P1-3).
- `Documentation API/API_MaxPV.pdf` (binaire, cf. P3-8), `EcoPV3/`, `Librairies IDE/`.
- Montée des versions (`MAXPV_VERSION`, `versionweb.txt`, `latest`, README « Versions »), tag et release : faits par l'utilisateur.
- Les liens EcoPV amont et `credits.html`.

## Exigences
Fichiers sources modifiables : `MaxPV3/maxpv_defines.h`, `MaxPV3/MaxPV3.ino`, `MaxPV3/data/admin.html`, `platformio.ini`, `test/test_config_size/test_main.cpp` (nouveau), `.github/workflows/build.yml`, `README.md`, `CLAUDE.md`. `docs/ROADMAP.md` est modifié localement : il n'est pas suivi par git, ne pas l'ajouter.

1. **Taille** : `#define JSON_CONFIG_SIZE    2048` dans `maxpv_defines.h`, sans changer le commentaire de fin de ligne. `DynamicJsonDocument` reste utilisé (tas, pas de pile). Si le test natif (exigence 6) échoue à 2048, passer à 3072 et le consigner dans `progress.md` et dans la PR.
2. **configWrite** : après le remplissage du document et avant `LittleFS.open`, si `jsonConfig.overflowed()` vaut true, appeler `logSerial(F("[ESP]"), F("Configuration trop volumineuse, non sauvegardée"))` puis `return` sans ouvrir ni écrire le fichier.
3. **API POST** : dans `setWebHandlers()`, ajouter `webServer.on("/api/set", HTTP_POST, ...)`. Le corps est en `application/x-www-form-urlencoded`, avec les champs `configmaxpv` (vide) et `value` (JSON de config), lus par `hasParam(F("..."), true)` / `getParam(F("value"), true)`.
   - `configRead(value)` vrai : `shouldSaveConfig = true`, réponse 200 `text/plain` « Request successfully processed ».
   - `configRead(value)` faux : réponse 400 `text/plain` « Bad request: invalid configuration ».
   - Champ `configmaxpv` ou `value` absent : réponse 400 « Bad request or request unknown ».
4. **GET retiré** : la branche `configmaxpv` du handler `HTTP_GET` de `/api/set` est supprimée (rupture assumée, cf. décision P1-2). `/api/get?configmaxpv` est inchangé.
5. **UI admin.html** :
   - `saveConfigMaxPV(onDone)` construit le même `doc`, puis :
     - `xhttp.open("POST", "api/set", true)` ;
     - en-tête `Content-Type: application/x-www-form-urlencoded` ;
     - `xhttp.send(new URLSearchParams({ configmaxpv: "", value: JSON.stringify(doc) }).toString())` ;
     - à `readyState == 4`, appel de `onDone(this.status == 200)` si `onDone` est fourni.
   - Formulaire `formIPConfig` : `onsubmit` appelle `saveConfigMaxPV(cb)`. Si `cb(true)`, l'alerte de succès existante s'affiche, puis `sendAction('rebootesp')`. Si `cb(false)`, une alerte `danger` « Échec de la sauvegarde : configuration refusée par MaxPV! » s'affiche dans `liveAlertPlaceholderIPConfig`, sans reboot.
   - Restauration : même chaînage dans `restoreConfigMaxPV()`. L'alerte « Configuration restaurée... » quitte le `onclick` du bouton et s'affiche dans le callback de succès (placeholder `liveAlertPlaceholderAdvancesAction`). L'échec affiche une alerte `danger`, sans reboot.
   - `sendAction(action, ele)` : `ele.blur()` seulement si `ele` est fourni.
   - Plus aucun appel `saveConfigMaxPV()` sans callback, plus aucune occurrence de `api/set?configmaxpv`.
6. **Test natif** :
   - `platformio.ini` : ajouter `[env:native]` avec `platform = native@1.2.1`, `test_framework = unity`, `lib_deps = bblanchon/ArduinoJson@6.21.6`, `build_flags = -I MaxPV3`. `[env:d1_mini]` reste inchangé.
   - `test/test_config_size/test_main.cpp` (Unity, `#include <ArduinoJson.h>`, `#include "maxpv_defines.h"`) :
     - `test_deserialize_max_config` : JSON des 29 clés de `configWrite()`, toutes les valeurs en chaînes comme l'envoie l'UI, à longueur max. IP : `"255.255.255.255"` pour ip, gateway, subnet, dns1, dns2, mqtt_ip, remoterelay_ip. `mqtt_user` et `mqtt_pass` : 39 car., dont `&#+%"\` dans `mqtt_pass`. `remoterelay_on`/`off` : 127 car. Nombres à leur max en chaîne (`"65535"`, `"3600"`, `"1380"`...), booléens `"1"`. Désérialisé depuis `std::string` dans `DynamicJsonDocument(JSON_CONFIG_SIZE)` : `DeserializationError::Ok`, et `mqtt_pass` relu à l'identique.
     - `test_write_roundtrip` : document `DynamicJsonDocument(JSON_CONFIG_SIZE)` rempli des 29 clés avec des valeurs `std::string` de mêmes longueurs (copies, comme `configWrite`) ; `overflowed()` faux ; `serializeJson` puis `deserializeJson` dans un nouveau document de même capacité : Ok.
     - Les deux tests affichent `memoryUsage()` via `TEST_MESSAGE`. La valeur est reportée dans `progress.md`.
   - CI `build.yml` : step `pio test -e native` dans le job `build`, après les compilations, avec le style des steps existants (`name:`, `run:`).
7. **README** : dans la section `## API`, ajouter une phrase : depuis cette version, la sauvegarde de config passe par `POST /api/set` (corps form-urlencoded, champs `configmaxpv` et `value`), et le GET `api/set?configmaxpv` est retiré ; le PDF n'est pas à jour sur ce point.
8. **ROADMAP (local)** : dans le tableau « Avancement », passer P0-3, P0-4, P0-5 à ✅ avec une ligne de synthèse (spec `config-json-post`, taille retenue, P1-9 réglé pour admin.html). Ajouter « — traité » aux titres P0-3, P0-4 et P0-5. Pour P0-5, noter que les liens EcoPV et `credits.html` sont conservés volontairement.
9. **Cohérence** : `.venv/bin/python -I scripts/check_versions.py` reste à 0.

## Exigences de sécurité
- Le mot de passe MQTT ne transite plus dans l'URL (ni historique, ni logs d'accès) : il passe dans le corps POST, encodé par `URLSearchParams`.
- Une entrée non fiable trop volumineuse ou malformée est refusée (400) ; elle n'est ni appliquée partiellement (`configRead` échoue avant toute affectation) ni persistée.
- Aucun nouveau point d'entrée en dehors de `POST /api/set`.

## Décisions prises
- P0-5 : clos en documentation, sans changement de code.
- P0-4 : POST pour `configmaxpv` seulement, GET retiré. Le reste de P1-2 est traité plus tard.
- P0-3 : `DynamicJsonDocument` 2048 (3072 si le test natif l'exige), 400 en cas d'échec, `configWrite` n'écrit pas un document en débordement.
- Validation par test natif PlatformIO, ajouté au contrat et à la CI. Le test est volontairement pessimiste : un slot ArduinoJson est 2× plus gros sur un hôte 64 bits.
- UI : reboot chaîné au succès de la sauvegarde (règle P1-9 pour admin.html).
- README : note sur la rupture d'API ; PDF inchangé.
- Boot : statu quo.

## Risques
- Rupture d'API : une intégration qui appelle `GET /api/set?configmaxpv` reçoit désormais « Bad request or request unknown ».
- Parsing des paramètres POST form-urlencoded par ESPAsyncWebServer 3.12.1 : à confirmer sur matériel (sauvegarde depuis l'UI, mot de passe avec `&#+%`).
- Validation finale sur appareil, faite par l'utilisateur : flash firmware + filesystem, sauvegarde d'une config à longueurs max, reboot et relecture.

## Taille retenue
M : 8 fichiers suivis sur trois zones (firmware ESP, UI, CI/test), surface de sécurité limitée (transport du mot de passe MQTT).
