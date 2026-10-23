# Fonctionnement

## Interface web

Page unique <http://maxpv.local/>, cinq onglets :

<!-- ![Onglet Routeur](../images/ui-routeur.png) -->

- **État** : puissances, température, index, état du relais, RSSI, uptime (rafraîchi toutes les 5 s) ; modes SSR et relais (stop, force, auto) et bouton BOOST.
- **Routeur** : les 16 paramètres EcoPV, BOOST (durée, puissance), limite de température, sauvegarde et rechargement des paramètres en EEPROM.
- **Réseau** : MQTT (hôte, port, utilisateur, mot de passe en écriture seule, période, identifiant et préfixe des topics), IP fixe optionnelle (vide = DHCP), watchdogs.
- **Étalonnage** : assistant d'étalonnage.
- **Système** : versions et vérification de mise à jour, `/update`, sauvegarde et restauration de la configuration, redémarrages, RAZ des index, formatage de l'EEPROM, effacement de la configuration ESP (chaque action destructrice demande une confirmation), crédits.

> [!CAUTION]
> L'interface et `/update` n'ont pas d'authentification : à n'utiliser que sur un réseau de confiance. Le backend HTTP (`/status`, `/params`, `/config`, `/cmd`, `/backup`) est interne et non documenté : il peut changer sans préavis. Les envois `POST` exigent l'en-tête `X-MaxPV`.

## Modes du SSR et du relais secondaire

Le mode normal est **AUTO** : le routeur dirige le surplus photovoltaïque vers la résistance du chauffe-eau (SSR) et vers la charge secondaire de délestage éventuellement connectée au relais. On peut forcer la marche (**FORCE**) ou l'arrêt (**STOP**) depuis l'onglet État ou depuis Home Assistant (selects `SSR` et `Relais`). Les modes par défaut à la mise sous tension se règlent dans `EcoPV3.ino` aux lignes 118 et 120.

Le relais secondaire fonctionne en AUTO même si le SSR ne l'est pas : le surplus correspond alors à la puissance exportée vers le réseau, et non à la puissance routée par le SSR.

## Mode BOOST

Le BOOST fait fonctionner le SSR (résistance du chauffe-eau) pour une durée et une puissance réglables (`BoostDuree` et `BoostRatio`). Pilotage de type « burst PWM » sur une période de 5 minutes.

- Une nouvelle demande pendant un BOOST actif réinitialise la durée.
- À l'arrêt du BOOST, le SSR revient au mode qu'il avait avant.
- Un BOOST est refusé si le SSR est en STOP.
- **Changer le mode du SSR pendant un BOOST l'arrête.**
- Déclenchement programmé : blueprint `boost_programme` ([Home Assistant](home-assistant.md#blueprints)).

## Limitation en température

Avec une sonde 1-Wire, la température est publiée. Si la limite est activée (switch `LimiteTemperature`) et que la température dépasse la température max, le BOOST s'arrête ; un BOOST est refusé tant que la température max est atteinte. Si la sonde est absente ou en défaut et que la limite est activée, le BOOST est refusé ou arrêté. La limite est appliquée par le Wemos : elle est inactive pendant son redémarrage.

## Watchdogs

- **WatchDog Wifi** (`watchdogwifi_active`) : ping toutes les 30 s vers le DNS1 si l'IP est fixe, sinon vers la passerelle. Si la cible reste injoignable 10 minutes, la connexion Wifi est réinitialisée.
- **Watchdog MQTT** (`watchdogmqtt_active`) : si MQTT est configuré et déconnecté depuis plus de 10 minutes, le Wemos redémarre. Avec un broker mal saisi, il redémarre donc toutes les 10 minutes (l'interface reste joignable entre deux) : désactivez ce watchdog si besoin.
- **Arduino Nano** : pas de watchdog matériel, par choix de conception (certains Nano ont un ancien bootloader qui boucle en redémarrage après un reset du watchdog, et le firmware comporte des attentes longues volontaires). Les index d'énergie sont sauvegardés en EEPROM une fois par heure : une coupure secteur peut perdre jusqu'à 1 h de comptage. Une période plus courte userait l'EEPROM. Le bouton « Sauver les index » (onglet Routeur ou Home Assistant) force une sauvegarde, par exemple avant de couper le courant.

## Allocation des pins de l'Arduino Nano

Les pins sont configurables dans `EcoPV3.ino` selon votre circuit, avec les contraintes du tableau ci-dessous. La pin D9 sert à la sonde 1-Wire depuis la 3.60.

![Pins EcoPV3](../images/Pin_allocation.png)
