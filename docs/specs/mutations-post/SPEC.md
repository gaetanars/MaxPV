# SPEC : mutations de l'API en POST avec en-tête anti-CSRF (mutations-post)

## Contexte
ROADMAP P1-2, P1-4, et la course erase/reboot d'`admin.html` (même nature que P1-9).
- P1-2 : `/api/action` (`restart`, `resetindex`, `saveindex`, `saveparam`, `loadparam`, `format`, `eraseconfigesp`, `rebootesp`, `booston`, `boostoff`) et `/api/set` (`param`+`value`, `relaymode`+`value`, `ssrmode`+`value`) sont en `HTTP_GET` (`MaxPV3/MaxPV3.ino`, `setWebHandlers()`). Une `<img src="http://maxpv.local/api/action?format">` sur n'importe quelle page du LAN formate l'EEPROM du Nano. `configmaxpv` est déjà en `POST /api/set` (spec `config-json-post`), mais sans protection : un POST `application/x-www-form-urlencoded` est une requête « simple » CORS, qu'un `<form>` auto-soumis depuis un site tiers peut envoyer.
- Exiger un en-tête personnalisé force un preflight CORS. L'ESP ne répond à `OPTIONS` que par `onNotFound` (redirection 302), donc le navigateur bloque la requête. Aucun en-tête `Access-Control-*` n'est émis aujourd'hui.
- Les erreurs de mutation répondent aujourd'hui 200 avec un texte (« Bad request », « Unknown request »).
- P1-4 : `setParamEcoPV()` concatène `param` et `value` bruts dans `SETPARAM,<param>,<value>,END#`. `value=5#FORMAT` ou `param=1,END#FORMAT` injecte une seconde commande série vers le Nano (`#` = délimiteur, `EcoPV3.ino:969`).
- `admin.html` (bouton « Retour configuration origine ») envoie `eraseconfigesp` et `rebootesp` en parallèle : le reboot peut passer avant l'effacement.
- Appelants côté UI (fonctions `sendAction`, `sendMode`, `sendParam`, `sendParamByValue`, `saveConfigMaxPV`) : `main.html`, `index.html`, `configuration.html`, `wizard.html`, `admin.html`.

## Objectif
Toute mutation HTTP passe par `POST` avec l'en-tête `X-MaxPV` et ses paramètres dans le corps form-urlencoded. Un GET de mutation reçoit 405, une mutation sans en-tête 403, une requête invalide 400. Aucune valeur de paramètre ne peut injecter de commande série.

## Hors périmètre
- Authentification (P1-1, statu quo) ; l'OTA `/update` (ElegantOTA) reste exposée au CSRF, comme documenté par P1-1.
- `/api/get`, `/api/history`, `/DLconfig` (lectures), MQTT.
- Envoi séquentiel des commandes de l'assistant (P2-7), chaînages `resetindex`/`restart` et `saveparam`/`restart` de `wizard.html`, alerte immédiate de `configuration.html`.
- `Documentation API/API_MaxPV.pdf` (binaire, cf. P3-8) ; `EcoPV3/`, `Librairies IDE/`.
- Montée des versions (`MAXPV_VERSION`, `versionweb.txt`, `latest`, README « Versions »), tag et release : faits par l'utilisateur.
- DNS rebinding, CORS explicite.

## Exigences
Fichiers modifiables : `MaxPV3/MaxPV3.ino`, `MaxPV3/maxpv_validate.h` (nouveau), `test/test_param_value/test_main.cpp` (nouveau), `MaxPV3/data/main.html`, `MaxPV3/data/index.html`, `MaxPV3/data/configuration.html`, `MaxPV3/data/wizard.html`, `MaxPV3/data/admin.html`, `README.md`. `docs/ROADMAP.md` est modifié localement : il n'est pas suivi par git, ne pas l'ajouter.

1. **Validation de valeur** : `MaxPV3/maxpv_validate.h`, sans dépendance Arduino (`#pragma once`, `<string.h>` au besoin), en-tête de licence LGPL identique aux autres fichiers de `MaxPV3/`. Fonction `inline bool isNumericParamValue(const char *s)` : vraie si et seulement si `s` est non nul, de 1 à 15 caractères, composé d'un `-` optionnel en première position, de chiffres et d'au plus un `.`, avec au moins un chiffre. (15 = 32 octets réservés pour la commande moins `SETPARAM,NN,` et `,END#`.)
2. **Test natif** `test/test_param_value/test_main.cpp` (Unity, `#include "maxpv_validate.h"`, style de `test/test_config_size`) :
   - `test_valid_values` : `"5"`, `"-3.5"`, `"0.001"`, `".5"`, `"1000"`, `"123456789012345"` (15 car.) → vrai ;
   - `test_rejected_values` : `nullptr`, `""`, `"-"`, `"."`, `"-."`, `"5#FORMAT"`, `"1,END#FORMAT"`, `"1.2.3"`, `"3-"`, `"--1"`, `" 5"`, `"1e5"`, `"Infinity"`, `"NaN"`, `"1234567890123456"` (16 car.) → faux.
   `[env:native]` (`platformio.ini`) est inchangé (`-I MaxPV3` suffit).
3. **Helpers serveur** dans `MaxPV3.ino`, avant `setWebHandlers()` :
   - `static bool checkMutationHeader(AsyncWebServerRequest *request)` : si `!request->hasHeader("X-MaxPV")`, envoie 403 `text/plain` « Forbidden: missing X-MaxPV header » et renvoie false ; sinon true.
   - `static void sendMethodNotAllowed(AsyncWebServerRequest *request)` : réponse 405 `text/plain` « Method Not Allowed: use POST » avec l'en-tête `Allow: POST` (`beginResponse` + `addHeader("Allow", "POST")` + `send`).
   - `#include "maxpv_validate.h"` avec les autres includes locaux.
4. **Handlers**, dans cet ordre dans `setWebHandlers()` : `/api/action` GET, `/api/action` POST, `/api/get` GET (inchangé), `/api/set` GET, `/api/set` POST, `/api/history` (inchangé).
   - `/api/action` et `/api/set` en `HTTP_GET` : appellent seulement `sendMethodNotAllowed(request)`.
   - Chaque handler `HTTP_POST` commence par `if ( !checkMutationHeader ( request ) ) return;`. Tous ses `hasParam`/`getParam` lisent le corps (second argument `true`).
   - `/api/action` POST : mêmes 10 actions et mêmes effets qu'aujourd'hui (y compris la garde de température de `booston`) ; succès → 200 « Request successfully processed » ; action inconnue → 400 « Unknown request ».
   - `/api/set` POST, branches dans l'ordre :
     - `configmaxpv` + `value` : comportement actuel inchangé (200 / 400 « Bad request: invalid configuration ») ;
     - `param` + `value` : `int p = getParam(F("param"), true)->value().toInt()` ; `value` avec `,` → `.` puis `trim()`. Si `p` hors `1..NB_PARAM-1` ou `!isNumericParamValue(value.c_str())` → 400 « Bad request: invalid parameter ». Sinon `setParamEcoPV ( String ( p ), value )` et 200 ;
     - `relaymode` + `value` / `ssrmode` + `value` : effets actuels (dont `relaisPlusIndex = -1` pour `relaymode`) ; valeur hors `stop|force|auto` → 400 « Bad request » ; sinon 200 ;
     - sinon → 400 « Bad request or request unknown ».
   - Aucun en-tête `Access-Control-*` ajouté, aucun handler `HTTP_OPTIONS`.
   - Vérifier dans la source de la lib (`.pio/libdeps/d1_mini/ESPAsyncWebServer/src/`) que les en-têtes de requête sont conservés par défaut (pas de filtrage « interesting headers ») ; sinon, faire le nécessaire pour que `X-MaxPV` soit conservé et le consigner dans `progress.md`.
5. **UI** : dans chacune des 5 pages, une fonction :
   ```js
   function postApi(url, params, onDone) {
     var xhttp = new XMLHttpRequest();
     xhttp.onreadystatechange = function () {
       if (this.readyState == 4 && onDone) onDone(this.status == 200);
     };
     xhttp.open("POST", url, true);
     xhttp.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
     xhttp.setRequestHeader("X-MaxPV", "1");
     xhttp.send(new URLSearchParams(params).toString());
   }
   ```
   - `sendAction(action, ele, onDone)` → `postApi("api/action", [[action, ""]], onDone)` ; `ele.blur()` seulement si `ele` est fourni ; le reste du corps actuel (alertes, `fillParam()` de `configuration.html`) est conservé.
   - `sendMode(name, ele)` → `postApi("api/set", [[name, ""], ["value", <id sans relay/ssr>]])`.
   - `sendParam` / `sendParamByValue` → `postApi("api/set", [["param", num_param], ["value", value_param]])`.
   - `admin.html` : `saveConfigMaxPV(onDone)` envoie via `postApi("api/set", { configmaxpv: "", value: JSON.stringify(doc) }, onDone)`. Bouton « Retour configuration origine » : `sendAction('eraseconfigesp', this, function (ok) { ... })` ; si `ok`, l'alerte actuelle puis `sendAction('rebootesp')` ; sinon alerte `danger` « Échec de l'effacement de la configuration » dans `liveAlertPlaceholderAdvancesAction`, sans reboot.
   - Plus aucune URL `api/set?` ni `api/action?` dans `MaxPV3/data/`.
6. **README** (`## API`) : remplacer la phrase sur `POST /api/set` par : toutes les mutations (`/api/action`, `/api/set`) passent par `POST`, paramètres dans le corps `application/x-www-form-urlencoded` (mêmes noms qu'avant), en-tête `X-MaxPV` obligatoire (403 sinon) ; un GET sur ces routes renvoie 405 ; les erreurs renvoient 400 ; exemple `curl -X POST -H 'X-MaxPV: 1' -d 'booston=' http://<ip>/api/action` ; le PDF n'est pas à jour sur ce point.
7. **ROADMAP (local)** : P1-2 et P1-4 passent à ✅ (« — traité » au titre, `### ✅ P1-2 ...`, `### ✅ P1-4 ...`) ; ligne de synthèse dans « Avancement » (spec `mutations-post`, en-tête `X-MaxPV`, 405/403/400, erase+reboot chaîné) ; P1-1 à P1-9 non traités mis à jour en conséquence.
8. **Cohérence** : `.venv/bin/python -I scripts/check_versions.py` reste à 0.

## Exigences de sécurité
- Une mutation sans en-tête `X-MaxPV` n'a aucun effet (403 avant toute lecture de paramètre).
- Une mutation en GET n'a aucun effet (405).
- Aucun caractère hors `-0-9.` n'atteint la liaison série via `SETPARAM` ; `param` est reformaté depuis l'entier validé.
- Aucun en-tête CORS permissif.

## Décisions prises
- POST + en-tête `X-MaxPV` (présence seule) sur toutes les mutations, y compris le POST config existant.
- GET de mutation → 405 + `Allow: POST` ; erreurs → 400 ; textes de réponse existants conservés.
- P1-4 inclus, validation dans `maxpv_validate.h` testée nativement.
- Chaînage erase → reboot dans `admin.html`.
- `postApi` dupliquée par page (pas de JS partagé : aucune route statique n'existe pour un `.js`).
- README mis à jour ; PDF inchangé.

## Risques
- Rupture d'API : toute intégration (domotique, scripts) doit passer en POST avec `X-MaxPV`.
- Conservation des en-têtes par ESPAsyncWebServer 3.x et parsing du corps form-urlencoded : à confirmer sur matériel.
- L'assistant peut calculer une valeur non numérique (`Infinity` si Papp = 0, cf. P2-6, ou notation exponentielle) : elle est désormais refusée (400) au lieu d'être envoyée au Nano.
- Validation finale sur appareil, faite par l'utilisateur : flash firmware + filesystem ; boutons de `main`/`index`/`configuration`/`wizard`/`admin` ; `curl` GET → 405, POST sans en-tête → 403.

## Taille retenue
M : ~10 fichiers dans un module (firmware ESP et son UI), surface de sécurité limitée (API locale, liaison série).
