/*
  This file is a part of MaxPV.ino - ESP8266 program that provides a web interface and a API for EcoPV
  Copyright (C) 2022 - Bernard Legrand.
  maxpv@bernard-legrand.net
  https://github.com/Jetblack31/

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation, either version 2.1 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

// ***********************************************************************************
// ****************************   Définitions générales   ****************************
// ***********************************************************************************

// Version MaxPV!
#define MAXPV_VERSION      "4.0.0"
#define MAXPV_VERSION_FULL "MaxPV! 4.0.0"

// SSID pour le Config Portal
#define SSID_CP            "MaxPV"

// Communication série de débug pendant la phase initiale de démarrage
#define SERIAL_LOG_BAUD   115200 // Vitesse de la liaison port série pour les logs de démarrage
#define SERIAL_LOG_ENABLE false   // Log sur liaison série : true ou false

// Communication série entre l'arduino et l'ESP 8266
#define SERIAL_BAUD     500000 // Vitesse de la liaison port série pour la connexion avec l'arduino
#define SERIALTIMEOUT   100    // Timeout pour les interrogations sur liaison série en ms
#define NANO_BOOT_TIMEOUT 2000 // Délai max en ms de chaque attente du Nano au démarrage
#define WIFI_PORTAL_TIMEOUT 300 // Durée max en s du portail Wifi si un réseau est mémorisé
#define SERIAL_BUFFER   256    // Taille du buffer RX pour la connexion avec l'arduino (256 max)
#define END_OF_TRANSMIT '#'    // Caractère de fin de transmission de l'Arduino

// Communications TCP, ports
#define MAXPV_MDNS      "maxpv"         // mDNS pour accès local
#define HTTP_PORT       80              // Port serveur web MaxPV! (fixe)

// Taille de la configuration JSON MaxPV! pour la manipulation
#define JSON_CONFIG_SIZE    1024        // en caractères

// Valeurs par défaut de la configuration réseau : vides = DHCP
#define DEFAULT_IP          ""
#define DEFAULT_GW          ""
#define DEFAULT_SN          ""
#define DEFAULT_DNS1        ""
#define DEFAULT_DNS2        ""

#define PING_WIFI_TIMEOUT    600000     // Délai en ms où on considère un problème de
                                        // connexion wifi qui active le watchdog wifi
#define MQTT_WATCHDOG_TIMEOUT 600000    // Délai en ms de déconnexion MQTT avant redémarrage
                                        // si le watchdog MQTT est actif

#define WELCOME_MESSAGE     "MaxPV! par Bernard Legrand (2022)"

#define DEFAULT_MQTT_SERVER     ""              // Hôte MQTT (IP ou nom DNS) ; vide = MQTT inactif
#define DEFAULT_MQTT_PORT       1883            // Port serveur MQTT
#define RECONNECT_TIME          5               // Délai de reconnexion en secondes suite à perte de connexion du serveur mqtt

#define DEFAUT_EMPTY_USER  ""
#define DEFAUT_EMPTY_PWD   ""
#define MQTT_HOST_LENGTH   64   // Longueurs maximales des champs de configuration MQTT
#define DEFAUT_USER_LENGTH 39
#define DEFAUT_PWD_LENGTH  39

// Mode BOOST
#define DEFAULT_BOOST_RATIO      100 // Ratio des burst 0..100
#define DEFAULT_BOOST_DURATION   120 // Durée mode boost en minutes

#define DEFAULT_MAX_TEMP          45


// définition de l'ordre des paramètres de configuration de EcoPV tels que transmis
// et de l'index de rangement dans le tableau de stockage (début à la position 1)
// on utilise la position 0 pour stocker la version
#define NB_PARAM      17    // Nombre de paramètres transmis par EcoPV (17 = 16 + VERSION)
#define ECOPV_VERSION 0
#define V_CALIB       1
#define P_CALIB       2
#define PHASE_CALIB   3
#define P_OFFSET      4
#define P_RESISTANCE  5
#define P_MARGIN      6
#define GAIN_P        7
#define GAIN_I        8
#define E_RESERVE     9
#define P_DIV2_ACTIVE 10
#define P_DIV2_IDLE   11
#define T_DIV2_ON     12
#define T_DIV2_OFF    13
#define T_DIV2_TC     14
#define CNT_CALIB     15
#define P_INSTALLPV   16


// définition de l'ordre des informations statistiques transmises par EcoPV
// et de l'index de rangement dans le tableau de stockage (début à la position 1)
// on utilise la position 0 pour stocker la version
#define NB_STATS        25     // Nombre d'informations statistiques transmis par EcoPV (25 = 24 + VERSION)
//#define ECOPV_VERSION 0
#define V_RMS           1
#define I_RMS           2
#define P_ACT           3
#define P_APP           4
#define P_ROUTED        5
#define P_IMP           6
#define P_EXP           7
#define COS_PHI         8
#define INDEX_ROUTED    9
#define INDEX_IMPORT    10
#define INDEX_EXPORT    11
#define INDEX_IMPULSION 12
#define P_IMPULSION     13
#define TRIAC_MODE      14
#define RELAY_MODE      15
#define DELAY_MIN       16
#define DELAY_AVG       17
#define DELAY_MAX       18
#define BIAS_OFFSET     19
#define STATUS_BYTE     20
#define ONTIME          21
#define SAMPLES         22
#define INDEX_RELAY     23
#define TEMP_ECS        24


// Publication MQTT : topics sous maxpv/<id> (voir maxpv_mqtt.h)
#define DEFAULT_MQTT_PUBLISH_PERIOD   10     // en secondes, intervalle de publication MQTT
