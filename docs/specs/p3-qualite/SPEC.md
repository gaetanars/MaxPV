# SPEC : p3-qualite

## Contexte
Roadmap (`docs/ROADMAP.md`) après `p2-justesse` : la ligne ☐ ne liste plus que P3-1 et P3-7.
- P3-1 : la partie ESP (`timeScheduler()`, variables locales qui masquent `maxTemp`/`tempLimitActive`) n'existe plus depuis MaxPV! 4.00. Reste la branche inaccessible dans `EcoPV3/EcoPV3.ino:815` (doublon de la ligne 814).
- P3-7 : le Nano n'a pas de watchdog matériel, et les index sont écrits en EEPROM une fois par heure (`PVRScheduler`). Choix de conception à documenter.
- P3-6 : `NTPClient` est déjà retiré de `platformio.ini` (spec `ha-mqtt-ui`), mais la roadmap l'annonce encore comme restant.

## Objectif
Clore les derniers P3 de la roadmap (P3-1, P3-7), corriger l'état affiché de P3-6, et laisser une roadmap sans item ouvert.

## Hors périmètre
- Watchdog matériel sur le Nano ; changement de la période de sauvegarde des index (reste 1 h).
- Réécriture de la cascade de zéros de `stats_error_status`, ou tout changement de la trame série.
- Bump de version, `latest`, section « Versions » du README, tag.
- Toute modification de `MaxPV3/`.

## Exigences
Fichiers modifiables : `EcoPV3/EcoPV3.ino`, `README.md`, `docs/ROADMAP.md`.
1. P3-1 : supprimer la ligne en double de la cascade `stats_error_status` (la première occurrence reste).
2. P3-7, commentaires : un dans `setup()`, deux dans `PVRScheduler()` (choix de conception, usure EEPROM), sans changer le code.
3. P3-7, README : puce « Arduino Nano » dans la section « Watchdogs ».
4. Roadmap : P3-1, P3-6, P3-7 en ✅, puce P3-7 dans « Décisions prises » ; plus aucun `☐` ni `◐`.

## Exigences de sécurité
Aucune surface touchée.

## Décisions prises
- P3-1 : seule la ligne en double est retirée.
- P3-7 : documenté, sans watchdog matériel (risque de boucle de reset avec l'ancien bootloader, attentes longues volontaires). Période d'une heure conservée (usure EEPROM).
- Pas de bump de version ; le Nano n'a pas besoin d'être reflashé.

## Risques
Quasi nuls : code compilé identique à une branche inaccessible près (compilation `nano` en CI).

## Taille retenue
S : 3 fichiers, aucune dépendance, aucune surface de sécurité.
