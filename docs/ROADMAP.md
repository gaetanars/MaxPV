# Roadmap MaxPV! : audit du code et de la CI

Audit du 2026-10-22 sur `main` @ 5e73771 (MaxPV! 3.61, EcoPV 3.60, Web 3.63).
Statut : **confirmé** = démontré par la lecture du code ou un run CI ; **à vérifier** = plausible, à reproduire (banc ou test natif) avant correction.
Effort : S < 1 h, M = une demi-journée, L > 1 jour.

Mise à jour du 2026-10-23 sur `main` @ 7f44c0c (MaxPV! 4.00) : voir « Avancement » ci-dessous. Les items traités sont marqués ✅ (le texte d'origine est conservé comme historique).

Légende des priorités :
- **P0** : cassé aujourd'hui, ou risque de perte de config / boucle de reboot.
- **P1** : sécurité et robustesse majeures.
- **P2** : justesse fonctionnelle.
- **P3** : qualité, maintenance, confort.

---

## Avancement

| Item | État | Livré par |
|---|---|---|
| P0-1, CI-1 à CI-9 | ✅ traités | `47a23c3` (spec `ci-hardening`, run PR vert) |
| CI-8 (b) | ✅ assets à noms fixes, README sur `releases/latest/download/`, `Binaires MaxPV/` supprimé | `47a23c3` |
| P3-6 | ◐ partiel : AsyncElegantOTA, `Librairies IDE/` et libs khoih-prog remplacés ; `NTPClient` toujours déclaré (`platformio.ini:29`) | `e29ccc7`, `8fc50b2`, `2530e2f` (spec `elegantota-v4`, CI verte) |
| P0-2 | ✅ `configBounded()` borne les 12 champs numériques de `configRead()` sur les min/max d'`admin.html` (clamp, correction réécrite dans `config.json`). Test fonctionnel sur appareil à faire (`mqtt_period` = 0 via `/api/set`) | `9c4d5d6` (spec `bornes-config`, PR #4, CI verte) |
| P0-3 | ✅ `JSON_CONFIG_SIZE` = 2048 (suffisant : 1691 o utilisés en lecture, 1320 o en écriture sur hôte 64 bits) ; `configWrite()` n'écrit pas un document en débordement ; échec de lecture remonté en HTTP 400 ; test natif `pio test -e native` en CI | spec `config-json-post`, PR #5 mergée (`95d5cb0`, CI verte) |
| P0-4 | ✅ config envoyée en `POST /api/set` (corps form-urlencoded via `URLSearchParams`), GET `configmaxpv` retiré ; P1-9 réglé pour `admin.html` (reboot chaîné au succès de la sauvegarde) | spec `config-json-post`, PR #5 mergée (`95d5cb0`, CI verte) |
| P0-5 | ✅ traité : liens et `latest` vers gaetanars/MaxPV (`1b1e1c0`) ; liens d'aide EcoPV (`configuration.html:64`, `README.md:8`) et `credits.html` conservés volontairement (documentation et attribution) | `47a23c3`, `1b1e1c0`, `95d5cb0` (spec `config-json-post`, PR #5 mergée) |
| P1-2, P1-4 | ✅ toutes les mutations (`/api/action`, `/api/set`) en `POST` avec en-tête `X-MaxPV` obligatoire (403 sinon), GET → 405, erreurs → 400 ; `param` reformaté depuis l'entier, `value` validée par `isNumericParamValue` (test natif) ; reboot chaîné au succès de l'effacement dans `admin.html`. Test sur appareil à faire | `1c8349a` (spec `mutations-post`, PR #6 mergée, CI verte) |
| P1-9, P3-4 | ✅ réglés en marge : reboot chaîné au succès de la sauvegarde (`admin.html:62-67`) ; `whatsnew` affiché via `textContent` (`main.html:175`) | PR #5, PR #3 |
| P1-3, P2-8 | ✅ `mqtt_pass` retiré de `/api/get?configmaxpv` et `/DLconfig` (`sendConfig`), vide/absent = conservé, `mqtt_user` vide = effacé ; test sur appareil à faire | `c1ea113` (PR #8, branche `fix/mqtt-pass-masque`) |
| P1-5, P2-3, P2-4, P2-5 (côté ESP), P2-6 et P2-7 (côté UI), P2-9, P3-2, P3-3, P3-5, P3-6, P3-8, P3-9 | ✅ traités ou rendus obsolètes (MaxPV! 4.00 : lecture du payload sur `len`, historique, index journaliers, planificateur, NTP et ancienne API supprimés, topics `maxpv/<id>`, `NTPClient` retiré) | spec `ha-mqtt-ui` (PR #9 mergée, `7f44c0c`, CI de la PR verte ; run `main` en cours, test sur matériel et UI en navigateur à faire) |
| P1-6 à P1-8, P2-1, P2-2, P3-1, P3-7 | ☐ non traités (`EcoPV3.ino` intact) | |
| Nouveautés 4.00 | ✅ Home Assistant via MQTT (autodiscovery de 59 entités, `maxpv_mqtt.h` testé en natif), UI à 5 onglets (Pico CSS, JS vanilla, sans ressource externe), backend HTTP interne (`/status`, `/params`, `/config`, `/cmd`, `/backup`), blueprints HA (BOOST programmé, RelayPlus, relais miroir) contrôlés par `check_blueprints.py`, CI sans FTP, doc API PDF supprimée | PR #9 (`7f44c0c`) |

Notes de livraison :
- Versions épinglées (PlatformIO 6.2.0, plateforme et libs exactes) ; pile async esp32async ; ElegantOTA 4.0.0 (AGPL-3.0, cf. `credits.html` et README).
- Remplacements de libs archivées : `AsyncMQTT_Generic` -> `heman/AsyncMqttClient-esphome` 2.1.0, `AsyncHTTPRequest_Generic` -> `boblemaire/asyncHTTPrequest` 1.2.2. `clearQueue()` retiré (absent du fork ; la file est vidée à la déconnexion).
- Build local : impossible nativement sur macOS arm64, faisable via Docker arm64 (cf. `CLAUDE.local.md`) ; la validation finale reste la CI. `pio check` reste non bloquant.
- P1-1 : le portail ElegantOTA ne demande toujours aucun identifiant (décision (c) statu quo).

---

## P0 : bloquant

### ✅ P0-1 CI cassée : actions d'artefacts v3 retirées (confirmé, S) — traité
- `.github/workflows/build.yml:29,40` : `actions/upload-artifact@v3` et `download-artifact@v3`. GitHub refuse ces versions depuis le 30/01/2025. Le dernier run amont (25969879355) échoue dès « Set up job », donc aucun build ni release n'est produit.
- Proposition : passer à `upload-artifact`/`download-artifact` v4, en même temps que les autres actions (voir CI-1).

### ✅ P0-2 Division par zéro si `mqtt_period` = 0 : boucle de reboot (confirmé, S) — traité
- `MaxPV3.ino:1339` calcule `generalCounterSecond % mqttPeriod`. `configRead()` (`:658-710`) ne valide aucune valeur. Le formulaire impose `min=1` (`admin.html:161`), mais pas l'API `/api/set?configmaxpv`, ni un JSON restauré puis modifié.
- Impact : avec MQTT actif et une période à 0, l'ESP plante (exception IntegerDivideByZero) à chaque seconde. La config fautive est persistée, d'où une boucle de reboot.
- Proposition : borner toutes les valeurs dans `configRead()` (période ≥ 1, heures 0-23, minutes 0-59, ratio 0-100, ports 1-65535, durées > 0). Rejeter la config sinon.
- Réalisé : clamp et non rejet (un rejet au boot efface `config.json` et force le mode AP, cf. P0-3). La correction est persistée ; l'API reste silencieuse.

### ✅ P0-3 Taille JSON de config trop juste : config refusée ou effacée (à vérifier, S) — traité
- `JSON_CONFIG_SIZE` vaut 1100 (`maxpv_defines.h:50`). `deserializeJson` lit un `const String&`, donc ArduinoJson copie les clés et les valeurs dans le pool. Estimation pour 29 clés : environ 464 o de slots, 371 o de clés et 150 à 250 o de valeurs, soit déjà 985 à 1085 o avec les valeurs par défaut. Un utilisateur ou mot de passe MQTT long (jusqu'à 39 car.) ou des chemins de relais longs font dépasser la limite, et la lecture échoue (`NoMemory`).
- Impact :
  - au boot, `setup()` (`:331-336`) efface `config.json` et redémarre en mode AP, ce qui fait perdre la config Wifi (`resetSettings`, `:358`) ;
  - via l'UI, `/api/set` répond « Request successfully processed » alors que rien n'est enregistré.
  `configWrite()` n'échoue pas, car les clés littérales ne sont pas copiées : on écrit un fichier qu'on ne sait pas relire.
- Proposition : monter `JSON_CONFIG_SIZE` à environ 2048, ou utiliser `StaticJsonDocument`, et traiter le code d'erreur. Valider d'abord par un test natif (ArduinoJson 6 sur host) avec des champs à longueur maximale.

### ✅ P0-4 Mot de passe MQTT non encodé dans l'URL (confirmé, S) — traité
- `admin.html:434-436` met `JSON.stringify(doc)` brut dans la query string GET. Un `&`, `#`, `+` ou `%` dans le mot de passe ou l'utilisateur MQTT casse ou altère la config (`+` devient espace, `#` tronque, `&` coupe le paramètre).
- Proposition : `encodeURIComponent()` côté UI, a minima. Mieux : passer en POST (voir P1-2).

### ✅ P0-5 Couper la dépendance à l'amont Jetblack31 (confirmé, S) — traité (liens EcoPV et `credits.html` conservés volontairement)
- `main.html:107` lit `https://raw.githubusercontent.com/Jetblack31/MaxPV/main/latest`. Dès que les versions du fork divergent, l'UI annonce à tort des « mises à jour » vers les versions amont et affiche leur `whatsnew`, qui est du HTML distant exécuté localement (voir P3-4).
- Autres références à l'amont :
  - liens d'aide : `main.html:91`, `configuration.html:64` (EcoPV) ;
  - `latest:5`, `README.md` (liens d'installation et de binaires) ;
  - `CLAUDE.md` (« Dépôt amont », branche `main` de Jetblack31).
  À conserver : les en-têtes de copyright et `credits.html` (attribution et notices LGPL).
- Proposition : pointer `urlVersion` vers `gaetanars/MaxPV`, ou supprimer la vérification en ligne ; réécrire `latest` ; mettre à jour les liens du README et le `CLAUDE.md` projet. Les liens d'aide vers la doc EcoPV amont peuvent rester (contenu documentaire).
- Le fork a le même workflow, donc le même échec CI que P0-1. Vérifier aussi que GitHub Actions est activé sur le fork : `gh run list` n'y renvoie aucun run.

---

## P1 : sécurité et robustesse

### P1-1 Aucune authentification : UI, API, OTA (confirmé, M) — décision requise
- `/update` (AsyncElegantOTA, `MaxPV3.ino:1932-1933`) ne demande pas d'identifiants : n'importe qui sur le LAN peut flasher un firmware.
- `/api/action` permet `format` (EEPROM Nano), `resetindex`, `eraseconfigesp` et `rebootesp` sans contrôle (`:2050-2075`).
- `/api/set` modifie les paramètres de régulation et la config (`:2135-2172`).
- Options :
  - (a) Basic Auth sur `/update`, `/api/set`, `/api/action` et `/DLconfig`, identifiants dans la config. Simple et supporté par ESPAsyncWebServer (`request->authenticate`), mais casse les intégrations qui appellent l'API.
  - (b) Basic Auth sur `/update` seulement : risque principal couvert, aucune rupture.
  - (c) Statu quo documenté (« réseau de confiance »).
- Recommandation : (b) tout de suite, (a) en option désactivée par défaut.

### ✅ P1-2 Actions destructrices en GET, donc CSRF (confirmé, M) — traité
- Toutes les mutations passent par `HTTP_GET`. N'importe quelle page web ouverte sur le LAN peut déclencher `http://maxpv.local/api/action?format` avec une simple `<img>`.
- Proposition : mutations en POST seulement. Ce n'est pas une protection CSRF complète, mais cela bloque les vecteurs triviaux. Il faut adapter le frontend et la doc API (`Documentation API/API_MaxPV.pdf`), ce qui casse l'API.

### ✅ P1-3 Secrets exposés (confirmé, S) — traité
- `/api/get?configmaxpv` (`:2080-2081`) et `/DLconfig` (`:2039-2042`) renvoient `mqtt_pass` en clair.
- Proposition : masquer `mqtt_pass` dans `configmaxpv` ; si le champ reçu est vide, conserver l'ancien mot de passe. `/DLconfig` reste complet (c'est une sauvegarde), mais derrière l'auth de P1-1.

### ✅ P1-4 Injection de commandes série vers le Nano (confirmé, S) — traité
- `setParamEcoPV()` (`:1035-1051`) concatène `param` brut (seul `toInt()` est contrôlé) et `value` (où seule la virgule est remplacée). Avec `param=1,END#FORMAT` ou `value=5#FORMAT`, le Nano reçoit une seconde commande arbitraire, puisque `#` sert de délimiteur.
- Proposition : reformater `param` à partir de l'entier et n'accepter que `[-0-9.]` dans `value`.

### ✅ P1-5 `onMqttMessage` lit au-delà du payload (confirmé, S) — traité
- `:1763-1784` construit `String(payload)` alors que le payload AsyncMqttClient n'est pas terminé par un NUL (`len` est fourni). Lecture hors buffer : comportement indéfini, crash possible.
- Proposition : comparer avec `strncmp(payload, "...", len)` ou construire la `String` sur `len` octets.

### P1-6 Boot ESP bloqué indéfiniment (confirmé, S)
- `setup()` `:444,450,455` : `while (!serialProcess())` sans timeout. Si le Nano se tait après le premier contact (`fatalError()` bloque environ 20 s ; `startPVR()` attend sans fin les passages à zéro, `EcoPV3.ino:931-933`), l'ESP ne démarre jamais le serveur web.
- Proposition : timeout de quelques secondes sur chaque attente.

### P1-7 Portail Wifi sans timeout après une coupure secteur (à vérifier, S)
- `wifiManager.autoConnect(SSID_CP)` (`:368`) n'a pas de `setConfigPortalTimeout`. Si la box redémarre moins vite que l'ESP, celui-ci peut rester en portail AP ouvert (SSID « MaxPV » sans mot de passe) au lieu de réessayer.
- Proposition : `setConfigPortalTimeout(180)` puis `ESP.restart()` quand une config existe. À valider en coupant la box.

### P1-8 Écriture de config non atomique (confirmé, S)
- `configWrite()` (`:753-755`) écrase `config.json` sur place. Une coupure pendant l'écriture laisse un fichier corrompu ; au boot suivant, P0-3 s'applique (effacement et mode AP).
- Proposition : écrire `config.tmp`, puis `rename`.

### ✅ P1-9 Sauvegarde et reboot en concurrence depuis l'UI (à vérifier, S) — traité pour `admin.html`
- `admin.html:62-65` envoie `saveConfigMaxPV()` et `rebootesp` en parallèle. Si `rebootesp` arrive avant la requête de config (URL longue), `loop()` (`:576`) redémarre sans sauvegarder.
- Proposition : déclencher `rebootesp` dans le callback de succès de la sauvegarde, ou ajouter un paramètre `reboot` à `/api/set?configmaxpv`.

---

## P2 : justesse fonctionnelle

### P2-1 BOOST et RelayPlus passent outre un arrêt manuel (confirmé, S) — décision requise
- Le BOOST programmé (`MaxPV3.ino:1387-1390`) démarre même si le SSR est en STOP. Pendant le BOOST, le Nano force FORCE/STOP (`EcoPV3.ino:1403-1404`), et en fin de BOOST le SSR passe en AUTOM (`:1398`) au lieu de revenir au mode précédent.
- RelayPlus remet chaque jour le relais en AUTOM (`MaxPV3.ino:1402`).
- Impact : un STOP posé pour une intervention sur le chauffe-eau est levé automatiquement.
- Options : (a) ne pas lancer de BOOST ni d'init RelayPlus si le mode est STOP ; (b) mémoriser le mode et le restaurer en fin de BOOST ; (c) statu quo documenté. Recommandation : (a) + (b).

### P2-2 Limite de température qui laisse passer en cas d'échec de lecture (confirmé, S) — décision requise
- Si la sonde est absente ou en défaut, `temperatureECS` vaut -99 (`EcoPV3.ino:1503-1510`). Les contrôles ESP (`MaxPV3.ino:962,1389,1781,2070`) laissent alors le BOOST démarrer et ne l'arrêtent jamais. La sonde n'est détectée qu'au boot (`EcoPV3.ino:547`).
- Proposition : quand `temp_limit_active` est actif et la température vaut -99, refuser ou arrêter le BOOST et logger. Le thermostat du ballon reste la vraie sécurité ; c'est donc un choix de comportement.

### ✅ P2-3 Index journaliers faux après un reboot de l'ESP (confirmé, M) — traité
- `setRefIndexJour()` est appelé au boot (`:479`). Après une OTA, un crash ou le watchdog, les compteurs « du jour » repartent de zéro. Si le Nano n'a pas encore répondu, la référence est vide et le compteur du jour égale le total (`index.html:281-285`).
- L'historique (`energyIndexHistoric`, `:197`) n'existe qu'en RAM : il est perdu à chaque reboot.
- Proposition : persister les références du jour avec leur date dans LittleFS et les recharger si la date correspond. L'historique peut éventuellement être persisté à chaque relevé (toutes les 30 min, usure flash acceptable).

### ✅ P2-4 Planificateur : double déclenchement et heure non synchronisée (confirmé, S) — traité
- `timeScheduler()` (`:1370-1404`) tourne toutes les 59,654 s : environ une fois toutes les 170 minutes, il s'exécute deux fois dans la même minute. Le BOOST est alors relancé (durée prolongée) et RelayPlus réinitialisé. Le commentaire `:624-625` prévoit ce cas, mais le code ne le gère pas.
- Il n'y a aucune garde `isTimeTrue` : avant la synchro NTP, l'heure vaut 01:00 (epoch 0 en heure de Paris).
- Proposition : mémoriser la dernière minute traitée et ne rien faire tant que `isTimeTrue` est faux.

### ✅ P2-5 Messages Nano sans terminateur `#` (confirmé, S) — traité
- `EcoPV3.ino:1125,1142,1400,1415` envoient `BOOSTTIME,-1,END` et `RELAYPLUSTIME,-1,END` sans `#`. Côté ESP, le message est fusionné avec la trame suivante, et la trame STATS qui suit est perdue.
- `RELAYPLUSTIME` n'est pas traité côté ESP (`serialProcess`, `:906-1010`).
- Proposition : ajouter `#` ; traiter ou ignorer explicitement `RELAYPLUSTIME`.

### ✅ P2-6 `P_CALIB` peut valoir 0 (confirmé, S) — traité
- La borne min est 0 (`EcoPV3.ino:277`). L'assistant calcule `p_calib = ... / Papp` (`wizard.html:433`) : si Papp vaut 0, on envoie `Infinity`, que le Nano ramène à 0. Au redémarrage suivant, `0.5 / P_CALIB` (`:1684-1715`) est infini : consigne et anti-windup sont faux.
- Proposition : borne min > 0 (par exemple 0.001) côté Nano, garde `Papp > 0` côté assistant.

### ✅ P2-7 Rafales de commandes : risque de saturer le buffer série du Nano (à vérifier, M) — traité
- L'assistant envoie 4 à 5 `SETPARAM` en parallèle (`wizard.html:393-408,447-450`). Chaque commande fait environ 20 octets, le buffer RX du Nano 64 octets, et `temperatureRead()` bloque environ 94 ms. Des commandes peuvent être perdues sans erreur.
- Proposition : envoi séquentiel côté UI (attendre la réponse avant la suivante), et vérification par relecture des paramètres (déjà faite via `shouldReadParams`).

### ✅ P2-8 `configRead` : bug de clé `mqtt_pass` (confirmé, S) — traité
- `:683` teste `containsKey("mqtt_user")` pour lire `mqtt_pass`. Si la clé `mqtt_pass` manque, le mot de passe devient `"null"` (ArduinoJson 6, `as<String>()` sur une valeur null).
- Proposition : tester `mqtt_pass`.

### ✅ P2-9 Topics MQTT fixes (confirmé, M) — traité
- Tous les topics sont en dur sous `maxpv/` (`maxpv_defines.h:179-209`). Avec deux MaxPV sur le même broker, états et commandes se mélangent. L'`uniq_id` HA est unique, mais les `stat_t`/`cmd_t` sont partagés.
- Proposition : préfixe configurable, `maxpv` par défaut pour rester compatible.

---

## P3 : qualité et maintenance

- **P3-1** Code mort ou trompeur : variables locales qui masquent les globales `maxTemp`/`tempLimitActive` dans `timeScheduler()` (`MaxPV3.ino:1392-1394`) ; branche dupliquée `EcoPV3.ino:813-814`.
- **P3-2** ✅ Log `[WARNING LOOP TIME]` (`MaxPV3.ino:604`) publié en retain à chaque boucle de plus de 100 ms : bruit MQTT. Ajouter un seuil ou un rate-limit.
- **P3-3** ✅ Frontend dépendant d'Internet : Bootstrap et Highcharts viennent de CDN, sans LAN isolé il n'y a plus d'UI. Highcharts n'a pas d'attribut SRI (`graph.html:188-194`). Ajouter le SRI, au minimum.
- **P3-4** ✅ (traité, `textContent`) `whatsnew` du fichier `latest` injecté en `innerHTML` (`main.html:172,180`) : le contenu distant s'exécute avec les droits de l'UI locale, donc avec accès à l'API sans auth. Après P0-5, le risque se limite à une compromission de mon propre dépôt. Utiliser `textContent` ou une liste blanche.
- **P3-5** ✅ Assistant : le bouton « Commencer » remet les index d'énergie à zéro (`wizard.html:125`) sans confirmation.
- **P3-6** ◐ (reste `NTPClient`) Dépendances : `AsyncElegantOTA` est déprécié (remplacé par ElegantOTA v3 en mode async) ; les libs khoih-prog sont archivées ; `NTPClient` est déclaré mais inutilisé (`platformio.ini:28`, l'heure passe par `configTzTime`).
- **P3-7** Nano sans watchdog matériel (`wdt_enable`). Index sauvegardés une fois par heure (`EcoPV3.ino:1343`) : jusqu'à 1 h de comptage perdu sur coupure. Choix de conception, à documenter.
- **P3-8** ✅ Doc API en PDF binaire (`Documentation API/API_MaxPV.pdf`) : impossible de suivre ses changements avec git. Envisager une version Markdown.
- **P3-9** ✅ `favicon.ico` servi en `image/png` (`MaxPV3.ino:2035`) ; `onNotFound` redirige vers `/` au lieu de renvoyer 404, ce qui gêne les clients de l'API.

---

## CI / automatisations GitHub

### ✅ CI-1 Mettre les actions à jour et les épingler (P0 avec P0-1, S) — traité
- `checkout@v3`, `cache@v3`, `setup-python@v4`, `upload/download-artifact@v3` et `svenstaro/upload-release-action@v2` (tag mutable, action tierce).
- Proposition : versions courantes épinglées par SHA avec un commentaire de version. Remplacer l'action tierce de release par `gh release upload` (CLI déjà présente sur le runner).

### ✅ CI-2 Permissions minimales (P1, S) — traité
- Pas de bloc `permissions` : le token reçoit les droits par défaut du dépôt.
- Proposition : `permissions: contents: read` au niveau du workflow, `contents: write` sur le seul job release.

### ✅ CI-3 Builds reproductibles (P1, S) — traité
- `pip install --upgrade platformio` sans version ; `platform = espressif8266` non épinglé ; `lib_deps` en `^`. Le même tag peut donner un binaire différent d'un jour à l'autre.
- Clé de cache statique `${{ runner.os }}-pio` (`build.yml:16`), jamais invalidée.
- Python 3.9 en fin de vie.
- Proposition : épingler PlatformIO, la plateforme et les libs (versions exactes) ; clé de cache `hashFiles('platformio.ini')` ; Python 3.12 ou 3.13.

### ✅ CI-4 Déclencheurs (P2, S) — traité
- Seulement `push` : pas de `pull_request` (les PR de fork ne sont pas validées), pas de `workflow_dispatch`, pas de `concurrency`.
- Proposition : `push` (branches + tags), `pull_request` et `workflow_dispatch` ; `concurrency` avec annulation des runs obsolètes.

### ✅ CI-5 Compiler aussi EcoPV et amorceESP (P2, M) — traité
- Le firmware Nano n'est compilé nulle part : une erreur de syntaxe passe inaperçue.
- Proposition : job `arduino-cli compile --fqbn arduino:avr:nano` avec OneWire et DallasTemperature, plus une variante `OLED_128X64` ; `amorceESP` pour esp8266. Ajouter aussi une variante ESP avec `MAXPV_FTP_SERVER` activé.

### ✅ CI-6 Contrôle de cohérence des versions (P2, S) — traité
- Une release touche à la main `maxpv_defines.h`, `versionweb.txt`, `EcoPV3.ino:87`, `latest` et `README.md`.
- Proposition : sur un tag, un script vérifie que `latest` (`maxpvver`/`webver`/`ecopvver`) correspond aux trois sources et que le tag correspond à `MAXPV_VERSION`. Le job échoue sinon.

### ✅ CI-7 Dependabot pour les actions (P3, S) — traité
- `.github/dependabot.yml` (écosystème `github-actions`, mensuel) pour maintenir les SHA épinglés.

### ✅ CI-8 Binaires commités dans le dépôt (P3) — décision requise — traité
- `Binaires MaxPV/*.bin` (1 Mo par release) gonflent l'historique et doublonnent les assets de release. Mais le README s'appuie sur ces noms fixes pour l'installation initiale.
- Options : (a) statu quo ; (b) la CI attache aussi des assets à noms fixes (`MaxPV3_firmware.bin`) à la release, et le README pointe vers `releases/latest/download/...`. Recommandation : (b), une fois CI-1 en place.

### ✅ CI-9 Analyse statique (P3, M) — traité
- Optionnel : `pio check` (cppcheck) en mode non bloquant, pour repérer des cas comme P2-8 et P3-1.

---

## Décisions prises (2026-10-22)
- **P1-1** : (c) statu quo. Pas d'authentification, à documenter comme « réseau de confiance » (README). L'OTA `/update` reste ouverte.
- **P1-2** : passage des mutations en POST (rupture d'API assumée). Règle aussi P0-4 : envoyer la config en corps POST.
- **P2-1** : (a) + (b). Pas de BOOST ni d'init RelayPlus si le mode est STOP ; fin de BOOST = retour au mode mémorisé.
- **P2-2** : limite de température active et sonde en défaut (-99) : refuser ou arrêter le BOOST, avec log.
- **CI-8** : (b). La CI attache les assets à noms fixes à la release ; le README pointe vers `releases/latest/download/...` ; `Binaires MaxPV/` est retiré du dépôt ensuite (après CI-1).

Périmètre : ce fork (gaetanars/MaxPV) est autonome. Pas de PR amont, pas de compatibilité imposée avec Jetblack31/MaxPV. Les ruptures d'API (P1-1, P1-2) ne concernent que mes propres intégrations.
