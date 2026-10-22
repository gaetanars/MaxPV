# SPEC : bornes des valeurs de configuration MaxPV! (bornes-config)

## Contexte
ROADMAP P0-2. `MaxPV3/MaxPV3.ino:1341` calcule `generalCounterSecond % mqttPeriod`. `configRead()` (`MaxPV3/MaxPV3.ino:660-713`) n'applique aucune borne aux valeurs lues. Le formulaire `admin.html` impose des `min`/`max`, mais ni l'API `/api/set?configmaxpv` (`MaxPV3.ino:2166-2169`), ni un JSON restauré puis modifié ne les respectent.
Avec MQTT actif et `mqtt_period` = 0, l'ESP plante (IntegerDivideByZero) à chaque seconde. La config fautive est persistée, ce qui provoque une boucle de reboot.
Points d'attention relevés dans le code :
- `configRead()` affecte les globales au fil de la lecture : un rejet a posteriori laisserait une config à moitié appliquée.
- Au boot, si `configRead()` renvoie false, `config.json` est effacé et l'ESP redémarre en mode AP (`MaxPV3.ino:331-336`), ce qui fait perdre la config Wifi. Rejeter une config persistée fautive est donc exclu.
- `shouldSaveConfig` est traité au boot (`MaxPV3.ino:387`) et dans `loop()` (`:571`).
- ArduinoJson 6.21.6 (`platformio.ini:23`) : `as<long>()` convertit aussi les nombres transmis en chaîne (l'UI envoie des chaînes).

## Objectif
Toute valeur numérique de configuration lue par `configRead()` est ramenée dans les bornes de l'UI (`admin.html`). Aucune configuration, qu'elle vienne de l'API, d'une restauration ou d'un fichier déjà persisté, ne peut plus provoquer de division par zéro ni de valeur hors plage. Une valeur corrigée est réécrite dans `config.json`.

## Hors périmètre
- Les champs booléens `*_active` et les champs texte (IP, utilisateur et mot de passe MQTT, chemins du relais).
- Le bug `containsKey("mqtt_user")` utilisé pour `mqtt_pass` (`MaxPV3.ino:685`).
- La réponse de `/api/set` (toujours « Request successfully processed ») et `admin.html`.
- `EcoPV3/`, `maxpv_defines.h`, `Documentation API/` (le contrat de l'API ne change pas).
- La montée des numéros de version (`MAXPV_VERSION`, `latest`, README « Versions »), le tag et la release : faits par l'utilisateur.
- `docs/ROADMAP.md` (non suivi par git).

## Exigences
Seul fichier source modifié : `MaxPV3/MaxPV3.ino`.
1. **Helper** : juste avant `configRead()`, ajouter, avec un cartouche de commentaire au style du fichier :
   ```cpp
   long configBounded(JsonVariantConst value, long minVal, long maxVal)
   {
     long v = value.as<long>();
     if ((v < minVal) || (v > maxVal)) {
       shouldSaveConfig = true;
       return constrain(v, minVal, maxVal);
     }
     return v;
   }
   ```
2. **Bornes** : dans `configRead()`, chaque affectation numérique ci-dessous remplace `X = jsonConfig["clef"];` par `X = configBounded(jsonConfig["clef"], min, max);`. La structure `if (containsKey) ... else shouldSaveConfig = true;` reste inchangée.

   | clef | variable | min | max |
   |---|---|---|---|
   | http_port | httpPort | 1 | 65535 |
   | mqtt_port | mqttPort | 1 | 65535 |
   | mqtt_period | mqttPeriod | 1 | 3600 |
   | remoterelay_port | relaisDistantPort | 1 | 65535 |
   | boost_duration | boostDuration | 10 | 600 |
   | boost_ratio | boostRatio | 0 | 100 |
   | boost_timer_hour | boostTimerHour | 0 | 23 |
   | boost_timer_minute | boostTimerMinute | 0 | 59 |
   | RelayPlus_min | relayPlusMin | 5 | 1200 |
   | RelayPlus_max | relayPlusMax | 10 | 1380 |
   | RelayPlus_hour | relayPlusHour | 0 | 23 |
   | max_temp | maxTemp | 15 | 90 |

3. **Règle RelayPlus existante** : les deux lignes qui suivent (`relayPlusMin <= 5` et `relayPlusMax <= relayPlusMin`) sont conservées et restent après les affectations.
4. **Aucune affectation numérique directe** `variable = jsonConfig[...]` ne subsiste pour les 12 variables du tableau.
5. **Persistance** : une valeur corrigée met `shouldSaveConfig` à true (via le helper). Au boot, une config persistée fautive est donc corrigée puis réécrite, sans effacement ni mode AP.
6. **Cohérence** : `.venv/bin/python -I scripts/check_versions.py` reste à 0.

## Exigences de sécurité
- L'entrée non fiable (`/api/set?configmaxpv`, `config.json`) ne peut plus produire un diviseur nul ni une valeur hors plage pour les 12 champs.
- Aucune nouvelle route, dépendance ou ressource externe.

## Décisions prises
- Borner (clamp) plutôt que rejeter : pas de config partiellement appliquée, pas de perte Wifi au boot, correction persistée. L'API reste silencieuse sur la correction (accepté).
- Bornes identiques aux `min`/`max` d'`admin.html` (le `step` n'est pas imposé).
- Un helper unique `configBounded()` plutôt que 12 `constrain` inline, pour lever `shouldSaveConfig` en cas de correction.
- Pas de montée de version dans ce lot.

## Risques
- Le build local est impossible sur macOS arm64 (toolchain xtensa x86_64). La compilation est validée par la CI de la PR, ou via Docker `python:3.13` comme dans les livraisons précédentes.
- Comportement non testable sans matériel (pas de suite de tests) : le test fonctionnel (`/api/set` avec `mqtt_period` = 0, puis vérification que `config.json` contient 1 et qu'il n'y a pas de reboot) relève de l'utilisateur, sur l'appareil.
- Une valeur non numérique (ex. `"abc"`) vaut 0, puis elle est bornée au minimum. C'est le comportement voulu.

## Taille retenue
S : un seul fichier source (`MaxPV3/MaxPV3.ino`), aucune dépendance, surface de sécurité réduite (durcissement d'une entrée existante).
