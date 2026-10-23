# SPEC : p2-justesse

## Contexte
Roadmap (`docs/ROADMAP.md`) après MaxPV! 4.00 et `p1-robustesse` : il reste quatre P2 à traiter.
- P2-1 : un BOOST démarre même si le SSR est en STOP. En fin de BOOST, le Nano repasse le SSR en AUTOM (`EcoPV3.ino:1398`) au lieu de revenir au mode d'avant. Pendant un BOOST, le SSR alterne FORCE/STOP (burst, `EcoPV3.ino:1403-1404`), donc `triac_mode` côté ESP ne dit pas si le SSR était en STOP avant le BOOST. RelayPlus est désormais le blueprint `relayplus.yaml`. Il pose lui-même STOP quand la durée max est atteinte, puis AUTO à minuit : un STOP manuel ne peut pas en être distingué.
- P2-2 : la sonde absente ou en défaut donne `temperatureECS = -99` (valide entre 1 et 94 °C, `EcoPV3.ino:1506-1509`). Avec la limite active, `handleCommand` accepte alors le BOOST (`maxpv_mqtt.h:464`, -99 < max) et `serialProcess` ne l'arrête jamais (`MaxPV3.ino:783`).
- P2-5, côté Nano : `EcoPV3.ino:1125,1142,1400,1415` émettent `BOOSTTIME,-1,END` / `RELAYPLUSTIME,-1,END` sans le terminateur `#`.
- P2-6, côté Nano et bornes : `P_CALIB` a une borne min à 0 côté Nano (`EcoPV3.ino:277`, bornes entières `paramInConfig`), dans l'entité HA (`maxpv_mqtt.h:130`) et dans l'UI (`app.js:9`). Avec 0, `0.5 / P_CALIB` (`EcoPV3.ino:1685-1715`) est infini.
- En production, `logSerial` ne sort rien (`serialLogAllowed = false`, `MaxPV3.ino:357`).

## Objectif
Clore tous les P2 de la roadmap : P2-1, P2-2, et les restes Nano de P2-5 et P2-6.

## Hors périmètre
- Bump de version (`VERSION`, `MAXPV_VERSION`, `versionweb.txt`), `latest`, section « Versions » du README, tag : release séparée.
- Nouvelle entité HA ou nouveau canal de log pour la sonde en défaut.
- Toute logique ajoutée au blueprint RelayPlus (entrée « pause », etc.).
- Garde STOP côté Nano sur `SETBOOST`.
- P3-1, P3-7 et tout autre item P3.
- Changement du format du protocole série (hors ajout du `#`).

## Exigences
Fichiers modifiables : `EcoPV3/EcoPV3.ino`, `MaxPV3/MaxPV3.ino`, `MaxPV3/maxpv_mqtt.h`, `MaxPV3/data/app.js`, `test/test_mqtt/test_main.cpp`, `blueprints/automation/maxpv/relayplus.yaml`, `README.md`, `docs/ROADMAP.md`.

1. **P2-1 refus du BOOST en STOP (ESP)** : `MqttCmdContext` (`maxpv_mqtt.h`) gagne deux champs ajoutés en fin de structure, `bool ssrStop;` et `bool boostActive;`. Les initialisations agrégées existantes restent valides. `handleCommand` refuse `boost` = `ON` si `ctx.ssrStop` est vrai et `ctx.boostActive` est faux : il renvoie `false`, donc HTTP 400 sur `/cmd` et commande MQTT ignorée. Un BOOST en cours reste relançable. `currentCmdContext()` (`MaxPV3.ino`) renseigne `ctx.ssrStop = ( ecoPVStats[TRIAC_MODE].toInt() == STOP )` et `ctx.boostActive = ( boostTime != -1 )`. `boost` = `OFF` reste toujours accepté.
2. **P2-1 restauration du mode (Nano)** : nouvelle globale `byte triacModeBeforeBoost` dans `EcoPV3.ino`. Dans `SETBOOST`, quand un BOOST démarre (`boostTime == -1` avant la commande et `bT != 0`), le Nano exécute `triacModeBeforeBoost = triacMode;` avant de mettre à jour `boostTime`. Une relance pendant un BOOST ne réécrit pas la valeur mémorisée. En fin de BOOST (branche `boostTime == 0`), `triacMode = triacModeBeforeBoost;` remplace `triacMode = AUTOM;`. Un `SETSSR` pendant un BOOST garde son comportement actuel : le BOOST est annulé et le mode demandé est conservé.
3. **P2-1 RelayPlus (doc)** : la description de `relayplus.yaml` gagne la phrase « Un STOP manuel du relais est levé à minuit (passage en auto) : désactivez l'automatisation pendant une intervention. ». `check_blueprints.py` passe.
4. **P2-2 sonde en défaut (ESP)** : la température est invalide si elle est ≤ 0. Cela couvre -99 et la valeur vide d'avant la première trame STATS. Quand `tempLimitActive` est vrai, `handleCommand` refuse `boost` = `ON` si `ctx.temp <= 0` ou si `!(ctx.temp < ctx.maxTemp)`. Dans `serialProcess`, à la réception d'une trame STATS, `boostOFF()` est appelé si `tempLimitActive == ON`, `boostTime > 0`, et que `ecoPVStats[TEMP_ECS].toInt() <= 0` ou `> maxTemp`. Aucun log ni entité ajoutés.
5. **P2-5 terminateurs (Nano)** : les quatre émissions `BOOSTTIME,-1,END` et `RELAYPLUSTIME,-1,END` de `EcoPV3.ino` se terminent par `,END#`.
6. **P2-6 P_CALIB > 0** :
   - `maxpv_mqtt.h:130` : borne min de `p_calib` à `0.0001` (ligne `"number|P_CALIB|Calibrage puissance|p_calib|p||||c||0.0001|1|0.0001\n"`). `handleCommand` rejette donc `0`, et la discovery publie `min` = 0.0001 ;
   - `app.js:9` : `['p_calib', 'P_CALIB', 'Facteur de calibrage de la puissance', 0.0001, 1, 0.0001]` ;
   - Nano, `SETPARAM` : pour `P_CALIB` (`index == 1`), une valeur ≤ 0 après `constrain` n'est pas écrite et le Nano répond `NOK:SETPARAM,OK,` ;
   - Nano, `eeConfigRead` : `P_CALIB` n'est chargé depuis l'EEPROM que si `pvrConfig.p_calib > 0`. Sinon, la valeur courante est conservée : 0.111 au boot, valeur en RAM sur un `LOADCFG`.
7. **Tests natifs** (`test/test_mqtt/test_main.cpp`) : le helper `cmd()` gagne deux paramètres par défaut, `bool ssrStop = false` et `bool boostActive = false`. Nouvelles fonctions enregistrées par `RUN_TEST` :
   - `test_boost_ssr_stop` : ON refusé si `ssrStop` et pas de BOOST actif ; accepté si `ssrStop` et `boostActive` ; OFF accepté si `ssrStop` ;
   - `test_boost_sonde_defaut` : limite active, ON refusé à -99 et à 0, accepté à 20 avec un max à 45 ; limite inactive, ON accepté à -99 ;
   - `test_p_calib_positive` : `p_calib` = `0` refusé, `0.0001` et `0.111` acceptés.
   `PARAM_MIN` du test de discovery passe à 0.0001 pour P_CALIB. Les tests existants restent verts.
8. **README** : dans « Mode BOOST » (`README.md:97`), « À l'arrêt du BOOST, le SSR repasse en mode AUTO. » devient « À l'arrêt du BOOST, le SSR revient au mode qu'il avait avant le BOOST. Un BOOST est refusé si le SSR est en STOP. ». Dans le paragraphe de la limite de température (`README.md:102`), la phrase « Si la sonde est absente ou en défaut et que la limite est activée, le BOOST est refusé ou arrêté. » est ajoutée.
9. **Roadmap** : le tableau « Avancement » de `docs/ROADMAP.md` gagne une ligne `| P2-1, P2-2, P2-5 (côté Nano), P2-6 (côté Nano) | ✅ … | spec p2-justesse |` (test sur matériel à faire). La ligne ☐ ne liste plus que P3-1 et P3-7. Les titres de P2-1 et P2-2 portent ✅, avec le texte d'origine conservé. « Décisions prises » note que RelayPlus (blueprint) est traité par documentation.

## Exigences de sécurité
- Aucune nouvelle entrée non fiable : les commandes passent toujours par `handleCommand`. Les refus ajoutés ne font que restreindre ce qui est accepté.
- P2-2 renforce le comportement en sécurité : avec la limite active, une sonde absente ou en défaut empêche ou arrête le BOOST. Le thermostat du ballon reste la vraie sécurité.
- P2-1 : un STOP posé pour une intervention n'est plus levé par un BOOST, ni par sa fin.

## Décisions prises
- Périmètre : P2-1, P2-2, P2-5 (Nano) et P2-6 (Nano et bornes), en un seul reflash du Nano.
- P2-1 (a) : refus côté ESP seulement, si le SSR est en STOP et qu'aucun BOOST n'est en cours. P2-1 (b) : mode mémorisé et restauré par le Nano.
- RelayPlus : la levée du STOP manuel à minuit est documentée dans le blueprint, sans logique ajoutée.
- P2-2 : température ≤ 0 = invalide. Pas de canal de log dédié : le refus se voit par un HTTP 400, par le switch Boost qui reste ou repasse à OFF, et par la température -99 dans HA.
- P2-6 : défense ESP (bornes) et Nano (SETPARAM et chargement EEPROM).
- Pas de bump de version.

## Risques
- Firmware Nano modifié : nécessite un reflash via l'IDE Arduino. Un ESP neuf avec un Nano ancien garde l'ancien comportement de fin de BOOST (AUTOM) et les messages sans `#`.
- Sans sonde (`isTemperatureSensorPresent` faux), la limite active rend le BOOST impossible : c'est le comportement voulu, documenté dans le README.
- Avant la première trame STATS (environ 1 s après le boot), la limite active refuse le BOOST.
- Validation limitée à la compilation et aux tests natifs. Restent à tester sur matériel : BOOST refusé en STOP, mode FORCE restauré après un BOOST, sonde débranchée pendant un BOOST, `P_CALIB` = 0 refusé.

## Taille retenue
M : 8 fichiers, aucune dépendance nouvelle, aucune surface de sécurité ajoutée ; deux firmwares touchés par des changements locaux, sans changement du format du protocole série.
