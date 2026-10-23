Vous aimez ce projet ? Vous pouvez m'encourager grâce au bouton **Sponsor** dans la barre ci-dessus !
Merci à tous pour vos messages et votre soutien au projet !

# MaxPV!
MaxPV! est une nouvelle interface pour EcoPV, compatible avec les montages EcoPV basés sur l'Arduino Nano et sur le Wemos ESP8266 pour la liaison Wifi. MaxPV! s'intègre d'abord à Home Assistant via MQTT (autodiscovery complète) et apporte une interface Web de configuration et de dépannage.

MaxPV! hérite d'EcoPV et de son algorithme de routage. Il sera utile de se référer au dépôt EcoPV : https://github.com/Jetblack31/EcoPV

La lecture de ces fils de discussion est plus que recommandée pour la mise en oeuvre :  
* Forum photovoltaïque, discussion sur MaxPV : https://forum-photovoltaique.fr/viewtopic.php?f=110&t=55244 
* Forum photovoltaïque, discussion sur EcoPV : https://forum-photovoltaique.fr/viewtopic.php?f=110&t=42721  
* Forum photovoltaïque, réalisation d'un PCB : https://forum-photovoltaique.fr/viewtopic.php?f=110&t=42874  
* Forum photovoltaïque, montage du PCB : https://forum-photovoltaique.fr/viewtopic.php?f=110&t=43197  

## Mise en garde  
Les programmes et les schémas proposés ont une vocation informative et pédagogique. Ils ont été testés avec succès par les auteurs. Cependant les auteurs de ces programmes et de ces schémas déclinent toute responsabilité. Les auteurs ne pourraient être tenus pour responsables du fonctionnement et des conséquences de l'utilisation des programmes et des schémas mis à disposition.  
Intervenir sur des circuits électriques est dangereux et nécessite le recours à une personne qualifiée et le respect strict des normes de sécurité et de protection en vigueur.

## Caractéristiques de MaxPV!
* Même qualité de routage du surplus photovoltaïque que EcoPV.
* Home Assistant d'abord : MQTT avec autodiscovery complète (mesures, modes, paramètres du routeur, réglages, diagnostics, boutons), 59 entités.
* Interface web à 5 onglets (Pico CSS, sans ressource externe) pour configurer et dépanner.
* Installation et paramétrage réseau facilités (DHCP par défaut, IP fixe optionnelle).
* Mise à jour du Wemos en OTA.
* Assistant d'étalonnage des paramètres du routeur.
* Le SSR et le relais secondaire de délestage peuvent être forcés sur arrêt permanent, marche permanente, en plus du mode automatique (par défaut).
* Mode Boost sur SSR pour complément de chauffe ; limite de température avec une sonde 1-Wire.
* WatchDog Wifi et watchdog MQTT.
* Blueprints Home Assistant : BOOST programmé, RelayPlus (durées min et max du relais) et relais miroir.

## Synoptique
![MaxPV! synoptique](images/synoptique.png)

# Schémas pour la réalisation du circuit électronique
Les schémas électroniques et la liste des composants nécessaires à la réalisation de la carte MaxPV! sont disponibles [ici](SchemasMaxPV/). 

# Installation des firmwares
## Pré-requis
L'installation de MaxPV! se fait par programmation de l'Arduino Nano et du Wemos/ESP8266 par USB. Vous aurez besoin de l'IDE Arduino pour le Nano (version conseillée : IDE Arduino 1.8.x). Pour le Wemos : un câble USB, et soit Chrome/Edge (page web esptool-js, rien à installer), soit esptool (Python).
Le fonctionnement de MaxPV! nécessite une connexion à votre réseau local en Wifi (l'adresse IP est attribuée par DHCP par défaut). En cours d'installation, vous aurez besoin de vous connecter temporairement en Wifi au Wemos à l'aide d'un ordinateur portable ou d'un téléphone.

## Programmation de l'Arduino Nano
* **ATTENTION** : si vous migrez un système EcoPV existant, prenez note des paramètres du routeur ! Ceux-ci seront effacés et devront être ré-introduits à la fin de l'installation !
* Ouvrez le programme *EcoPV3.ino* dans l'IDE de l'Arduino configuré pour la programmation de l'Arduino Nano.
* Installez le cas échéant les bibliothèques OneWire.h et DallasTemperature.
* Si vous utilisez l'écran oLed, dé-commentez la ligne 47 du code et vérifiez que la bibliothèque SSD1306Ascii est bien installée.
* Si vous voulez que l'énergie délivrée en mode FORCE et en mode BOOST ne soit pas prise en compte dans l'index d'énergie routée, modifiez la ligne 65.
* Téléchargez le programme dans l'Arduino Nano.

## Programmation du Wemos
* Téléchargez l'image complète *MaxPV3_full.bin* (firmware + filesystem) : https://github.com/gaetanars/MaxPV/releases/latest/download/MaxPV3_full.bin
* Branchez le Wemos en USB (installez le pilote CH340 si le port n'apparaît pas).
* Ouvrez https://espressif.github.io/esptool-js/ dans Chrome ou Edge, cliquez sur **Connect** et choisissez le port du Wemos, puis **Erase Flash**. Indiquez ensuite **Flash Address** `0x0` avec le fichier *MaxPV3_full.bin*, puis cliquez sur **Program**.
* Alternative en ligne de commande avec [esptool](https://github.com/espressif/esptool) (version 5.x) : `esptool --chip esp8266 write-flash --erase-all 0x0 MaxPV3_full.bin`
* Redémarrez le Wemos, puis connectez-vous au réseau Wifi **MaxPV** qu'il crée. Un portail captif s'ouvre, s'il ne s'ouvre pas automatiquement, connectez-vous à l'adresse http://192.168.4.1

![MaxPV! captif portal](images/captif.png)
* Choisissez votre réseau Wifi et saisissez son mot de passe : le portail ne gère que le Wifi. Le réseau (DHCP par défaut, IP fixe optionnelle) et MQTT se règlent ensuite dans l'interface web.
* Vous pouvez maintenant ré-installer l'Arduino Nano et le Wemos sur la carte électronique de votre routeur.
## Premier démarrage
* Vérifiez que votre ordinateur / téléphone est bien connecté à votre réseau local.
* Ouvrez http://maxpv.local/ (ou l'adresse IP du Wemos). L'en-tête de la page indique l'état du Nano (routeur), de MQTT et la version.
* Onglet **Étalonnage** : l'assistant vous guide si vous êtes nouvel utilisateur. Sinon, saisissez vos paramètres un par un dans l'onglet **Routeur**.
* **Attention** : 2 paramètres sont apparus avec la version 3.5 : **P_INSTALLPV** (puissance de l'installation en Wc) et **CNT_CALIB** (poids en Wh des impulsions du compteur de production).
* Onglet **Réseau** : saisissez l'hôte du broker MQTT (laissez vide pour désactiver MQTT), puis enregistrez : le Wemos redémarre.
* Dans Home Assistant, l'appareil MaxPV apparaît via l'autodiscovery MQTT. Utilisez le bouton « SauverParametres » (ou l'onglet Routeur) pour enregistrer les paramètres du routeur dans l'EEPROM du Nano.

## Mises à jour
Les mises à jour de l'Arduino se font par USB comme décrit ci-dessus.

Les mises à jour de MaxPV! sur le Wemos se font par Wifi en OTA via le lien **Update** (`/update`) de l'onglet Système. **Attention** : la mise à jour du filesystem nécessite de reconfigurer la connexion Wifi du Wemos (réseau Wifi MaxPV, portail à l'adresse 192.168.4.1). Faites une sauvegarde de la configuration (onglet Système) avant.

Pour les mises à jour, **l'ordre à suivre est** : firmware puis filesystem. En cas d'échec, utilisez la procédure « Programmation du Wemos » (elle efface toute la configuration du Wemos).

## Installation avancée
Si vous souhaitez compiler le firmware et le filesystem du Wemos pour réaliser une installation personnalisée, les codes sources sont disponibles dans le répertoire **"MaxPV3"**.

# Tech zone
## Interface web
Page unique `http://maxpv.local/`, cinq onglets :
* **État** : puissances, température, index, état du relais, RSSI, uptime (rafraîchi toutes les 5 s) ; modes SSR et relais (stop, force, auto) et bouton BOOST.
* **Routeur** : les 16 paramètres EcoPV, BOOST (durée, puissance), limite de température, sauvegarde et rechargement des paramètres en EEPROM.
* **Réseau** : MQTT (hôte, port, utilisateur, mot de passe en écriture seule, période, identifiant et préfixe des topics), IP fixe optionnelle (vide = DHCP), watchdogs.
* **Étalonnage** : assistant d'étalonnage.
* **Système** : versions et vérification de mise à jour, `/update`, sauvegarde et restauration de la configuration, redémarrages, RAZ des index, formatage de l'EEPROM, effacement de la configuration ESP (chaque action destructrice demande une confirmation), crédits.

L'interface et `/update` n'ont pas d'authentification : à n'utiliser que sur un réseau de confiance. Le backend HTTP (`/status`, `/params`, `/config`, `/cmd`, `/backup`) est interne et non documenté : il peut changer sans préavis. Les envois `POST` exigent l'en-tête `X-MaxPV`.

## Explication du mode de fonctionnement des sorties SSR et du relais secondaire
Le mode de fonctionnement normal des sorties SSR et relais secondaire est le **mode AUTO**. Dans ce mode, le routeur dirige le surplus de production photovoltaïque vers la résistance du chauffe-eau et la charge secondaire de délestage éventuellement connectée au relais.
Vous pouvez forcer la marche du SSR et/ou du relais (mode FORCE) ou l'empêcher (mode STOP), depuis l'onglet État ou depuis Home Assistant (selects « SSR » et « Relais »).
Le mode par défaut des sorties à la mise sous tension peut être modifié dans le fichier EcoPV3.ino en lignes 118 et 120.

Le relais secondaire fonctionne en mode AUTO même si le SSR n'est pas en mode AUTO : le surplus PV correspond alors à la puissance exportée vers le réseau, et non à la puissance routée par le SSR.

## Mode BOOST
Le mode BOOST déclenche le fonctionnement du SSR (résistance du chauffe-eau) pour une durée et avec une puissance réglables (entités BoostDuree et BoostRatio, onglet Routeur). Une nouvelle demande pendant un BOOST actif ré-initialise la durée. À l'arrêt du BOOST, le SSR revient au mode qu'il avait avant le BOOST. Un BOOST est refusé si le SSR est en STOP. Le pilotage est de type 'burst PWM' sur une période de 5 minutes.
**ATTENTION** : la modification du mode du SSR est prioritaire sur le BOOST et l'arrête.
Pour un déclenchement programmé, utilisez le blueprint `boost_programme` (voir ci-dessous).

## Limitation en température
Si une sonde 1-Wire est installée, la température est publiée. Si la limite est activée (switch `LimiteTemperature`) et que la température dépasse la température max, le BOOST s'arrête ; un BOOST est refusé tant que la température max est atteinte. Si la sonde est absente ou en défaut et que la limite est activée, le BOOST est refusé ou arrêté. Cette limite est appliquée par le Wemos : elle est inactive pendant son redémarrage.

## Home Assistant
MaxPV! publie en MQTT avec autodiscovery (préfixe `homeassistant`). `<id>` = `maxpv` + identifiant de la puce ESP (visible dans l'onglet Réseau) ; base des topics : `maxpv/<id>`.

* `maxpv/<id>/status` : disponibilité (`online`/`offline`, retain, LWT).
* `maxpv/<id>/state` : JSON retain de l'état (mesures, index, modes, états, diagnostics), publié toutes les `mqtt_period` secondes et après chaque commande de mode ou de BOOST.
* `maxpv/<id>/params` : JSON retain des 16 paramètres EcoPV et des réglages ESP.
* `maxpv/<id>/<clé>/set` : commandes (`triac_mode`, `relay_mode` : `stop`/`force`/`auto` ; `boost` : `ON`/`OFF` ; une clé par paramètre ; boutons `restartesp`, `resetecopv`, `savecfg`, `loadcfg`, `saveindx`, `indx0` avec le payload `PRESS`). Une valeur hors bornes est rejetée. Le formatage de l'EEPROM et l'effacement de la configuration ne sont jamais acceptés en MQTT.

Entités (59) : capteurs de mesure, d'énergie et de diagnostic (certains désactivés par défaut), états `EtatRelais` et `ContactRouteur`, selects `SSR` et `Relais`, switchs `Boost` et `LimiteTemperature`, 20 numbers (16 paramètres EcoPV, `BoostDuree`, `BoostRatio`, `TempMax`, `PeriodeMQTT`), boutons `RedemarrerESP`, `RedemarrerRouteur`, `SauverParametres`, `RechargerParametres`, `SauverIndex`, `RAZIndex` (désactivé par défaut). Les 18 identifiants uniques historiques sont conservés : Home Assistant garde les entités et leur historique.

Les paramètres modifiés depuis Home Assistant sont appliqués immédiatement au Nano mais ne sont gardés après une coupure qu'après avoir pressé **SauverParametres**.

Les index journaliers et l'historique ne sont plus calculés par le Wemos : utilisez des `utility_meter` sur les capteurs d'énergie, et le dashboard Énergie de Home Assistant.

### Blueprints
Dans `blueprints/automation/maxpv/` : `boost_programme.yaml` (BOOST à heure fixe, jours au choix), `relayplus.yaml` (durées min et max du relais, avec un capteur `history_stats` décrit dans le blueprint), `relais_miroir.yaml` (recopie de l'état du relais sur un interrupteur distant). Import : Paramètres > Automatisations > Blueprints > Importer un blueprint, avec l'URL du fichier sur GitHub (https://github.com/gaetanars/MaxPV/blob/main/blueprints/automation/maxpv/boost_programme.yaml, idem pour les autres).

## Watchdogs
* **WatchDog Wifi** (`watchdogwifi_active`) : le Wemos envoie un ping toutes les 30 s, vers le DNS1 si l'IP est fixe, sinon vers la passerelle. Si la cible reste injoignable pendant 10 minutes, la connexion Wifi est ré-initialisée.
* **Watchdog MQTT** (`watchdogmqtt_active`) : si MQTT est configuré et déconnecté depuis plus de 10 minutes, le Wemos redémarre. Avec un broker mal saisi, il redémarre donc toutes les 10 minutes (l'interface reste joignable entre deux) : désactivez ce watchdog si besoin.

## Migration depuis la 3.x
La 4.00 est une rupture. Retirés : historique et graphiques, index journaliers, BOOST programmé, RelayPlus (blueprints HA), relais distant (blueprint `relais_miroir`), FTP, NTP, `logMqtt`/`maxpv/SYS/*`, API `/api/*` et `/DLconfig` (le PDF n'existe plus), pages web multiples.
* Configuration : l'IP fixe d'une config 3.x est conservée (si la passerelle, le masque ou le DNS1 sont invalides : DHCP). Les clés inconnues sont ignorées. L'activation de MQTT dépend désormais uniquement de la présence d'un hôte MQTT. Le port HTTP est fixe (80).
* Les anciens topics `maxpv/vrms` … `maxpv/temperature`, `maxpv/state` et `maxpv/SYS/*` ne sont plus publiés mais leurs messages retain restent sur le broker. Pour les purger : `mosquitto_pub -r -n -t maxpv/vrms` (idem pour chaque topic, y compris `maxpv/SYS/<sous-topic>`).
* Reflasher le firmware puis le filesystem (ordre dans « Mises à jour ») ou utiliser `MaxPV3_full.bin`.


## Allocation des pins de l'Arduino Nano
Les pins d'entrée-sortie de l'Arduino Nano sont configurables dans le code EcoPV3 en fonction du développement de votre circuit électronique. Il y a toutefois certaines contraintes résumées dans le tableau ci-dessous.

![Pins EcoPV3](images/Pin_allocation.png)

La pin D9 est utilisé pour la sonde de température 1-Wire depuis la version 3.60.

## Licence
* Le code de MaxPV! est distribué sous licence LGPL 2.1+ ; il est issu de [Jetblack31/MaxPV](https://github.com/Jetblack31/MaxPV) (Bernard Legrand / Jetblack31), dont l'attribution est conservée.
* Le firmware du Wemos embarque ElegantOTA (AGPL-3.0). Les binaires de firmware distribués sont de ce fait soumis à l'AGPL-3.0.
* L'interface embarque [Pico CSS](https://picocss.com) 2.1.1 (MIT), copie exacte de `@picocss/pico@2.1.1/css/pico.min.css`. SHA-256 de `MaxPV3/data/pico.min.css` : `fbc9a63fc9fc9f72d12fd7fc9806e11fa9f77ae4f9cad146b27003a1119ba3db`.

# Versions
### **V 4.00** - 23/10/2026
* Home Assistant via MQTT d'abord : autodiscovery de 59 entités, topics `maxpv/<id>`, état et paramètres en JSON, commandes validées par une fonction unique.
* Nouvelle interface web à 5 onglets (Pico CSS, sans ressource externe).
* Watchdog MQTT ; DHCP par défaut ; portail Wifi limité au Wifi.
* Blueprints HA : BOOST programmé, RelayPlus, relais miroir.
* Retraits : API, historique, index journaliers, FTP, NTP, relais distant. Voir « Migration depuis la 3.x ».
* Versions : MaxPV! 4.00, site Web 4.00, EcoPV 3.60.
### **V 3.63** - 16/05/2026
* Correction des sources script pour affichage des graphiques.
* Versions : MaxPV! 3.61, site Web 3.63, EcoPV 3.60.
### **V 3.62** - 05/05/2025
* Changement des échelles des jauges.
* Changement taille des champs URL relais distant.
* Versions : MaxPV! 3.61, site Web 3.62, EcoPV 3.60.
### **V 3.61** - 04/02/2024
* Prise en charge de la sonde de température ECS.
* Information de température disponible en MQTT.
* Versions : MaxPV! 3.61, site Web 3.60, EcoPV 3.60.
### **V 3.60** - 03/02/2024
* Prise en charge de la sonde de température ECS.
* Fonction BOOST limitée en température maximale.
* Versions : MaxPV! 3.60, site Web 3.60, EcoPV 3.60.
### **V 3.59** - 27/01/2024
* Utilisation de l'heure de Paris au lieu de l'heure solaire. Passage heure été / hiver automatique.
* Zoom dans le graphe historique.
* Correction bug mineur MQTT.
* Préparation de la prise en charge de la sonde de température ECS !! pas encore fonctionnel !!.
* Versions : MaxPV! 3.59, site Web 3.59, EcoPV 3.57.
### **V 3.58** - 03/11/2023
* Ajout des topics Puissance importée et Puissance exportée à MQTT.
* Versions : MaxPV! 3.58, site Web 3.57, EcoPV 3.57.
### **V 3.57** - 11/06/2023
* Ajout de la fonctionnalité RelayPlus.
* Correction bug affichage "année 1970" dans les graphes.
* Amélioration de la détection de la déconnexion de la charge SSR (Merci Djeje !).
* Versions : MaxPV! 3.57, site Web 3.57, EcoPV 3.57.
### **V 3.56** - 12/03/2023
* Ajout du graphe historique de fonctionnement du relais en % de temps de fonctionnement.
* Update API HTTP.
* Versions : MaxPV! 3.56, site Web 3.56, EcoPV 3.55.
### **V 3.55** - 05/03/2023
* Corrections d'un bug dans la gestion du relais secondaire.
* Possiblité de définir les modes par défaut du SSR et du relais.
* Versions : MaxPV! 3.54, site Web 3.54, EcoPV 3.55.
### **V 3.54** - 18/02/2023
* Amélioration du WatchDog Wifi.
* WatchDog Wifi désactivable.
* Correction affichage écran oLed.
* Correction affichage version EcoPV.
* Versions : MaxPV! 3.54, site Web 3.54, EcoPV 3.54.
### **V 3.53** - 11/02/2023
* Amélioration du WatchDog Wifi.
* Ajout d'une temporisation pour le changement d'état du relais distant.
* Versions : MaxPV! 3.53, site Web 3.52, EcoPV 3.51.
### **V 3.52** - 29/01/2023
* Ajout de l'index de comptage en temps du relais secondaire à l'autodiscovery HomeAssitant.
* Ajout mDNS pour accéder à MaxPV! par l'URL : http://maxpv.local
* Ajout du pilotage d'un relais distant par requête HTTP, en fonctionnement miroir du relais de délestage secondaire.
* Versions : MaxPV! 3.52, site Web 3.52, EcoPV 3.51.
### **V 3.51** - 21/01/2023
* Prise en compte de l'énergie routée en mode FORCE selon une option de compilation du programme Arduino Nano
* Amélioration de la gestion de l'écran Oled.
* Correction d'un effet de bord en cas de demande de redémarrage de l'Arduino Nano.
* Amélioration du site Web. Retour d'état des boutons en page d'accueil.
* L'absence de réponse de l'Arduino Nano n'est plus bloquante pour le démarrage du Wemos et du site Web.
* Corrections diverses.
### **V 3.5** - 15/01/2023
* Réorganisation du code MaxPV!. Fonctionnement à 160 MHz au lieu de 80 MHz. Optimisation de l'utilisation mémoire et de la vitesse de fonctionnement.
* Suppression de l'accès TELNET. Accès FTP possible en option de compilation.
* Informations de fonctionnement du Wemos disponibles en MQTT.
* Amélioration de l'interface Web.
* Amélioration de la gestion du mode BOOST. Gestion correcte du changement de mode SSR en cours de mode BOOST.
* Ajout d'un index de comptage du temps de fonctionnement du relais.
* Modification de la logique de comptage de l'énergie routée : L'énergie routée en mode FORCE est comptabilisée.
* Supression d'un bug mineur au re-démarrage à chaud de l'Arduino Nano.
### **V 3.35** - 23/11/2022
* Correction de bugs Auto-discovery MQTT Home Assistant. Intégration des compteurs d'énergie au panneau Energie de Home Assistant (Merci à zenman94). 
* Ajout d'un mode sombre au thème du site Web MaxPV! (Merci à Grubuntu).
* Améliorations mineures diverses.
* Versions : MaxPV! 3.35, site Web 3.35, EcoPV 3.31.
### **V 3.32** - 11/11/2022
* MQTT version complète. Support Auto-discovery MQTT Home Assistant (Merci à zenman94)
* Corrections et ajouts dans l'interface web (Merci à Grubuntu)
* Avertissement de mise à jour amélioré. Découplage des 3 MAJ.
### **V 3.31** - 05/11/2022
* Transmission MQTT authentifiée (optionnelle). Ajout de données transmises en MQTT
* Programmation horaire du mode BOOST
* Correction de bugs (restauration configuration)
### **V 3.3** - 01/11/2022
* Ajout de la transmission de données par MQTT (expérimental)
* Possibilité de sauvegarder et de restaurer la configuration MaxPV!
### **V 3.2** - 28/10/2022
* Ajout du mode BOOST programmable en durée et en puissance
### **V 3.11** - 23/10/2022
* Amélioration du comptage des impulsions externes (mesure puissance PV)
* Petites corrections dans MaxPV3
### **V 3.1** - 13/08/2022
* Petites corrections dans EcoPV
* Ajout du graphique historique par pas de 30 minutes sur 4 jours (96h) dans MaxPV!
* Ajout de fonctionnalités à l'API
### **V 3.0.2** - 08/08/2022
* Amélioration du traitement de la liaison série dans EcoPV
* Ajout d'un bargraph dans l'assistant de calibrage
* Optimisation du site Web
* Ajout de la vérification d'une mise à jour disponible sur la page d'accueil
### **V 3.0.1** - 31/07/2022
* Correction d'un bug mineur mode STOP SSR
* Ajout de graphiques
### **V 3.0**   - 30/07/2022
* Version initiale



