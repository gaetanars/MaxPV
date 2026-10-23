# Changelog

Toutes les évolutions notables de MaxPV! sont consignées ici, au format [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/).
Trois numéros de version sont indépendants : MaxPV! (firmware du Wemos), site Web (interface embarquée) et EcoPV (firmware de l'Arduino Nano).

## [4.0.0] - 2026-10-23

Versions : MaxPV! 4.0.0, site Web 4.0.0, EcoPV 3.6.1. **Rupture avec la 3.x** : voir [la migration](docs/installation.md#migration-depuis-la-3x).

### Ajouté
- Home Assistant via MQTT d'abord : autodiscovery de 59 entités (mesures, énergie, diagnostics, modes, paramètres du routeur, réglages, boutons), topics `maxpv/<id>`, état et paramètres en JSON retain, commandes validées par une fonction unique.
- Nouvelle interface web à 5 onglets (Pico CSS, JS vanilla, sans ressource externe).
- Watchdog MQTT.
- Blueprints Home Assistant : BOOST programmé, RelayPlus, relais miroir.
- Installation initiale par une image unique `MaxPV3_full.bin` (firmware + filesystem).

### Modifié
- DHCP par défaut (IP fixe optionnelle) ; le portail Wifi ne gère plus que le Wifi ; l'activation de MQTT dépend de la seule présence d'un hôte.
- Version au format semver (`x.y.z`).
- CI durcie : actions épinglées, PlatformIO et bibliothèques en versions exactes, tests natifs.
- ElegantOTA 4.0.0 (AGPL-3.0) pour la mise à jour OTA.

### Corrigé
- Robustesse : attentes du Nano au démarrage bornées, timeout du portail Wifi, écriture atomique de la configuration, bornes des valeurs de configuration, mots de passe MQTT jamais renvoyés par l'interface.
- Justesse (EcoPV 3.6.1) : BOOST refusé si le SSR est en STOP, mode du SSR restauré en fin de BOOST, terminateurs `#` des messages `BOOSTTIME`/`RELAYPLUSTIME`, `P_CALIB` > 0, branche inaccessible retirée.
- Mutations HTTP en `POST` avec en-tête `X-MaxPV`.

### Retiré
- API HTTP `/api/*`, `/DLconfig`, historique et graphiques, index journaliers, FTP, NTP, relais distant, `logMqtt` et topics `maxpv/SYS/*`, pages web multiples.

## [3.63] - 2026-05-16

* Correction des sources script pour affichage des graphiques.
* Versions : MaxPV! 3.61, site Web 3.63, EcoPV 3.60.

## [3.62] - 2025-05-05

* Changement des échelles des jauges.
* Changement taille des champs URL relais distant.
* Versions : MaxPV! 3.61, site Web 3.62, EcoPV 3.60.

## [3.61] - 2024-02-04

* Prise en charge de la sonde de température ECS.
* Information de température disponible en MQTT.
* Versions : MaxPV! 3.61, site Web 3.60, EcoPV 3.60.

## [3.60] - 2024-02-03

* Prise en charge de la sonde de température ECS.
* Fonction BOOST limitée en température maximale.
* Versions : MaxPV! 3.60, site Web 3.60, EcoPV 3.60.

## [3.59] - 2024-01-27

* Utilisation de l'heure de Paris au lieu de l'heure solaire. Passage heure été / hiver automatique.
* Zoom dans le graphe historique.
* Correction bug mineur MQTT.
* Préparation de la prise en charge de la sonde de température ECS !! pas encore fonctionnel !!.
* Versions : MaxPV! 3.59, site Web 3.59, EcoPV 3.57.

## [3.58] - 2023-11-03

* Ajout des topics Puissance importée et Puissance exportée à MQTT.
* Versions : MaxPV! 3.58, site Web 3.57, EcoPV 3.57.

## [3.57] - 2023-06-11

* Ajout de la fonctionnalité RelayPlus.
* Correction bug affichage "année 1970" dans les graphes.
* Amélioration de la détection de la déconnexion de la charge SSR (Merci Djeje !).
* Versions : MaxPV! 3.57, site Web 3.57, EcoPV 3.57.

## [3.56] - 2023-03-12

* Ajout du graphe historique de fonctionnement du relais en % de temps de fonctionnement.
* Update API HTTP.
* Versions : MaxPV! 3.56, site Web 3.56, EcoPV 3.55.

## [3.55] - 2023-03-05

* Corrections d'un bug dans la gestion du relais secondaire.
* Possiblité de définir les modes par défaut du SSR et du relais.
* Versions : MaxPV! 3.54, site Web 3.54, EcoPV 3.55.

## [3.54] - 2023-02-18

* Amélioration du WatchDog Wifi.
* WatchDog Wifi désactivable.
* Correction affichage écran oLed.
* Correction affichage version EcoPV.
* Versions : MaxPV! 3.54, site Web 3.54, EcoPV 3.54.

## [3.53] - 2023-02-11

* Amélioration du WatchDog Wifi.
* Ajout d'une temporisation pour le changement d'état du relais distant.
* Versions : MaxPV! 3.53, site Web 3.52, EcoPV 3.51.

## [3.52] - 2023-01-29

* Ajout de l'index de comptage en temps du relais secondaire à l'autodiscovery HomeAssitant.
* Ajout mDNS pour accéder à MaxPV! par l'URL : http://maxpv.local
* Ajout du pilotage d'un relais distant par requête HTTP, en fonctionnement miroir du relais de délestage secondaire.
* Versions : MaxPV! 3.52, site Web 3.52, EcoPV 3.51.

## [3.51] - 2023-01-21

* Prise en compte de l'énergie routée en mode FORCE selon une option de compilation du programme Arduino Nano
* Amélioration de la gestion de l'écran Oled.
* Correction d'un effet de bord en cas de demande de redémarrage de l'Arduino Nano.
* Amélioration du site Web. Retour d'état des boutons en page d'accueil.
* L'absence de réponse de l'Arduino Nano n'est plus bloquante pour le démarrage du Wemos et du site Web.
* Corrections diverses.

## [3.5] - 2023-01-15

* Réorganisation du code MaxPV!. Fonctionnement à 160 MHz au lieu de 80 MHz. Optimisation de l'utilisation mémoire et de la vitesse de fonctionnement.
* Suppression de l'accès TELNET. Accès FTP possible en option de compilation.
* Informations de fonctionnement du Wemos disponibles en MQTT.
* Amélioration de l'interface Web.
* Amélioration de la gestion du mode BOOST. Gestion correcte du changement de mode SSR en cours de mode BOOST.
* Ajout d'un index de comptage du temps de fonctionnement du relais.
* Modification de la logique de comptage de l'énergie routée : L'énergie routée en mode FORCE est comptabilisée.
* Supression d'un bug mineur au re-démarrage à chaud de l'Arduino Nano.

## [3.35] - 2022-11-23

* Correction de bugs Auto-discovery MQTT Home Assistant. Intégration des compteurs d'énergie au panneau Energie de Home Assistant (Merci à zenman94). 
* Ajout d'un mode sombre au thème du site Web MaxPV! (Merci à Grubuntu).
* Améliorations mineures diverses.
* Versions : MaxPV! 3.35, site Web 3.35, EcoPV 3.31.

## [3.32] - 2022-11-11

* MQTT version complète. Support Auto-discovery MQTT Home Assistant (Merci à zenman94)
* Corrections et ajouts dans l'interface web (Merci à Grubuntu)
* Avertissement de mise à jour amélioré. Découplage des 3 MAJ.

## [3.31] - 2022-11-05

* Transmission MQTT authentifiée (optionnelle). Ajout de données transmises en MQTT
* Programmation horaire du mode BOOST
* Correction de bugs (restauration configuration)

## [3.3] - 2022-11-01

* Ajout de la transmission de données par MQTT (expérimental)
* Possibilité de sauvegarder et de restaurer la configuration MaxPV!

## [3.2] - 2022-10-28

* Ajout du mode BOOST programmable en durée et en puissance

## [3.11] - 2022-10-23

* Amélioration du comptage des impulsions externes (mesure puissance PV)
* Petites corrections dans MaxPV3

## [3.1] - 2022-08-13

* Petites corrections dans EcoPV
* Ajout du graphique historique par pas de 30 minutes sur 4 jours (96h) dans MaxPV!
* Ajout de fonctionnalités à l'API

## [3.0.2] - 2022-08-08

* Amélioration du traitement de la liaison série dans EcoPV
* Ajout d'un bargraph dans l'assistant de calibrage
* Optimisation du site Web
* Ajout de la vérification d'une mise à jour disponible sur la page d'accueil

## [3.0.1] - 2022-07-31

* Correction d'un bug mineur mode STOP SSR
* Ajout de graphiques

## [3.0] - 2022-07-30

* Version initiale
