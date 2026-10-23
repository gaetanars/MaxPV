# SPEC : p1-robustesse

## Contexte
Roadmap (`docs/ROADMAP.md`) après MaxPV! 4.00 : trois P1 restent ouverts côté ESP (`MaxPV3/MaxPV3.ino`).
- P1-6 : `setup()` attend le Nano par trois `while (!serialProcess()) { delay (10); }` sans timeout (`MaxPV3.ino:370,376,381`). Si le Nano se tait après le premier contact, le serveur web ne démarre jamais. En régime normal, la boucle se rattrape seule : relecture des paramètres toutes les 2 s tant que `shouldReadParams` est vrai (`:1099`), version toutes les 119 s (`:1110`).
- P1-7 : `wifiManager.autoConnect(SSID_CP)` (`:321`) n'a pas de timeout de portail. La lib retente déjà le réseau mémorisé toutes les 10 s (`setTryConnectDuringConfigPortal(true)`, `ESPAsyncWiFiManager.cpp:603-614`), mais le portail reste ouvert indéfiniment, avec l'AP « MaxPV » sans mot de passe. Avec un timeout, `autoConnect()` renvoie `false` et le setup continue aujourd'hui sans Wifi.
- P1-8 : `configWrite()` (`:607-623`) écrase `/config.json` sur place. Une coupure pendant l'écriture laisse un fichier illisible, qui est effacé au boot suivant (`:286-289`). `LittleFS::rename` s'appuie sur `lfs_rename`, qui remplace la cible de façon atomique (framework `LittleFS.h:102-111`).
- P1-1 : décision (c) statu quo, déjà documentée (`README.md:87`), mais encore affichée « décision requise » dans la roadmap.

## Objectif
Clore tous les P1 de la roadmap : corriger P1-6, P1-7 et P1-8 dans le firmware ESP, et marquer P1-1, P1-6, P1-7 et P1-8 traités dans `docs/ROADMAP.md`.

## Hors périmètre
- Bump de version, `latest`, section « Versions » du README, tag (release faite séparément).
- Mot de passe sur l'AP du portail, authentification (P1-1 reste en statu quo).
- `EcoPV3.ino` et tous les P2/P3.
- Récupération d'un `/config.tmp` orphelin au boot (le `rename` est atomique, le fichier temporaire est écrasé à l'écriture suivante).

## Exigences
Fichiers sources modifiables : `MaxPV3/MaxPV3.ino`, `MaxPV3/maxpv_defines.h`. Documentation : `docs/ROADMAP.md`.

1. **P1-6** : nouvelle constante `NANO_BOOT_TIMEOUT` (2000 ms) dans `maxpv_defines.h`. Chacune des trois attentes du setup (après `getAllParamEcoPV()`, après `getVersionEcoPV()`, puis l'attente des STATS) est bornée à `NANO_BOOT_TIMEOUT` ms. Le boot continue après un timeout, sans autre traitement : la boucle principale relit les paramètres et la version. Le code ne contient plus de `while (!serialProcess())`.
2. **P1-7** : nouvelle constante `WIFI_PORTAL_TIMEOUT` (300 s) dans `maxpv_defines.h`. Avant `autoConnect`, si un réseau est mémorisé (`WiFi.SSID()` non vide), `wifiManager.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT)` est appelé. Sans réseau mémorisé (première installation), le portail reste sans limite, comme aujourd'hui. Si `autoConnect` renvoie `false`, l'ESP logge l'échec (`logSerial`) puis appelle `ESP.restart()`.
3. **P1-8** : `configWrite()` sérialise dans `/config.tmp`, puis appelle `LittleFS.rename("/config.tmp", "/config.json")`. Si l'ouverture du fichier temporaire échoue, ou si `serializeJson` renvoie 0, le fichier temporaire est supprimé, l'échec est loggé et `/config.json` n'est pas touché. Le comportement existant en cas de débordement (`overflowed()`, pas d'écriture) est conservé.
4. **Roadmap** : le tableau « Avancement » de `docs/ROADMAP.md` gagne une ligne ✅ pour P1-1 (décision (c), documentée `README.md:87`) et une ligne ✅ pour P1-6, P1-7 et P1-8 (spec `p1-robustesse`, test sur matériel à faire). La ligne ☐ ne liste plus que P2-1, P2-2, P3-1 et P3-7. Les titres des sections P1-1, P1-6, P1-7 et P1-8 portent ✅, avec le texte d'origine conservé.

## Exigences de sécurité
- P1-7 réduit la durée d'exposition de l'AP ouvert (300 s, puis redémarrage et nouvelle tentative du réseau mémorisé). Aucune nouvelle entrée non fiable, aucun secret ajouté.
- P1-8 : aucune perte de `config.json` (donc ni du mot de passe MQTT, ni de l'IP fixe) sur une coupure pendant l'écriture.

## Décisions prises
- P1-7 : timeout de 300 s et `ESP.restart()`, appliqués seulement si un réseau est mémorisé. Pas de mot de passe sur l'AP.
- P1-6 : 2 s par attente, sans relance active (la boucle s'en charge).
- P1-8 : fichier temporaire puis `rename` atomique, sans récupération au boot.
- P1-1 : clos par la décision (c), sans changement de code.
- Pas de bump de version dans cette spec.

## Risques
- `WiFi.SSID()` avant `autoConnect` : il doit renvoyer le SSID mémorisé par le SDK. Si ce n'est pas le cas (chaîne vide), le comportement reste celui d'aujourd'hui (portail sans limite), sans régression.
- Un utilisateur qui reconfigure le Wifi alors qu'un réseau est mémorisé est interrompu au bout de 5 min (le portail se rouvre après le redémarrage).
- Validation limitée à la compilation : les trois scénarios (Nano muet, box coupée, coupure pendant une sauvegarde) restent à tester sur matériel.

## Taille retenue
S : 3 fichiers (`MaxPV3.ino`, `maxpv_defines.h`, `docs/ROADMAP.md`), aucune dépendance nouvelle, surface de sécurité réduite (pas étendue).
