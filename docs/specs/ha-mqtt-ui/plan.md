# Plan ha-mqtt-ui : MaxPV! 4.00

Spec : `docs/specs/ha-mqtt-ui/SPEC.md`. Critères : `docs/specs/ha-mqtt-ui/features.json`.
Mode : checkout principal, branche `agent/ha-mqtt-ui`. Pas de push, merge, force.

## Tâches

| Id | Titre | Dépend de |
|---|---|---|
| T1 | Firmware ESP 4.00 | aucune |
| T2 | UI web à 5 onglets | aucune |
| T3 | Blueprints, CI, docs, nettoyage | T1, T2 |

### T1 : firmware ESP 4.00 (F1 à F7, F9 partiel)
Fichiers : `MaxPV3/MaxPV3.ino`, `MaxPV3/maxpv_defines.h`, `MaxPV3/maxpv_options.h`, `MaxPV3/maxpv_validate.h`, `MaxPV3/maxpv_mqtt.h` (nouveau), `platformio.ini`, `test/test_config_size/test_main.cpp`, `test/test_mqtt/test_main.cpp`, `test/test_param_value/test_main.cpp`.

- F1 (R1) : supprimer historique, index journaliers, BOOST programmé, RelayPlus, relais distant, FTP, NTP, ancienne API, `logMqtt`/SYS, `mqtt_active`, `http_port`, `[WARNING LOOP TIME]`. Retirer `asyncHTTPrequest`, `SimpleFTPServer`, `NTPClient` de `platformio.ini` ; garder `AsyncPing` et `TickerScheduler`.
- F2 (R2) : `config.json` à 16 clés, clamps via `configBounded`, DHCP par défaut, IP fixe validée par `IPAddress::fromString`, MQTT actif ssi `mqtt_ip` non vide (hôte 64 car.), WiFiManager limité au Wifi, fichier illisible supprimé sans reboot, `JSON_CONFIG_SIZE` réduit (le test natif exige 1024 au plus).
- F3 (R3) : `watchDogWifi` conservé (cible `dns1` si IP fixe, sinon `WiFi.gatewayIP()`), `MQTT_WATCHDOG_TIMEOUT` 600000 et `ESP.restart()` après log série si `watchdogmqtt_active`.
- F4 (R4) : LWT `maxpv/<id>/status`, état unique `buildStateJson`, `buildParamsJson`, abonnement `maxpv/<id>/+/set`, payload lu sur `len` (max 16), aucun `String(payload)`.
- F5 (R5) : table PROGMEM dans `maxpv_mqtt.h`, publication progressive (une entité par `loop()`, retry si `publish()` renvoie 0, buffer unique).
- F6 (R6) : `handleCommand` dans `maxpv_mqtt.h`, appelé par `onMqttMessage` et `POST /cmd`.
- F7 (R7) : routes statiques, `/status`, `/params`, `/config`, `/cmd`, `/backup`, `/update` ; `checkMutationHeader` (403), 405, 404 `text/plain`, aucun redirect, `mqtt_pass` jamais renvoyé.
- F9 partiel (R9) : `MAXPV_VERSION "4.00"`, `MAXPV_VERSION_FULL "MaxPV! 4.00"`.
- `EcoPV3/EcoPV3.ino` non modifié.

#### API de `maxpv_mqtt.h` (fixée par `test/test_mqtt/test_main.cpp`)
La spec ne précise pas les signatures ; les tests écrits avant l'implémentation les figent.

- Constantes : `MQTT_ENTITY_COUNT` (59 = 27 sensors, 2 binary_sensors, 2 selects, 2 switchs, 20 numbers, 6 buttons), `MQTT_DISCOVERY_BUF_SIZE`, `MQTT_STATE_BUF_SIZE`, `MQTT_PARAMS_BUF_SIZE`, `MQTT_PAYLOAD_MAX` (16).
- Discovery : `size_t mqttDiscoveryTopic(char*, size_t, size_t i, const char *id)` et `size_t mqttDiscoveryPayload(char*, size_t, size_t i, const char *id, const char *ip)` ; retour = longueur écrite, 0 si le buffer est trop petit.
- État : `struct MqttState` (champs `vrms, irms, pact, papp, prouted, pimp, pexp, cosphi, pimpulsion, idxRouted, idxImport, idxExport, idxImpulsion, idxRelay, triacMode, relayMode, relayOn, temp, boost, nano, rssi, uptime, heap, resetReason, ecopvVersion, delayMin, delayAvg, delayMax, biasOffset, statusByte, onTime, samples`) et `size_t buildStateJson(char*, size_t, const MqttState&)` (0 si trop petit, jamais de troncature). Les champs `relayOn`, `boost`, `nano` sortent en `ON`/`OFF`.
- Params : `struct MqttParams { const char *ecopv[16]; long boostDuration, boostRatio, maxTemp, tempLimit, mqttPeriod; }` et `size_t buildParamsJson(char*, size_t, const MqttParams&)`.
- Commandes : `bool handleCommand(const char *key, const char *value, bool fromHttp, const MqttCmdContext &ctx, MqttCommand &out)`.
  - `MqttCmdContext { bool tempLimitActive; double temp; long maxTemp; }` (l'état est passé en paramètre, la fonction reste pure).
  - `MqttCommand { MqttAction action; int mode; int index; long number; char value[16]; }`.
  - Actions : `CMD_NONE, CMD_SSR_MODE, CMD_RELAY_MODE, CMD_BOOST_ON, CMD_BOOST_OFF, CMD_SET_PARAM, CMD_SET_ESP, CMD_RESTART_ESP, CMD_RESET_ECOPV, CMD_SAVECFG, CMD_LOADCFG, CMD_SAVEINDX, CMD_INDX0, CMD_FORMAT, CMD_ERASE_CONFIG_ESP`.
  - Modes : `MQTT_MODE_STOP/FORCE/AUTO`. Réglages ESP (`index`) : `MQTT_ESP_BOOST_DURATION/BOOST_RATIO/MAX_TEMP/TEMP_LIMIT/MQTT_PERIOD`.
  - Clés : `triac_mode`, `relay_mode`, `boost`, les 16 clés de params en minuscules (`v_calib` … `p_installpv`, `index` = rang 0 à 15), `boost_duration`, `boost_ratio`, `max_temp`, `temp_limit` (`ON`/`OFF`), `mqtt_period`, boutons `restartesp`, `resetecopv`, `savecfg`, `loadcfg`, `saveindx`, `indx0` (payload `PRESS`), HTTP seulement `format`, `eraseconfigesp` (payload `PRESS`).
  - Les `cmd_t` de la discovery doivent toutes être acceptées par `handleCommand` (test de cohérence).

Hypothèse (à consigner dans `progress.md`) : un nombre hors bornes dans une commande MQTT ou `/cmd` est rejeté (R6 dit « bornes » sans préciser) ; seul `POST /config` clampe (R2).

Vérification T1 (sous-commandes, le contrat complet n'est vert qu'avec T3) : `pio test -e native`, `pio run -e d1_mini`, greps F1 à F7 et F9 de `test/acceptance_ha_mqtt_ui.sh`.

### T2 : UI web (F8, F9 partiel)
Fichiers : `MaxPV3/data/index.html`, `app.js`, `pico.min.css` (Pico 2.1.1 à l'octet près), `maxpv.css`, `favicon.ico` (existant), `versionweb.txt` ; suppression de `admin`, `configuration`, `credits`, `graph`, `main`, `maj`, `wizard` (`.html`), après reprise de l'aide, de l'assistant, du check `latest` et des crédits.

- 5 sections `tab-(etat|routeur|reseau|etalonnage|systeme)`, navigation par ancre, un onglet visible.
- Aucune ressource externe, pas de `api/` ni `cdn.`, `whatsnew` en `textContent`.
- Fonction unique `post(url, params)` (`fetch`, `X-MaxPV`, `URLSearchParams`).
- Contenu par onglet selon R8 ; `confirm()` pour les actions destructrices et la RAZ des index.
- `versionweb.txt` = 4.00.

Vérification : `pio run -t buildfs -e d1_mini`, greps F8.

### T3 : blueprints, CI, docs, nettoyage (F10 à F13)
Fichiers : 3 blueprints `blueprints/automation/maxpv/`, `scripts/check_blueprints.py`, `.github/workflows/build.yml`, `latest`, `README.md`, `CLAUDE.md`, `docs/ROADMAP.md`, suppression de `Documentation API/`, `scripts/test_elegantota_v4.py`, `images/mainpage.png`, journal `progress.md`.

- `check_blueprints.py` : YAML valide, `blueprint.name`, domaine `automation`, inputs non vides, inputs tous utilisés, aucun `!input` non déclaré, échec si aucun blueprint.
- `build.yml` : variante FTP retirée, job lint avec `pyyaml==6.0.2` et `python3 -I scripts/check_blueprints.py`, aucune nouvelle action.
- `latest` : `maxpvver` et `webver` 4.00, `ecopvver` = version EcoPV, `whatsnew` 4.00 ; `check_versions.py` inchangé.
- README réécrit (R12), SHA-256 de `pico.min.css`, entrée `### **V 4.00** - 23/10/2026`.
- `CLAUDE.md` : contrat build avec `pio run -t buildfs -e d1_mini && `, `maxpv_mqtt.h`, `handleCommand`, routes R7 (modification demandée explicitement par R12).
- ROADMAP : ligne `ha-mqtt-ui` (P1-5, P2-3, P2-4, P2-5, P2-6, P2-7, P2-9, P3-2, P3-3, P3-5, P3-6, P3-8, P3-9).
- F13 : contrat complet vert sur l'ensemble intégré, `EcoPV3.ino` intact ; `gh pr checks` reste en attente de l'autorisation de push (hors limites de cette exécution).

## Tests d'acceptation (écrits avant le code)
- `test/test_mqtt/test_main.cpp` (nouveau) : discovery (59 entités, JSON valide, 18 uniq_id historiques et composants, bornes et pas, 10 entités désactivées, cohérence avec `handleCommand`), état aux longueurs max dans le buffer fixe, params, `handleCommand` (selects exacts, boost et limite, params bornés, injection `,` `#`, réglages ESP, boutons, format/erase HTTP seulement, payload trop long).
- `test/test_config_size/test_main.cpp` (adapté) : 16 clés R2 aux longueurs max (hôte 64, user 39, pass 39 avec `&#+%"\`, IP 15), `JSON_CONFIG_SIZE` au plus 1024.
- `test/test_param_value/test_main.cpp` : inchangé, doit rester vert.
- `test/acceptance_ha_mqtt_ui.sh` (nouveau) : rejoue les critères grep, fichiers et scripts de F1 à F13 (hors commandes `pio` et `gh`). Chaque ligne affiche `ok` ou `FAIL`.

## Environnement et risques
- Les tests natifs (`pio test -e native`) tournent directement avec `.venv/bin/pio` ; les builds ESP/Nano passent par Docker en arm64 natif (jamais `--platform linux/amd64`), cf. `CLAUDE.local.md`.
- `pico.min.css` : copie exacte depuis le paquet npm `@picocss/pico@2.1.1` (réseau requis), SHA-256 noté dans le README.
- Mémoire ESP8266 (discovery de 59 entités, serveur async, Pico servi en flux) : validation sur matériel par l'utilisateur.
- Pas de 0.0001 (`V_CALIB`, `P_CALIB`, `CNT_CALIB`) : la précision dépend du reformatage de `param` vers `SETPARAM` (15 car. max).
- Rupture 3.x : API, pages et topics supprimés, topics retain non purgés (documentation seulement). Config 3.x avec IP fixe invalide : bascule en DHCP.
- Watchdog MQTT avec broker mal saisi : redémarrage toutes les 10 min ; limite de température inactive pendant un reboot.
- Sécurité : égalité exacte, longueurs bornées, lecture sur `len`, aucun `,` ni `#` vers le Nano, `mqtt_pass` absent de `/config`, `/backup`, `/status`, discovery, état, logs ; `format` et `eraseconfigesp` HTTP seulement ; `/update` et l'UI sans authentification (P1-5 corrigé, P1-1 assumé).
- Contrat `test` : `check_blueprints.py` n'existe qu'après T3 et `check_versions.py` n'est cohérent qu'après T1, T2 et T3.
