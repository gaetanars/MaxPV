# SPEC ha-mqtt-ui : MaxPV! 4.00, Home Assistant via MQTT et UI web de configuration

## Contexte
MaxPV! (fork autonome gaetanars/MaxPV) : ESP8266 (`MaxPV3/MaxPV3.ino`, ~2300 lignes) relié en série à un Nano EcoPV. Aujourd'hui, l'ESP embarque :
- une UI de 8 pages LittleFS (`MaxPV3/data/`, Bootstrap et Highcharts en CDN) ;
- une API HTTP publique documentée en PDF (`/api/get|set|action|history`, `/DLconfig`), qui sert aussi de backend à l'UI ;
- un historique en RAM, des index journaliers, le BOOST programmé, RelayPlus, un relais distant miroir (HTTP), un FTP optionnel ;
- un WatchDog Wifi (ping DNS1), le NTP, une IP statique obligatoire.

Le MQTT actuel publie des topics fixes `maxpv/*` et une discovery HA de 18 entités copiée-collée (`MaxPV3.ino:1461-1778`). Il ne couvre ni les paramètres EcoPV, ni les réglages, ni les boutons, ni les diagnostics. `onMqttMessage` lit `String(payload)` sans terminateur (ROADMAP P1-5).

## Objectif
HA via MQTT devient l'usage principal : autodiscovery complète (mesures, modes, réglages, diagnostics, boutons). Une UI web agréable, en une seule page à onglets, sans ressource externe, permet de configurer et de dépanner. Elle s'appuie sur un backend HTTP interne minimal, non documenté. Le WatchDog Wifi est conservé et un watchdog MQTT est ajouté. BOOST programmé, RelayPlus et relais miroir passent dans des blueprints HA fournis. Version MaxPV! 4.00, Web 4.00.

## Hors périmètre
- Firmware EcoPV (`EcoPV3/EcoPV3.ino`) : aucune modification, protocole série inchangé.
- Entité `update` HA.
- Authentification HTTP (décision ROADMAP P1-1 : réseau de confiance, `/update` ouvert).
- Purge automatique des anciens topics retain (documentée seulement).
- Correctifs ROADMAP non liés : P1-6, P1-7, P1-8, P2-1, P2-2, P2-6, P2-7.
- `SchemasMaxPV/` inchangé.

## Fichiers sources de sketch autorisés
`MaxPV3/MaxPV3.ino`, `MaxPV3/maxpv_defines.h`, `MaxPV3/maxpv_options.h`, `MaxPV3/maxpv_validate.h`, nouveau `MaxPV3/maxpv_mqtt.h`. `EcoPV3/EcoPV3.ino` interdit.

## Exigences

### R1 Suppressions côté firmware ESP
Supprimer entièrement le code, les variables, les constantes, les clés de config et les libs de :
- l'historique (`energyIndexHistoric`, `recordHistoricData`, `initHistoric`, `HISTORY_*`) et les index journaliers (`setRefIndexJour`, `INDEX_*_J`, `NB_STATS_SUPP`) ;
- le BOOST programmé (`boostTimer*`), RelayPlus (`relayPlus*`, `relaisPlus*`, `relayPlusEcoPV`) et `timeScheduler` avec sa tâche ;
- le relais distant (`remoteRelay`, `relaisDistant*`, `REMOTE_RELAY_*`, lib `boblemaire/asyncHTTPrequest`) ;
- le FTP (`MAXPV_FTP_SERVER`, section de `maxpv_options.h`, `LOGIN_FTP`/`PWD_FTP`, lib `xreef/SimpleFTPServer`) ;
- le NTP et l'heure (`configTzTime`, `TIME_ZONE`, `NTP_SERVER`, `TIME_REF`, `checkTimeSet`, `isTimeTrue`, `#include <TZ.h>`, lib `arduino-libraries/NTPClient`) ;
- l'API HTTP actuelle (`/api/action`, `/api/get`, `/api/set`, `/api/history`, `/DLconfig`) et les routes par page ;
- `logMqtt`/`maxpv/SYS/logger`, `espTransmitInfos`/`maxpv/SYS/*`, `mqtt_active`, `http_port` (port fixe 80) et le log `[WARNING LOOP TIME]`. Les appels `logMqtt` restants deviennent `logSerial` ou disparaissent.

Restent inchangés : `TickerScheduler`, la lib `akajes/AsyncPing(esp8266)`, ElegantOTA, mDNS `maxpv`, `formatEepromEcoPV`.

### R2 Configuration ESP
- `config.json` (LittleFS) contient exactement ces clés (noms existants conservés pour la migration) :
  - réseau : `ip`, `gateway`, `subnet`, `dns1`, `dns2` ;
  - watchdogs : `watchdogwifi_active`, `watchdogmqtt_active` ;
  - MQTT : `mqtt_ip` (hôte : IP ou nom DNS, 64 caractères max), `mqtt_port`, `mqtt_user`, `mqtt_pass`, `mqtt_period` ;
  - BOOST : `boost_duration`, `boost_ratio`, `max_temp`, `temp_limit_active`.
  Les clés inconnues d'un ancien fichier sont ignorées et disparaissent à la réécriture suivante.
- Bornes, appliquées par clamp via `configBounded` existant :

  | Clé | Bornes | Défaut |
  |---|---|---|
  | `mqtt_port` | 1-65535 | 1883 |
  | `mqtt_period` | 1-3600 | 10 |
  | `boost_duration` | 10-600 | 120 |
  | `boost_ratio` | 0-100 | 100 |
  | `max_temp` | 15-90 | 45 |
  | `temp_limit_active`, `watchdogwifi_active`, `watchdogmqtt_active` | 0/1 | 0, 1, 1 |

- Réseau : `ip` vide = DHCP (défaut, `DEFAULT_IP` etc. vides). `ip` non vide : `gateway`, `subnet` et `dns1` doivent être des IPv4 valides (`IPAddress::fromString`), sinon DHCP et log ; `setSTAStaticIPConfig` n'est appelé que dans ce cas. Les configs 3.x gardent donc leur IP fixe.
- Le portail WiFiManager (AP « MaxPV ») ne gère que le Wifi. Il s'ouvre, comme aujourd'hui, quand aucun Wifi n'est mémorisé ou joignable. L'absence de `config.json` ou de la clé `ip` ne force plus le mode AP ni `resetSettings()`. Fichier illisible : supprimé, valeurs par défaut, pas de redémarrage.
- MQTT actif si et seulement si `mqtt_ip` est non vide (défaut `""`). `mqttClient.setServer(host, port)` reçoit la chaîne d'hôte.
- `JSON_CONFIG_SIZE` est réduit à la taille nécessaire, vérifiée par le test natif `test_config_size` adapté. Valeurs à longueur max : hôte 64 car., user 39, pass 39 contenant `&#+%"\`, et IP de 15 car.

### R3 Watchdogs
- WatchDog Wifi : mécanisme actuel conservé (`watchDogWifi`, ping toutes les 30 s, `PING_WIFI_TIMEOUT`). Cible : `dns1` si l'IP est fixe, sinon `WiFi.gatewayIP()`. Activable par `watchdogwifi_active`.
- Watchdog MQTT : si `watchdogmqtt_active` vaut 1 et que MQTT est actif mais déconnecté depuis plus de `MQTT_WATCHDOG_TIMEOUT` (600000 ms, `maxpv_defines.h`), alors `ESP.restart()` après un log série.
- Contact Nano (`watchDogContactEcoPV`) inchangé.

### R4 Structure MQTT
- `<id>` = `maxpv` + `ESP.getChipId()` (identique à l'actuel `deviceID`). Base des topics : `maxpv/<id>`.
- Availability : `maxpv/<id>/status`. LWT retain `offline` ; `online` retain à la connexion.
- État : `maxpv/<id>/state`, un seul JSON retain, publié toutes les `mqtt_period` secondes et immédiatement après chaque commande de mode ou de BOOST. Clés :
  - mesures : `vrms`, `irms`, `pact`, `papp`, `prouted`, `pimp`, `pexp`, `cosphi`, `pimpulsion` ;
  - index : `idx_routed`, `idx_import`, `idx_export`, `idx_impulsion` (kWh, conversion existante), `idx_relay` (min) ;
  - modes : `triac_mode`, `relay_mode` (codes Nano : 0 stop, 1 force, autre auto) ;
  - états : `relay_on` (bit 2 de `STATUS_BYTE`, `ON`/`OFF`), `temp`, `boost` (`ON`/`OFF` selon `boostTime`), `nano` (`ON`/`OFF` selon `contactEcoPV`) ;
  - diagnostics : `rssi`, `uptime` (s), `heap`, `reset_reason`, `ecopv_version`, `delay_min`, `delay_avg`, `delay_max`, `bias_offset`, `status_byte`, `ontime`, `samples`.
  Le JSON est produit par une fonction unique (`buildStateJson`) avec ArduinoJson dans un buffer fixe. Un test natif vérifie que l'état aux longueurs max tient dans ce buffer.
- Réglages : `maxpv/<id>/params`, JSON retain produit par `buildParamsJson`, avec :
  - les 16 paramètres EcoPV : `v_calib`, `p_calib`, `phase_calib`, `p_offset`, `p_resistance`, `p_margin`, `gain_p`, `gain_i`, `e_reserve`, `p_div2_active`, `p_div2_idle`, `t_div2_on`, `t_div2_off`, `t_div2_tc`, `cnt_calib`, `p_installpv` ;
  - les réglages ESP : `boost_duration`, `boost_ratio`, `max_temp`, `temp_limit`, `mqtt_period`.
  Publié à la connexion, à chaque trame `PARAM` reçue du Nano et à chaque changement de réglage ESP.
- Commandes : `maxpv/<id>/<clé>/set`, avec un abonnement unique `maxpv/<id>/+/set`. Le payload est lu sur `len` octets (jamais `String(payload)` non terminé, corrige P1-5). Longueur max 16, sinon le message est ignoré.

### R5 Discovery HA
- Une entrée par entité dans une table `PROGMEM` de `MaxPV3/maxpv_mqtt.h` : composant, suffixe d'uniq_id, nom, clé JSON, device_class, state_class, unité, catégorie, activée par défaut, bornes et pas pour les number.
- Topic et payload sont générés par des fonctions pures du même header (accès `_P` ; shims `PROGMEM`/`memcpy_P`/`strncpy_P` pour la compilation native), testées en natif.
- Topic : `homeassistant/<component>/<id>/<id><suffixe>/config` (schéma actuel). `uniq_id` = `<id><suffixe>`.
- Les 18 suffixes existants sont conservés à l'identique, avec le même composant, pour que HA garde les entités et leur historique :
  - sensors `Tension`, `Courant`, `PuissanceApparente`, `FacteurPuissance`, `PuissanceActive`, `PuissanceRoutee`, `PuissanceImportee`, `PuissanceExportee`, `PuissanceProduite`, `Temperature`, `EnergieRoutee`, `EnergieImportee`, `EnergieExportee`, `EnergieProduite`, `TempsFonctionnementRelais` ;
  - selects `SSR`, `Relais` ;
  - switch `Boost`.
  Device, device_class, state_class et unités inchangés pour ceux-ci : `dev.ids` = `<id>`, `name` « MaxPV », `mdl` « MaxPV! », `mf` inchangé, `sw` = `MAXPV_VERSION`, `cu` = `http://<ip>/`.
- Toutes les entités : `avty_t` = `maxpv/<id>/status` (payloads `online`/`offline`), `stat_t` = `maxpv/<id>/state` ou `/params`, `val_tpl` `{{ value_json.<clé> }}` (selects : mapping existant 0→stop, 1→force, sinon auto).
- Nouvelles entités :
  - binary_sensor `EtatRelais` (`relay_on`, device_class `power`) ; binary_sensor `ContactRouteur` (`nano`, device_class `connectivity`, catégorie diagnostic) ;
  - sensors diagnostic : `RSSI` (dBm, `signal_strength`), `Uptime` (s, `duration`), `VersionEcoPV`. Désactivés par défaut : `MemoireLibre` (B), `RaisonReset`, `DelayMin`, `DelayAvg`, `DelayMax`, `BiasOffset`, `StatusByte`, `OnTime`, `Samples` ;
  - number catégorie config, `mode: box`, sur `/params` :
    - les 16 paramètres EcoPV, suffixes `V_CALIB` … `P_INSTALLPV`. Bornes du tableau `pvrParamConfig` d'`EcoPV3.ino` : V_CALIB 0-2, P_CALIB 0-1, PHASE_CALIB -16..48, P_OFFSET -100..100, P_RESISTANCE 100-10000, P_MARGIN -2000..2000, GAIN_P 0-1000, GAIN_I 0-1000, E_RESERVE 0-200, P_DIV2_ACTIVE 0-9999, P_DIV2_IDLE 0-9999, T_DIV2_ON 0-240, T_DIV2_OFF 0-240, T_DIV2_TC 0-60, CNT_CALIB 0-1000, P_INSTALLPV 100-30000. Pas de 0.0001 pour V_CALIB, P_CALIB et CNT_CALIB, 1 sinon ;
    - `BoostDuree` (10-600 min), `BoostRatio` (0-100 %), `TempMax` (15-90 °C), `PeriodeMQTT` (1-3600 s) ;
  - switch catégorie config `LimiteTemperature` (`temp_limit`) ;
  - buttons : `RedemarrerESP`, `RedemarrerRouteur` (RESET Nano), `SauverParametres` (SAVECFG), `RechargerParametres` (LOADCFG), `SauverIndex` (SAVEINDX), `RAZIndex` (INDX0, désactivé par défaut).
- Publication progressive et non bloquante : une entité par passage dans `loop()` après la connexion. Si `publish()` renvoie 0, nouvel essai au passage suivant. Un seul buffer de payload, réutilisé.

### R6 Commandes : une seule fonction pour MQTT et HTTP
- `handleCommand(key, value, fromHttp)` est une fonction pure de validation dans `maxpv_mqtt.h`, testée en natif, suivie de l'action dans `MaxPV3.ino`. Elle est appelée par `onMqttMessage` (`fromHttp` = false) et par `POST /cmd` (`fromHttp` = true).
- `triac_mode`/`relay_mode` : `stop`/`force`/`auto` en égalité exacte → `SSRModeEcoPV`/`relayModeEcoPV`. Tout autre payload est ignoré.
- `boost` : `ON` → `boostON()` si `temp_limit` est inactif ou si `temp < max_temp` ; `OFF` → `boostOFF()`.
- Paramètres EcoPV : valeur validée par `isNumericParamValue`, puis par les bornes de R5 ; ensuite `setParamEcoPV` (reformatage de `param` existant).
- Réglages ESP : bornes de R2, mise à jour de la variable, `shouldSaveConfig = true`, republication de `/params`.
- Buttons : payload `PRESS` → action correspondante. `RedemarrerESP` passe par `shouldReboot`.
- Uniquement si `fromHttp` vaut true : `format` (`formatEepromEcoPV`) et `eraseconfigesp` (payload `PRESS`). Ils ne sont jamais acceptés en MQTT.
- Tout autre cas : ignoré en MQTT, 400 en HTTP. La limite de température locale reste : arrêt du BOOST dans `serialProcess` si `temp_limit` est actif et `temp > max_temp`.

### R7 Backend HTTP interne (remplace l'API)
Routes autorisées :
- `/`, `/index.html`, `/app.js`, `/pico.min.css`, `/maxpv.css`, `/favicon.ico` (servi en `image/x-icon`), `/versionweb.txt` : fichiers LittleFS ;
- `GET /status` : `buildStateJson` + `mqtt` (`ON`/`OFF` connecté), `ip`, `maxpv_version`, `mqtt_id` (`<id>`) ;
- `GET /params` : `buildParamsJson` ;
- `GET /config` : config de R2 sans `mqtt_pass` ;
- `POST /config` : corps `application/x-www-form-urlencoded`, clés de R2. `mqtt_pass` vide ou absent = conservé ; `mqtt_user` vide = authentification désactivée. Valeurs hors format → 400 ; nombres hors bornes → clamp. Succès → sauvegarde, réponse 200, puis redémarrage via `shouldReboot`. Sert aussi à la restauration d'une sauvegarde ;
- `POST /cmd` : corps form-urlencoded `key`, `value` → `handleCommand(key, value, true)` ; 200 ou 400 ;
- `GET /backup` : `config.json` en pièce jointe, sans `mqtt_pass` ;
- `/update` : ElegantOTA, inchangé.

Tout `POST` exige l'en-tête `X-MaxPV`, sinon 403 (`checkMutationHeader` existant). `GET` sur une route POST seule → 405. Toute autre route → 404 `text/plain` (plus de redirection). Le serveur fonctionne sans broker.

### R8 UI web : page unique à onglets
- Fichiers `MaxPV3/data/` à la fin :
  - `index.html` ;
  - `app.js` (JS vanilla, aucune bibliothèque) ;
  - `pico.min.css` (Pico CSS 2.1.1, MIT, copie de `@picocss/pico@2.1.1/css/pico.min.css` ; SHA-256 noté dans le README) ;
  - `maxpv.css` (surcharges légères) ;
  - `favicon.ico` ;
  - `versionweb.txt`.
  Supprimés : `admin.html`, `configuration.html`, `credits.html`, `graph.html`, `main.html`, `maj.html`, `wizard.html`. Aucune ressource externe (script, feuille de style, police) ; le mode sombre automatique de Pico est conservé.
- Navigation par ancre (`#etat`, `#routeur`, `#reseau`, `#etalonnage`, `#systeme`), un `<section id="tab-…">` par onglet, un seul visible à la fois. L'en-tête affiche l'état Nano, l'état MQTT et la version.
- **État** : rafraîchi toutes les 5 s via `GET /status`. Puissances, température, index, état du relais, RSSI, uptime. Sélecteurs SSR et relais (stop/force/auto) et bouton BOOST via `POST /cmd`.
- **Routeur** : les 16 paramètres EcoPV (libellés et bornes de R5, aide reprise de `configuration.html`), plus BOOST (durée, puissance) et limite de température (switch, max). Chaque champ est envoyé par `POST /cmd`. Boutons Sauver/Recharger les paramètres en EEPROM.
- **Réseau** : MQTT (hôte, port, user, mot de passe en écriture seule avec un placeholder « inchangé », période, identifiant `<id>` et préfixe de topics affichés), IP fixe optionnelle (vide = DHCP), activation des deux watchdogs. Envoi par `POST /config` (redémarrage annoncé).
- **Étalonnage** : l'assistant de `wizard.html` est porté (mêmes étapes et calculs). Le calcul `p_calib` exige `Papp > 0`, sinon un message d'erreur (corrige le côté UI de P2-6). Les `SETPARAM` sont envoyés un par un, chaque envoi attendant la réponse du précédent (côté UI de P2-7). La RAZ des index au démarrage de l'assistant passe par une `confirm()` (P3-5).
- **Système** :
  - versions MaxPV!, Web et EcoPV, avec vérification de mise à jour via `latest` (URL et affichage `textContent` de `main.html` repris) ;
  - lien vers `/update` ;
  - sauvegarde (`/backup`) et restauration (fichier → `POST /config`) ;
  - redémarrer l'ESP ou le Nano, RAZ des index, formatage EEPROM, effacement de la config ESP : chacune de ces actions passe par une `confirm()` ;
  - crédits et licences repris de `credits.html` (Bernard Legrand / Jetblack31 LGPL 2.1+, ElegantOTA AGPL-3.0, Pico CSS MIT) et lien d'aide EcoPV.
- Tous les envois passent par une fonction unique `post(url, params)` (`fetch`, en-tête `X-MaxPV`, `URLSearchParams`) qui affiche le succès ou l'erreur. `app.js` n'utilise aucune route `/api/`.

### R9 Versions et contrôle
- `MAXPV_VERSION` "4.00", `MAXPV_VERSION_FULL` "MaxPV! 4.00", `versionweb.txt` "4.00", version EcoPV inchangée.
- `latest` : `maxpvver` "4.00", `webver` "4.00", `ecopvver` égal à la version EcoPV, et un `whatsnew` qui décrit la 4.00.
- `scripts/check_versions.py` est inchangé dans sa logique : il doit passer, ainsi que `--tag v4.00`.

### R10 CI et release (`.github/workflows/build.yml`)
- Supprimer la variante FTP. Conserver buildfs, l'image fusionnée `MaxPV<ver>_full.bin` et les assets à noms fixes.
- Job `lint` : installer `pyyaml==6.0.2` et exécuter `python3 -I scripts/check_blueprints.py`.
- Actions épinglées par SHA conservées ; aucune nouvelle action.

### R11 Blueprints HA
Fichiers `blueprints/automation/maxpv/` :
- `boost_programme.yaml` : inputs heure (time), switch Boost (entity, domaine switch), jours (optionnel) ; lance `switch.turn_on` à l'heure ;
- `relayplus.yaml` : inputs select Relais, binary_sensor EtatRelais, sensor `history_stats` de durée ON du jour (documenté), durée min, durée max, heure de fin. Le relais passe en `stop` quand le max est atteint, en `force` quand le min ne peut plus être atteint avant l'heure de fin, et en `auto` au début de journée ;
- `relais_miroir.yaml` : inputs binary_sensor EtatRelais et entité cible (switch) ; recopie ON/OFF.

`scripts/check_blueprints.py` (PyYAML, loader qui accepte le tag `!input`) vérifie chaque `blueprints/**/*.yaml` :
- YAML valide ;
- `blueprint.name` présent, `blueprint.domain` = `automation`, `blueprint.input` non vide ;
- chaque input déclaré est utilisé au moins une fois via `!input`, et aucun `!input` ne pointe vers un input non déclaré.
Sortie non nulle en cas d'échec ; échec aussi si aucun blueprint n'est trouvé.

### R12 Suppressions hors firmware et documentation
- Supprimer `Documentation API/`, `scripts/test_elegantota_v4.py` et `images/mainpage.png`.
- README réécrit :
  - présentation « Home Assistant via MQTT d'abord » et caractéristiques ;
  - installation (Nano inchangé, Wemos via `MaxPV3_full.bin` comme aujourd'hui, portail Wifi, puis UI sur `http://maxpv.local/`) et mise à jour OTA (firmware puis filesystem) ;
  - section UI (onglets) ;
  - section HA : entités, topics, bouton « SauverParametres », index journaliers via `utility_meter`, dashboard Énergie, import des blueprints ;
  - section Watchdogs ;
  - migration depuis 3.x : fonctions retirées et leurs remplacements, IP fixe conservée, API supprimée, purge des anciens topics retain avec `mosquitto_pub -r -n -t <topic>` pour `maxpv/vrms` … `maxpv/temperature`, `maxpv/state` et `maxpv/SYS/*` ;
  - section Licence (attributions de R8) et SHA-256 de `pico.min.css` ;
  - entrée `### **V 4.00** - <date du jour JJ/MM/AAAA>` avec les trois numéros de version.
  Supprimer les sections API, Relais distant, FTP et TELNET.
- `CLAUDE.md` projet : description, architecture (MQTT, `maxpv_mqtt.h`, `handleCommand`, routes de R7, UI à une page), mention « doc API PDF » retirée, contrat agentique de ce plan.
- `docs/ROADMAP.md` : ligne d'avancement `ha-mqtt-ui` listant les items traités ou rendus obsolètes : P1-5, P2-3, P2-4, P2-5 (côté ESP), P2-6 et P2-7 (côté UI), P2-9, P3-2, P3-3, P3-5, P3-6, P3-8, P3-9.

## Exigences de sécurité
- Entrées non fiables : payloads MQTT (n'importe quel client du broker), corps `POST /config` et `POST /cmd`, fichier de restauration. Validation stricte : égalité exacte, `isNumericParamValue`, bornes, IPv4 via `fromString`, longueurs bornées (payload 16, hôte 64, user/pass 39). Lecture sur `len`. Aucun `,` ni `#` n'est transmis au Nano.
- `mqtt_pass` n'est jamais renvoyé : ni dans `/config`, `/backup`, `/status`, ni dans la discovery, l'état ou les logs.
- Tout `POST` HTTP exige `X-MaxPV` (403 sinon) ; GET sur une mutation → 405.
- Formatage EEPROM et effacement de config : HTTP seulement (UI avec `confirm()`), jamais en MQTT. `RAZIndex` MQTT désactivé par défaut.
- L'UI ne charge aucune ressource tierce. Le seul appel externe est le GET de `latest` (raw.githubusercontent.com, `textContent`).
- `/update` et l'UI sans authentification : statu quo assumé (décision P1-1).

## Décisions prises
- Remplace le cadrage `ha-mqtt-only` ; son volet MQTT est repris (topics `maxpv/<id>`, état JSON, params, commandes, discovery PROGMEM, uniq_id conservés).
- API publique supprimée ; backend interne minimal, partagé avec MQTT via `handleCommand`.
- BOOST programmé, RelayPlus et relais miroir → blueprints HA. Historique et index journaliers → statistiques HA / `utility_meter`. NTP supprimé, FTP supprimé.
- Watchdogs : Wifi (ping) conservé, plus un watchdog MQTT de 10 min ; les deux sont activables dans l'UI.
- UI : page unique à 5 onglets, Pico CSS embarqué et JS vanilla, sans CDN. Pilotage des modes et du BOOST dans l'onglet État. Assistant d'étalonnage conservé. Vérification `latest` conservée. Actions destructrices dans l'UI uniquement, avec confirmation.
- Réseau : DHCP par défaut, IP fixe optionnelle, port HTTP fixe 80, portail WiFiManager limité au Wifi.
- Une seule spec, version 4.00. Nano intact. Anciens topics retain : documentation seulement.

## Risques
- Mémoire ESP8266 (discovery d'environ 50 entités plus le serveur web) : publication progressive, buffer unique, table PROGMEM ; le retrait de l'historique, du FTP, du NTP et de asyncHTTPrequest libère de la RAM. `pico.min.css` (~80 Ko) est servi en flux depuis LittleFS.
- Rupture pour les utilisateurs 3.x : API, pages, topics et fonctions locales disparaissent (les entités sont conservées par uniq_id). Tout est documenté dans le README.
- Broker mal saisi avec le watchdog MQTT actif : redémarrage toutes les 10 min, l'UI reste joignable entre deux redémarrages, et le watchdog est désactivable.
- Pendant un redémarrage de l'ESP, le Nano continue de router, mais la limite de température est inactive.
- Validation sur matériel hors CI (discovery, commandes, UI, watchdogs, OTA) : à faire par l'utilisateur après la PR.

## Taille retenue
L : plusieurs modules (firmware ESP, UI, CI, docs, blueprints et script), rupture d'interface et migration.
