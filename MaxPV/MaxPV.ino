/*
  MaxPV.ino - ESP8266 program that provides a web interface and a API for EcoPV
  Copyright (C) 2022 - Bernard Legrand 
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

/*************************************************************************************
**                                                                                  **
**        Ce programme fonctionne sur ESP8266 de type Wemos avec 4 Mo de mémoire    **
**        La compilation s'effectue avec l'IDE Arduino                              **
**        Site Arduino : https://www.arduino.cc                                     **
**                                                                                  **
**************************************************************************************/

// ***********************************************************************************
// ******************             OPTIONS DE COMPILATION               ***************
// ***********************************************************************************

// Importation des options de compilation
#include "maxpv_options.h"

// ***********************************************************************************
// ******************         FIN DES OPTIONS DE COMPILATION           ***************
// ***********************************************************************************


// ***********************************************************************************
// ******************                   LIBRAIRIES                     ***************
// ***********************************************************************************

// ***      ATTENTION : NE PAS ACTIVER LE DEBUG SERIAL SUR AUCUNE LIBRAIRIE        ***

#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <AsyncMqttClient.h>
#include <ElegantOTA.h>
#include <Ticker.h>
#include <TickerScheduler.h>
#include <ArduinoJson.h>
#include "AsyncPing.h"

// ***      ATTENTION : NE PAS ACTIVER LE DEBUG SERIAL SUR AUCUNE LIBRAIRIE        ***

// ***********************************************************************************
// ******************               FIN DES LIBRAIRIES                 ***************
// ***********************************************************************************

// ***********************************************************************************
// ************************    DEFINITIONS ET DECLARATIONS     ***********************
// ***********************************************************************************

// Importation des définitions de configuration générale de MaxPV!
#include "maxpv_defines.h"
#include "maxpv_validate.h"
#include "maxpv_mqtt.h"

// Définitions utiles pour le programme
#define OFF         0
#define ON          1
#define STOP        0
#define FORCE       1
#define AUTOM       9


// ***********************************************************************************
// ************************         VARIABLES GLOBALES         ***********************
// ***********************************************************************************

// ***********************************************************************************
// ******************* Variables globales de configuration MaxPV! ********************
// ******************* et configuration par défaut                ********************
// ***********************************************************************************

// NOTE : L'INITIALISATION DES STRINGs SE FAIT DANS SETUP()

// Configuration IP statique pour le mode STA. static_ip vide = DHCP
String static_ip;
String static_gw;
String static_sn;
String static_dns1;
String static_dns2;
bool   staticIpActive = false;                    // IP fixe valide et appliquée

int wtdWifiActive    = ON;                        // WatchDog Wifi actif (= ON) ou non (= OFF)
int wtdMqttActive    = ON;                        // WatchDog MQTT actif (= ON) ou non (= OFF)

// Définition des paramètres du mode BOOST
byte boostRatio      = DEFAULT_BOOST_RATIO;       // En % de la puissance max
int boostDuration    = DEFAULT_BOOST_DURATION;    // En minutes

// Configuration de MQTT : actif si et seulement si mqttIP n'est pas vide
String mqttIP;                                      // Hôte du serveur MQTT (IP ou nom DNS)
String mqttUser;                                    // Utilisateur du serveur MQTT - Optionnel : si vide, pas d'authentification
String mqttPass;                                    // Mot de passe du serveur MQTT
uint16_t mqttPort   = DEFAULT_MQTT_PORT;            // Port du serveur MQTT
int mqttPeriod      = DEFAULT_MQTT_PUBLISH_PERIOD;  // Période de transmission en secondes

// Définition des paramètres de contrôle de température
int maxTemp         = DEFAULT_MAX_TEMP;
int tempLimitActive = OFF;


// ***********************************************************************************
// *************** Déclaration des variables globales de fonctionnement **************
// ***********************************************************************************

// Stockage des informations en provenance de EcoPV - Arduino Nano
String ecoPVConfig[NB_PARAM];
String ecoPVStats[NB_STATS];

// Définition des tâches de Ticker
TickerScheduler ts(1);
Ticker mqttReconnectTimer;

// Compteur général à la seconde
unsigned long generalCounterSecond = 0;

// Flag indiquant la nécessité de rebooter MaxPV!
bool shouldReboot = false;
// Flag indiquant la nécessité de sauvegarder la configuration de MaxPV!
bool shouldSaveConfig = false;
// Flag indiquant la nécessité de lire les paramètres de routage EcoPV
bool shouldReadParams = false;
// Flag indiquant la nécessité de vérifier la connexion effective au Wifi
bool shouldCheckWifi = false;
// Flag indiquant la nécessité d'exécuter les traitements à la seconde
bool shouldExecuteEachSecondTasks = false;
// Flags indiquant la nécessité de publier l'état ou les réglages en MQTT
bool shouldPublishState = false;
bool shouldPublishParams = false;

unsigned long refTimePingWifi = millis();
unsigned long refTimeContactEcoPV = millis();
unsigned long refTimeMqttConnected = millis();
bool contactEcoPV = false;
bool serialLogAllowed = true;   // faux dès que la liaison série est dédiée à l'Arduino Nano

// Variables pour la gestion du mode BOOST
long boostTime = -1;         // Temps restant pour le mode BOOST, en secondes (-1 = arrêt)

// Identifiants et topics MQTT (maxpv/<id>/...). Les pointeurs sont conservés par le client MQTT.
char mqttId[24];
char mqttTopicStatus[40];
char mqttTopicState[40];
char mqttTopicParams[40];
char mqttTopicPrefix[32];
char mqttTopicSub[40];
// Buffer unique de payload MQTT (discovery, état, réglages), utilisé uniquement par loop()
char mqttBuf[MQTT_DISCOVERY_BUF_SIZE];
// Buffer des réponses JSON du serveur web (/status, /params)
char httpBuf[MQTT_STATE_BUF_SIZE + 160];
// Prochaine entité de discovery à publier (MQTT_ENTITY_COUNT = terminé)
size_t mqttDiscoveryNext = MQTT_ENTITY_COUNT;

// ***********************************************************************************
// ********************** Déclaration des serveurs et des clients ********************
// ***********************************************************************************

AsyncWebServer    webServer(HTTP_PORT);
AsyncMqttClient   mqttClient;
AsyncPing ping;

// ***********************************************************************************
// ************************     FIN DES VARIABLES GLOBALES     ***********************
// ***********************************************************************************

// ***********************************************************************************
// **********************  FIN DES DEFINITIONS ET DECLARATIONS  **********************
// ***********************************************************************************



// ***********************************************************************************
// ***************************   FONCTIONS ET PROCEDURES   ***************************
// ***********************************************************************************

// ***********************************************************************************
// ***************************            SETUP()          ***************************
// ***************************   INITIALISATION GENERALE   ***************************
// ***********************************************************************************

void setup()
{
DNSServer         dnsServer;
AsyncWiFiManager  wifiManager(&webServer, &dnsServer);

// Déclaration des variables locales
  IPAddress         _ip, _gw, _sn, _dns1, _dns2;
  File              configFile;
  String            msg;
  unsigned long     refTime = millis();


  msg.reserve(32);

//Initialisation des variables globales de type String
  static_ip.reserve(15);
  static_gw.reserve(15);
  static_sn.reserve(15);
  static_dns1.reserve(15);
  static_dns2.reserve(15);
  static_ip =    DEFAULT_IP;
  static_gw =    DEFAULT_GW;
  static_sn =    DEFAULT_SN;
  static_dns1 =  DEFAULT_DNS1;
  static_dns2 =  DEFAULT_DNS2;

  mqttIP.reserve(MQTT_HOST_LENGTH);
  mqttUser.reserve(DEFAUT_USER_LENGTH);
  mqttPass.reserve(DEFAUT_PWD_LENGTH);
  mqttIP      = DEFAULT_MQTT_SERVER;
  mqttUser    = DEFAUT_EMPTY_USER;
  mqttPass    = DEFAUT_EMPTY_PWD;

  // Identifiant MQTT / Home Assistant et topics : maxpv/<id>/...
  snprintf(mqttId, sizeof(mqttId), "maxpv%lu", (unsigned long)ESP.getChipId());
  snprintf(mqttTopicStatus, sizeof(mqttTopicStatus), "maxpv/%s/status", mqttId);
  snprintf(mqttTopicState, sizeof(mqttTopicState), "maxpv/%s/state", mqttId);
  snprintf(mqttTopicParams, sizeof(mqttTopicParams), "maxpv/%s/params", mqttId);
  snprintf(mqttTopicPrefix, sizeof(mqttTopicPrefix), "maxpv/%s/", mqttId);
  snprintf(mqttTopicSub, sizeof(mqttTopicSub), "maxpv/%s/+/set", mqttId);

  // Début du debug sur liaison série
  // Attention : si SERIAL_LOG_ENABLE n'est pas définie à true, rien ne sera loggé sur la liaison série
  Serial.begin(SERIAL_LOG_BAUD);
  delay(50);
  logSerial ( F("[ESP]"), F(WELCOME_MESSAGE) );
  msg = F("Version : ");
  msg += MAXPV_VERSION_FULL;
  logSerial ( F("[ESP]"), msg );

  delay(5);

  // Montage du système de ficher et chargement de la configuration
  logSerial ( F("[ESP]"), F("Montage du filesystem") );
  // On teste l'existence du système de fichier
  // et sinon on formatte le système de fichier
  if (!LittleFS.begin()) {
    logSerial ( F("[ESP]"), F("Filesystem absent, formattage") );
    LittleFS.format();
    if (LittleFS.begin())
      logSerial ( F("[ESP]"), F("Filesystem prêt") );
    else {
      logSerial ( F("[ESP]"), F("Erreur de filesystem, redémarrage") );
      delay(100);
      ESP.restart();
    }
  }
  else logSerial ( F("[ESP]"), F("Filesystem prêt") );

  delay(5);

  // On teste l'existence du fichier de configuration de MaxPV!
  // Son absence n'est pas une erreur : les valeurs par défaut s'appliquent (DHCP, MQTT inactif)
  if (LittleFS.exists(F("/config.json"))) {
    String msg_tmp;
    configFile = LittleFS.open(F("/config.json"), "r");
    if (configFile.size() <= (4 * JSON_CONFIG_SIZE)) {
      msg_tmp.reserve(configFile.size());
      msg_tmp = configFile.readString();
    }
    configFile.close();
    logSerial ( F("[ESP]"), F("Fichier de configuration présent") );
    if (configRead(msg_tmp)) {
      logSerial ( F("[ESP]"), F("Configuration lue") );
    }
    else {
      logSerial ( F("[ESP]"), F("Fichier de configuration illisible, effacement, valeurs par défaut") );
      LittleFS.remove(F("/config.json"));
    }
  }
  else
    logSerial ( F("[ESP]"), F("Fichier de configuration absent, valeurs par défaut") );

  delay(5);

  // IP fixe optionnelle : gateway, subnet et dns1 doivent être valides, sinon DHCP
  staticIpActive = false;
  if (static_ip.length() > 0) {
    if ( _ip.fromString(static_ip) && _gw.fromString(static_gw)
         && _sn.fromString(static_sn) && _dns1.fromString(static_dns1) ) {
      _dns2.fromString(static_dns2);
      staticIpActive = true;
    }
    else logSerial ( F("[ESP]"), F("Configuration IP fixe invalide, DHCP") );
  }

  // Connexion au réseau Wifi. Le portail (AP) ne gère que le Wifi, il s'ouvre
  // si aucun réseau n'est mémorisé ou joignable
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setTryConnectDuringConfigPortal(true);
  wifiManager.setDebugOutput(true);
  if (staticIpActive) {
    wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn, _dns1, _dns2);
    msg = F("Adresse IP fixe : ");
    msg += _ip.toString();
    logSerial ( F("[ESP]"), msg );
  }
  else logSerial ( F("[ESP]"), F("Adresse IP par DHCP") );
  delay(5);

  // Réseau mémorisé : le portail est borné, puis redémarrage et nouvelle tentative
  if (WiFi.SSID().length() > 0) wifiManager.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT);
  if (!wifiManager.autoConnect(SSID_CP)) {
    logSerial ( F("[ESP]"), F("Échec de connexion Wifi, redémarrage") );
    delay(500);
    ESP.restart();
  }
  delay(500);

  msg = F("Connecté avec l'IP ");
  msg += WiFi.localIP().toString();
  logSerial ( F("[ESP]"), msg );

  WiFi.setAutoReconnect(true);
  wifiManager.setDebugOutput(false);
  delay(5);

  // Sauvegarde de la configuration MaxPV! si nécessaire
  if (shouldSaveConfig) {
    configWrite();
    logSerial ( F("[ESP]"), F("Configuration sauvegardée") );
    shouldSaveConfig = false;
  }
  delay(50);

  logSerial ( F("[ESP]"), F("Initialisation") );

  // Démarrage du client MQTT si configuré
  if (mqttEnabled()) {
    logSerial ( F("[ESP]"), F("Démarrage client Mqtt") );
    startMqtt();
  }

  // Fermeture du debug serial et fermeture de la liaison série
  logSerial ( F("[ESP]"), F("Fermeture de la liaison série") );
  delay(5);
  serialLogAllowed = false;
  Serial.end();
  delay(50);

  // Configuration et connexion à l'Arduino Nano
  Serial.setRxBufferSize(SERIAL_BUFFER);
  Serial.begin(SERIAL_BAUD);
  Serial.setTimeout(SERIALTIMEOUT);
  delay(5);
  clearSerialInputCache();
  refTime = millis();
  contactEcoPV = false;
  while (( (millis()-refTime) <= 3000 ) && (!contactEcoPV) ) serialProcess();
  delay(5);
  if (contactEcoPV) {
    // Premier peuplement des informations de configuration de EcoPV
    clearSerialInputCache();
    getAllParamEcoPV();
    delay(50);
    refTime = millis();
    while (!serialProcess() && ((millis()-refTime) <= NANO_BOOT_TIMEOUT)) { delay (10); }
    delay(5);
    clearSerialInputCache();
    delay(5);
    getVersionEcoPV();
    delay(50);
    refTime = millis();
    while (!serialProcess() && ((millis()-refTime) <= NANO_BOOT_TIMEOUT)) { delay (10); }
    delay(5);
    clearSerialInputCache();
    // Récupération des informations de fonctionnement
    // En moins d'une seconde EcoPV devrait envoyer les informations
    refTime = millis();
    while (!serialProcess() && ((millis()-refTime) <= NANO_BOOT_TIMEOUT)) { delay (10); }
    delay(5);
  }

  // Configuration des tâches planifiées
  setTsTasks ();
  delay(50);

  // Configuration du CallBack Ping
  setPingCallback ();
  delay(50);

  // Démarrage du serveur web
  startWeb();
  delay(100);

  // Démarrage mDNS
  if (MDNS.begin(MAXPV_MDNS)) {
    delay (10);
    MDNS.addService("http", "tcp", HTTP_PORT);
    delay (100);
  }

  refTimeMqttConnected = millis();
  clearSerialInputCache();
}

// ***********************************************************************************
// ***************************            LOOP ()          ***************************
// ***********************   BOUCLE INIFINE DE FONCTIONNEMENT   **********************
// ***********************************************************************************

void loop()
{
  static int i = 0;

  ElegantOTA.loop();

  // ***   Exécution des tâches récurrentes   ***

  i = 0;
  while ( i < 5 ) {
    if ( Serial.available ( ) ) serialProcess ( );
    yield();
    i++;
  }

  watchDogContactEcoPV();

  yield();

  ts.update();

  yield();

  // ***   Actions sur Flag   ***

  if (shouldSaveConfig) {
    configWrite();
    shouldSaveConfig = false;
  }

  yield();

  if ((shouldReboot) && (!shouldSaveConfig)) {
    rebootESP();
    shouldReboot = false;
  }

  yield();

  if ((shouldCheckWifi) && (wtdWifiActive==ON)) {
    watchDogWifi();
    shouldCheckWifi = false;
  }

  yield();

  if (shouldExecuteEachSecondTasks) {
    eachSecondTasks();
    shouldExecuteEachSecondTasks = false;
  }

  yield();

  // Une publication MQTT au plus par passage
  mqttService();

  yield();
}




// ***********************************************************************************
// ***********************       FONCTIONS ET PROCEDURES        **********************
// ***********************************************************************************

///////////////////////////////////////////////////////////////////
// Définition des tâches de TickerScheduler                      //
// Le nombre de tâches doit être en correspondance               //
// avec la définition de ts                                      //
///////////////////////////////////////////////////////////////////

void setTsTasks (void) {

  // Traitement des tâches à la seconde
  ts.add(
    0, 1000, [&](void *)
  {
    shouldExecuteEachSecondTasks = true;
  },
  nullptr, false);
}


///////////////////////////////////////////////////////////////////
// MQTT actif si et seulement si un hôte est configuré           //
///////////////////////////////////////////////////////////////////

bool mqttEnabled (void)
{
  return mqttIP.length() > 0;
}


///////////////////////////////////////////////////////////////////
// Gestion de la configuration de MaxPV!                         //
// bool configRead(const String& configString)                   //
// void configWrite(void)                                        //
///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
// Lecture d'une valeur numérique de la configuration            //
// La valeur est ramenée dans les bornes [minVal, maxVal].       //
// Si elle a été corrigée, la configuration sera réécrite        //
// (shouldSaveConfig)                                            //
///////////////////////////////////////////////////////////////////

long configBounded(JsonVariantConst value, long minVal, long maxVal)
{
  long v = value.as<long>();
  if ((v < minVal) || (v > maxVal)) {
    shouldSaveConfig = true;
    return constrain(v, minVal, maxVal);
  }
  return v;
}

///////////////////////////////////////////////////////////////////
// Lecture de la configuration à partir d'une chaîne             //
// de caractères en JSON (config.json)                           //
// Si une clef n'est pas présente dans le fichier,               //
// on garde la valeur par défaut et le fichier sera réécrit.     //
// Les clefs inconnues (anciennes versions) sont ignorées.       //
///////////////////////////////////////////////////////////////////

bool configRead(const String& configString)
{
  if (configString != "")
  {
    // buffer pour manipuler le fichier de configuration de MaxPV!
    DynamicJsonDocument jsonConfig(JSON_CONFIG_SIZE);

    DeserializationError error = deserializeJson(jsonConfig, configString);
    if ((!error) && jsonConfig.is<JsonObject>())
    {
      if (jsonConfig.containsKey("ip"))                   static_ip = jsonConfig["ip"].as<String>(); else shouldSaveConfig = true;
      if (jsonConfig.containsKey("gateway"))              static_gw = jsonConfig["gateway"].as<String>(); else shouldSaveConfig = true;
      if (jsonConfig.containsKey("subnet"))               static_sn = jsonConfig["subnet"].as<String>(); else shouldSaveConfig = true;
      if (jsonConfig.containsKey("dns1"))                 static_dns1 = jsonConfig["dns1"].as<String>(); else shouldSaveConfig = true;
      if (jsonConfig.containsKey("dns2"))                 static_dns2 = jsonConfig["dns2"].as<String>(); else shouldSaveConfig = true;
      if (jsonConfig.containsKey("watchdogwifi_active"))  wtdWifiActive = configBounded(jsonConfig["watchdogwifi_active"], 0, 1); else shouldSaveConfig = true;
      if (jsonConfig.containsKey("watchdogmqtt_active"))  wtdMqttActive = configBounded(jsonConfig["watchdogmqtt_active"], 0, 1); else shouldSaveConfig = true;
      if (jsonConfig.containsKey("mqtt_ip"))              mqttIP = jsonConfig["mqtt_ip"].as<String>(); else shouldSaveConfig = true;
      if (jsonConfig.containsKey("mqtt_port"))            mqttPort = configBounded(jsonConfig["mqtt_port"], 1, 65535); else shouldSaveConfig = true;
      if (jsonConfig.containsKey("mqtt_user"))            mqttUser = jsonConfig["mqtt_user"].as<String>(); else shouldSaveConfig = true;
      if (jsonConfig.containsKey("mqtt_pass"))            mqttPass = jsonConfig["mqtt_pass"].as<String>(); else shouldSaveConfig = true;
      if (jsonConfig.containsKey("mqtt_period"))          mqttPeriod = configBounded(jsonConfig["mqtt_period"], 1, 3600); else shouldSaveConfig = true;
      if (jsonConfig.containsKey("boost_duration"))       boostDuration = configBounded(jsonConfig["boost_duration"], 10, 600); else shouldSaveConfig = true;
      if (jsonConfig.containsKey("boost_ratio"))          boostRatio = configBounded(jsonConfig["boost_ratio"], 0, 100); else shouldSaveConfig = true;
      if (jsonConfig.containsKey("max_temp"))             maxTemp = configBounded(jsonConfig["max_temp"], 15, 90); else shouldSaveConfig = true;
      if (jsonConfig.containsKey("temp_limit_active"))    tempLimitActive = configBounded(jsonConfig["temp_limit_active"], 0, 1); else shouldSaveConfig = true;

      // Longueurs maximales des champs texte
      if (mqttIP.length() > MQTT_HOST_LENGTH)     { mqttIP = mqttIP.substring(0, MQTT_HOST_LENGTH); shouldSaveConfig = true; }
      if (mqttUser.length() > DEFAUT_USER_LENGTH) { mqttUser = mqttUser.substring(0, DEFAUT_USER_LENGTH); shouldSaveConfig = true; }
      if (mqttPass.length() > DEFAUT_PWD_LENGTH)  { mqttPass = mqttPass.substring(0, DEFAUT_PWD_LENGTH); shouldSaveConfig = true; }
    }
    else return false;
  }
  else return false;
  return true;
}

///////////////////////////////////////////////////////////////////
// Remplit un document JSON avec la configuration courante       //
// (clefs de config.json). Le mot de passe MQTT n'est inclus     //
// que pour l'écriture du fichier                                //
///////////////////////////////////////////////////////////////////

void configToJson(JsonDocument& jsonConfig, bool withPassword)
{
  jsonConfig["ip"] = static_ip;
  jsonConfig["gateway"] = static_gw;
  jsonConfig["subnet"] = static_sn;
  jsonConfig["dns1"] = static_dns1;
  jsonConfig["dns2"] = static_dns2;
  jsonConfig["watchdogwifi_active"] = wtdWifiActive;
  jsonConfig["watchdogmqtt_active"] = wtdMqttActive;
  jsonConfig["mqtt_ip"] = mqttIP;
  jsonConfig["mqtt_port"] = mqttPort;
  jsonConfig["mqtt_user"] = mqttUser;
  if (withPassword) jsonConfig["mqtt_pass"] = mqttPass;
  jsonConfig["mqtt_period"] = mqttPeriod;
  jsonConfig["boost_duration"] = boostDuration;
  jsonConfig["boost_ratio"] = boostRatio;
  jsonConfig["max_temp"] = maxTemp;
  jsonConfig["temp_limit_active"] = tempLimitActive;
}

///////////////////////////////////////////////////////////////////
// Ecriture de la configuration courante en JSON                 //
// directement dans le fichier config.json                       //
///////////////////////////////////////////////////////////////////

void configWrite(void)
{
  // buffer pour manipuler le fichier de configuration de MaxPV!
  DynamicJsonDocument jsonConfig(JSON_CONFIG_SIZE);

  configToJson(jsonConfig, true);

  if (jsonConfig.overflowed())
  {
    logSerial ( F("[ESP]"), F("Configuration trop volumineuse, non sauvegardée") );
    return;
  }

  // Écriture dans un fichier temporaire puis rename atomique : config.json reste intact en cas de coupure
  File configFile = LittleFS.open(F("/config.tmp"), "w");
  if (!configFile) {
    logSerial ( F("[ESP]"), F("Ouverture de config.tmp impossible, configuration non sauvegardée") );
    return;
  }
  size_t written = serializeJson(jsonConfig, configFile);
  configFile.close();
  if (written == 0) {
    LittleFS.remove(F("/config.tmp"));
    logSerial ( F("[ESP]"), F("Écriture de config.tmp échouée, configuration non sauvegardée") );
    return;
  }
  if (!LittleFS.rename(F("/config.tmp"), F("/config.json"))) {
    LittleFS.remove(F("/config.tmp"));
    logSerial ( F("[ESP]"), F("Renommage de config.tmp échoué, configuration non sauvegardée") );
  }
}


///////////////////////////////////////////////////////////////////
// Fonction Callback du portail AP de configuration Wifi         //
// void configModeCallback(AsyncWiFiManager *myWiFiManager)      //
///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
// Log des informations à la mise en place du point d'accès      //
// et du portail de configuration                                //
///////////////////////////////////////////////////////////////////

void configModeCallback(AsyncWiFiManager *myWiFiManager)
{
  String msg;
  msg.reserve(128);
  msg = F("Mode AP, SSID : ");
  msg += myWiFiManager->getConfigPortalSSID();
  logSerial ( F("[ESP]"), msg );
  msg = F("IP du portail de configuration : ");
  msg += WiFi.softAPIP().toString();
  logSerial ( F("[ESP]"), msg );
}


///////////////////////////////////////////////////////////////////
// Fonctions utiles de gestion de l'ESP                          //
// void rebootESP(void)                                          //
// void logSerial ( const String& logger, const String& message )//
///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
// Redémarrage software de l'ESP                                 //
// avec arrêt des connexions clients                             //
///////////////////////////////////////////////////////////////////

void rebootESP(void)
{
  logSerial (F("ESP"), F("!! Redémarrage !!"));
  delay (50);
  if (mqttClient.connected()) {
    // disponibilité "offline" explicite : une déconnexion propre n'envoie pas le LWT
    mqttClient.publish(mqttTopicStatus, 0, true, "offline");
    delay(50);
    mqttClient.disconnect();
  }
  delay(200);
  ESP.restart();
}

///////////////////////////////////////////////////////////////////
// log des informations sur la liaison série                     //
// Fonction utilisée lors du boot initial de l'ESP               //
///////////////////////////////////////////////////////////////////

void logSerial ( const String& logger, const String& message )
// !! A N'UTILISER QU'AU DEBUT DU SETUP AVANT LA CONNEXION A L'ARDUINO NANO !!
{
  if ( SERIAL_LOG_ENABLE && serialLogAllowed ) {
    String msg;
    msg.reserve(128);
    msg = String(millis() / 1000);
    msg += F("  ");
    msg += logger;
    msg += F("  ");
    msg += message;
    Serial.println(msg);
   }
}


///////////////////////////////////////////////////////////////////
// Fonctions de communication avec l'Arduino Nano                //
// par liaison série                                             //
// Traitement des données reçues et envoyées                     //
///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
// Vidage du buffer RX                                           //
///////////////////////////////////////////////////////////////////

void clearSerialInputCache(void)
{
  while (Serial.available() > 0)    Serial.read();
}

///////////////////////////////////////////////////////////////////
// Traitement des messages en provenance de l'Arduino Nano       //
///////////////////////////////////////////////////////////////////

bool serialProcess(void)
{
  int stringCounter = 0;
  int index = 0;
  String incomingData;

  incomingData.reserve(200);

  // Les chaînes valides envoyées par l'arduino se terminent toujours par END_OF_TRANSMIT = '#'
  incomingData = Serial.readStringUntil(END_OF_TRANSMIT);
  yield();

  if (incomingData.length() == 0) return false;

  // On teste la validité de la chaîne qui doit contenir 'END' à la fin
  else if (incomingData.endsWith(F("END")))
  {
    incomingData.replace(F(",END"), "");
    contactEcoPV = true;
    refTimeContactEcoPV = millis();

    if (incomingData.startsWith(F("STATS")))
    {
      incomingData.replace(F("STATS,"), "");
      stringCounter++; // on incrémente pour placer la première valeur à l'index 1
      while ((incomingData.length() > 0) && (stringCounter < NB_STATS))
      {
        index = incomingData.indexOf(',');
        if (index == -1)
        {
          ecoPVStats[stringCounter++] = incomingData;
          break;
        }
        else
        {
          ecoPVStats[stringCounter++] = incomingData.substring(0, index);
          incomingData = incomingData.substring(index + 1);
        }
      }
      // Conversion des index en kWh
      ecoPVStats[INDEX_ROUTED] = String((ecoPVStats[INDEX_ROUTED].toFloat() / 1000.0), 3);
      ecoPVStats[INDEX_IMPORT] = String((ecoPVStats[INDEX_IMPORT].toFloat() / 1000.0), 3);
      ecoPVStats[INDEX_EXPORT] = String((ecoPVStats[INDEX_EXPORT].toFloat() / 1000.0), 3);
      ecoPVStats[INDEX_IMPULSION] = String((ecoPVStats[INDEX_IMPULSION].toFloat() / 1000.0), 3);

      // Arrêt du Boost si la température ECS est atteinte
      // ou si la sonde est absente ou en défaut (température <= 0)
      if ( ( tempLimitActive == ON ) && ( boostTime > 0 ) && ( ( ecoPVStats[TEMP_ECS].toInt() <= 0 ) || ( ecoPVStats[TEMP_ECS].toInt() > maxTemp ) ) ) boostOFF ( );
    }

    else if (incomingData.startsWith(F("PARAM")))
    {
      incomingData.replace(F("PARAM,"), "");
      stringCounter++; // on incrémente pour placer la première valeur Vrms à l'index 1
      while ((incomingData.length() > 0) && (stringCounter < NB_PARAM))
      {
        index = incomingData.indexOf(',');
        if (index == -1)
        {
          ecoPVConfig[stringCounter++] = incomingData;
          break;
        }
        else
        {
          ecoPVConfig[stringCounter++] = incomingData.substring(0, index);
          incomingData = incomingData.substring(index + 1);
        }
      }
      shouldPublishParams = true;
    }

    else if (incomingData.startsWith(F("VERSION")))
    {
      incomingData.replace(F("VERSION,"), "");
      index = incomingData.indexOf(',');
      if (index != -1)
      {
        ecoPVConfig[ECOPV_VERSION] = incomingData.substring(0, index);
        ecoPVStats[ECOPV_VERSION] = ecoPVConfig[ECOPV_VERSION];
      }
    }

    else if (incomingData.startsWith(F("BOOSTTIME")))
    {
      incomingData.replace(F("BOOSTTIME,"), "");
      boostTime = incomingData.toInt();
    }

    return true;
  }
  else
  {
    return false;
  }
}

///////////////////////////////////////////////////////////////////
// Provoque le formattage de l'EEPROM de l'Arduino Nano          //
///////////////////////////////////////////////////////////////////

void formatEepromEcoPV(void)
{
  Serial.print(F("FORMAT,END#"));
}

///////////////////////////////////////////////////////////////////
// Provoque l'envoi des paramètres de configuration du routeur   //
///////////////////////////////////////////////////////////////////

void getAllParamEcoPV(void)
{
  Serial.print(F("PARAM,END#"));
  shouldReadParams = false;
}

///////////////////////////////////////////////////////////////////
// Modifie un paramètre de configuration du routeur              //
///////////////////////////////////////////////////////////////////

void setParamEcoPV(const String& param, const String& value)
{
  String command;
  command.reserve(32);
  command = F("SETPARAM,");
  if ((param.toInt() < 10) && (!param.startsWith("0")))
    command += "0";
  command += param;
  command += F(",");
  command += value;
  command += F(",END#");
  Serial.print(command);
  shouldReadParams = true; // on demande la lecture des paramètres contenus dans EcoPV
  // pour mettre à jour dans MaxPV
  // ce qui permet de vérifier la prise en compte de la commande
  // C'est par ce seul moyen de MaxPV met à jour sa base interne des paramètres
}

///////////////////////////////////////////////////////////////////
// Provoque l'envoi de la version du programme EcoPV             //
///////////////////////////////////////////////////////////////////

void getVersionEcoPV(void)
{
  Serial.print(F("VERSION,END#"));
}

///////////////////////////////////////////////////////////////////
// Provoque l'enregistrement de la configuration en EEPROM       //
///////////////////////////////////////////////////////////////////

void saveConfigEcoPV(void)
{
  Serial.print(F("SAVECFG,END#"));
}

///////////////////////////////////////////////////////////////////
// Provoque le chargement de la configuration depuis l'EEPROM    //
///////////////////////////////////////////////////////////////////

void loadConfigEcoPV(void)
{
  Serial.print(F("LOADCFG,END#"));
  shouldReadParams = true; // on demande la lecture des paramètres contenus dans EcoPV
  // pour mettre à jour dans MaxPV
  // ce qui permet de vérifier la prise en compte de la commande
  // C'est par ce seul moyen de MaxPV met à jour sa base interne des paramètres
}

///////////////////////////////////////////////////////////////////
// Provoque l'enregistrement des index en EEPROM                 //
///////////////////////////////////////////////////////////////////

void saveIndexEcoPV(void)
{
  Serial.print(F("SAVEINDX,END#"));
}

///////////////////////////////////////////////////////////////////
// Provoque la remise à zéro des index                           //
///////////////////////////////////////////////////////////////////

void resetIndexEcoPV(void)
{
  Serial.print(F("INDX0,END#"));
}

///////////////////////////////////////////////////////////////////
// Provoque le redémarrage software du routeur                   //
///////////////////////////////////////////////////////////////////

void restartEcoPV(void)
{
  Serial.print(F("RESET,END#"));
}

///////////////////////////////////////////////////////////////////
// Change le mode de fonctionnement du relais                    //
///////////////////////////////////////////////////////////////////

void relayModeEcoPV(byte opMode)
{
  String command;
  command.reserve(32);
  command = F("SETRELAY,");
  if (opMode == STOP)
    command += F("STOP");
  if (opMode == FORCE)
    command += F("FORCE");
  if (opMode == AUTOM)
    command += F("AUTO");
  command += F(",END#");
  Serial.print(command);

  // Le nouvel état est publié sans attendre la trame suivante de l'Arduino
  ecoPVStats[RELAY_MODE] = String(opMode);
}

///////////////////////////////////////////////////////////////////
// Change le mode de fonctionnement du SSR                       //
///////////////////////////////////////////////////////////////////

void SSRModeEcoPV(byte opMode)
{
  String command;
  command.reserve(32);
  command = F("SETSSR,");
  if (opMode == STOP)
    command += F("STOP");
  if (opMode == FORCE)
    command += F("FORCE");
  if (opMode == AUTOM)
    command += F("AUTO");
  command += F(",END#");
  Serial.print(command);

  // Le nouvel état est publié sans attendre la trame suivante de l'Arduino
  ecoPVStats[TRIAC_MODE] = String(opMode);
}

///////////////////////////////////////////////////////////////////
// Active ou arrête le mode BOOST                                //
// Doit être appelé par boostON() ou boostOFF()                  //
///////////////////////////////////////////////////////////////////

void boostModeEcoPV(long bT, int bR)
{
  String command;
  command.reserve(32);
  command = F("SETBOOST,");
  command += bT;
  command += F(",");
  command += bR;
  command += F(",END#");
  Serial.print(command);
}

///////////////////////////////////////////////////////////////////
// WatchDog de surveillance de fonctionnement du routeur         //
///////////////////////////////////////////////////////////////////

void watchDogContactEcoPV(void)
{
  if ((millis() - refTimeContactEcoPV) > 1500)
    contactEcoPV = false;
}


///////////////////////////////////////////////////////////////////
// Mode BOOST                                                    //
///////////////////////////////////////////////////////////////////

void boostON(void)
{
  if ( boostRatio != 0 ) {
    long bT = ( 60L * boostDuration );   // conversion en secondes
    boostModeEcoPV(bT,boostRatio);
    boostTime = bT;
  }
}


void boostOFF(void)
{
  boostModeEcoPV(0,boostRatio);
  boostTime = -1;
}


///////////////////////////////////////////////////////////////////
// Commandes : exécution d'une commande validée par handleCommand //
// Appelée par onMqttMessage (MQTT) et par POST /cmd (HTTP)      //
///////////////////////////////////////////////////////////////////

static byte modeToByte(int mode)
{
  if (mode == MQTT_MODE_STOP)  return STOP;
  if (mode == MQTT_MODE_FORCE) return FORCE;
  return AUTOM;
}

static MqttCmdContext currentCmdContext(void)
{
  MqttCmdContext ctx;
  ctx.tempLimitActive = (tempLimitActive == ON);
  ctx.temp = ecoPVStats[TEMP_ECS].toDouble();
  ctx.maxTemp = maxTemp;
  ctx.ssrStop = ( ecoPVStats[TRIAC_MODE].toInt() == STOP );
  ctx.boostActive = ( boostTime != -1 );
  return ctx;
}

void applyCommand(const MqttCommand& cmd)
{
  switch (cmd.action) {
    case CMD_SSR_MODE:
      SSRModeEcoPV ( modeToByte ( cmd.mode ) );
      shouldPublishState = true;
      break;
    case CMD_RELAY_MODE:
      relayModeEcoPV ( modeToByte ( cmd.mode ) );
      shouldPublishState = true;
      break;
    case CMD_BOOST_ON:
      boostON ( );
      shouldPublishState = true;
      break;
    case CMD_BOOST_OFF:
      boostOFF ( );
      shouldPublishState = true;
      break;
    case CMD_SET_PARAM:
      // la mise à jour de la base interne et de /params se fait à la réception de la trame PARAM
      setParamEcoPV ( String ( cmd.index + 1 ), String ( cmd.value ) );
      break;
    case CMD_SET_ESP:
      if      ( cmd.index == MQTT_ESP_BOOST_DURATION ) boostDuration = cmd.number;
      else if ( cmd.index == MQTT_ESP_BOOST_RATIO )    boostRatio = cmd.number;
      else if ( cmd.index == MQTT_ESP_MAX_TEMP )       maxTemp = cmd.number;
      else if ( cmd.index == MQTT_ESP_MQTT_PERIOD )    mqttPeriod = cmd.number;
      else if ( cmd.index == MQTT_ESP_TEMP_LIMIT )     tempLimitActive = cmd.number;
      shouldSaveConfig = true;
      shouldPublishParams = true;
      break;
    case CMD_RESTART_ESP:
      shouldReboot = true;
      break;
    case CMD_RESET_ECOPV:
      restartEcoPV ( );
      break;
    case CMD_SAVECFG:
      saveConfigEcoPV ( );
      break;
    case CMD_LOADCFG:
      loadConfigEcoPV ( );
      break;
    case CMD_SAVEINDX:
      saveIndexEcoPV ( );
      break;
    case CMD_INDX0:
      resetIndexEcoPV ( );
      break;
    case CMD_FORMAT:
      formatEepromEcoPV ( );
      break;
    case CMD_ERASE_CONFIG_ESP:
      LittleFS.remove ( F("/config.json") );
      break;
    default:
      break;
  }
}


///////////////////////////////////////////////////////////////////
// Fonctions de gestion des tâches récurrentes à la seconde      //
///////////////////////////////////////////////////////////////////

void eachSecondTasks(void)
  // Fonction appelée uniquement par la loop. On peut utiliser yield()
{
  generalCounterSecond++;

  // Update de mDNS
  MDNS.update();

  yield();

  // Toutes les 2 secondes on vérifie s'il faut aller récupérer les infos de config ecoPV
  if ( ( generalCounterSecond % 2 == 0 ) && ( shouldReadParams ) )  getAllParamEcoPV ( );
  // Le flag est remis à false dans la procédure getAllParamEcoPV ()

  yield();

  // Toutes les 119 secondes on demande les infos de config ecoPV
  if ( generalCounterSecond % 119 == 47 )  shouldReadParams = true;

  yield();

  // Toutes les 119 secondes on demande la version ecoPV
  if ( generalCounterSecond % 119 == 63 )  getVersionEcoPV();

  yield();

  // Publication périodique de l'état MQTT
  if ( mqttEnabled() && ( generalCounterSecond % mqttPeriod == 0 ) ) shouldPublishState = true;

  yield();

  // Watchdog MQTT : redémarrage si MQTT est configuré mais déconnecté depuis trop longtemps
  if ( mqttEnabled() ) {
    if ( mqttClient.connected() ) refTimeMqttConnected = millis();
    else if ( ( wtdMqttActive == ON ) && ( ( millis() - refTimeMqttConnected ) > MQTT_WATCHDOG_TIMEOUT ) ) {
      logSerial ( F("[ESP]"), F("Watchdog MQTT : déconnecté trop longtemps, redémarrage") );
      delay(50);
      ESP.restart();
    }
  }

  yield();

  // On ping périodiquement, ici toutes les 30 secondes, 2 fois maxi, timeout 1000 ms, si le watchdog wifi est actif
  // Cible : dns1 si l'IP est fixe, sinon la passerelle
  if ((generalCounterSecond % 30 == 7) && (wtdWifiActive==ON)) {
    IPAddress _target;
    if ( staticIpActive ) _target.fromString(static_dns1);
    else _target = WiFi.gatewayIP();
    ping.begin (_target, 2, 1000);
  }

  yield();
}



///////////////////////////////////////////////////////////////////
// Fonctions de gestion de la connexion Mqtt                     //
// et de l'autodiscovery Home Assisant                           //
///////////////////////////////////////////////////////////////////

void startMqtt (void)
{
  mqttClient.onConnect(onMqttConnect);        // Appel de la fonction lors d'une connexion MQTT établie
  mqttClient.onDisconnect(onMqttDisconnect);  // Appel de la fonction lors d'une déconnexion MQTT
  mqttClient.onMessage(onMqttMessage);        // Appel de la fonction lors de la réception d'un message MQTT
  mqttClient.setClientId(mqttId);
  if ( mqttUser.length() != 0 ) {
     mqttClient.setCredentials(mqttUser.c_str(), mqttPass.c_str());
  }
  mqttClient.setServer(mqttIP.c_str(), mqttPort);
  mqttClient.setWill(mqttTopicStatus, 0, true, "offline");
  mqttConnect();
  delay(100);
}


void mqttConnect (void)
{
  mqttClient.connect();
}


void onMqttConnect(bool sessionPresent)
{
  // Souscription unique à toutes les commandes : maxpv/<id>/+/set
  mqttClient.subscribe(mqttTopicSub, 0);

  // Disponibilité
  mqttClient.publish(mqttTopicStatus, 0, true, "online");

  // Discovery, réglages et état sont publiés progressivement par mqttService()
  mqttDiscoveryNext = 0;
  shouldPublishParams = true;
  shouldPublishState = true;
}


void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  mqttReconnectTimer.once(RECONNECT_TIME, mqttConnect);
}


void onMqttMessage(char* topic, char* payload, const AsyncMqttClientMessageProperties& properties,
                   const size_t& len, const size_t& index, const size_t& total)
{
  // Topic attendu : maxpv/<id>/<clé>/set. Le payload n'est pas terminé par un NUL :
  // il est lu sur len octets, copié, et ignoré s'il est fragmenté ou trop long.
  char key[24];
  char value[MQTT_PAYLOAD_MAX + 1];
  size_t prefixLen = strlen(mqttTopicPrefix);
  size_t topicLen = strlen(topic);

  if ( ( index != 0 ) || ( len != total ) || ( len > MQTT_PAYLOAD_MAX ) ) return;
  if ( ( payload == NULL ) && ( len > 0 ) ) return;
  if ( ( len > 0 ) && ( memchr(payload, 0, len) != NULL ) ) return;
  if ( strncmp(topic, mqttTopicPrefix, prefixLen) != 0 ) return;
  if ( ( topicLen < prefixLen + 5 ) || ( strcmp(topic + topicLen - 4, "/set") != 0 ) ) return;
  if ( ( topicLen - 4 - prefixLen ) >= sizeof(key) ) return;

  memcpy(key, topic + prefixLen, topicLen - 4 - prefixLen);
  key[topicLen - 4 - prefixLen] = '\0';
  if ( len > 0 ) memcpy(value, payload, len);
  value[len] = '\0';

  MqttCommand cmd;
  if ( handleCommand ( key, value, false, currentCmdContext(), cmd ) ) applyCommand ( cmd );
}


///////////////////////////////////////////////////////////////////
// Construction de l'état et des réglages publiés                //
///////////////////////////////////////////////////////////////////

static void fillMqttState(MqttState& s, const String& resetReason)
{
  long statusByte = ecoPVStats[STATUS_BYTE].toInt();

  s.vrms = ecoPVStats[V_RMS].toDouble();
  s.irms = ecoPVStats[I_RMS].toDouble();
  s.pact = ecoPVStats[P_ACT].toInt();
  s.papp = ecoPVStats[P_APP].toInt();
  s.prouted = ecoPVStats[P_ROUTED].toInt();
  s.pimp = ecoPVStats[P_IMP].toInt();
  s.pexp = ecoPVStats[P_EXP].toInt();
  s.cosphi = ecoPVStats[COS_PHI].toDouble();
  s.pimpulsion = ecoPVStats[P_IMPULSION].toInt();
  s.idxRouted = ecoPVStats[INDEX_ROUTED].toDouble();
  s.idxImport = ecoPVStats[INDEX_IMPORT].toDouble();
  s.idxExport = ecoPVStats[INDEX_EXPORT].toDouble();
  s.idxImpulsion = ecoPVStats[INDEX_IMPULSION].toDouble();
  s.idxRelay = ecoPVStats[INDEX_RELAY].toInt();
  s.triacMode = ecoPVStats[TRIAC_MODE].toInt();
  s.relayMode = ecoPVStats[RELAY_MODE].toInt();
  s.relayOn = ( statusByte & B00000100 ) != 0;
  s.temp = ecoPVStats[TEMP_ECS].toDouble();
  s.boost = ( boostTime != -1 );
  s.nano = contactEcoPV;
  s.rssi = WiFi.RSSI();
  s.uptime = generalCounterSecond;
  s.heap = ESP.getFreeHeap();
  s.resetReason = resetReason.c_str();
  s.ecopvVersion = ecoPVConfig[ECOPV_VERSION].c_str();
  s.delayMin = ecoPVStats[DELAY_MIN].toInt();
  s.delayAvg = ecoPVStats[DELAY_AVG].toInt();
  s.delayMax = ecoPVStats[DELAY_MAX].toInt();
  s.biasOffset = ecoPVStats[BIAS_OFFSET].toInt();
  s.statusByte = statusByte;
  s.onTime = strtoul(ecoPVStats[ONTIME].c_str(), NULL, 10);
  s.samples = strtoul(ecoPVStats[SAMPLES].c_str(), NULL, 10);
}

static void fillMqttParams(MqttParams& p)
{
  for (int k = 0; k < 16; k++) p.ecopv[k] = ecoPVConfig[V_CALIB + k].c_str();
  p.boostDuration = boostDuration;
  p.boostRatio = boostRatio;
  p.maxTemp = maxTemp;
  p.tempLimit = tempLimitActive;
  p.mqttPeriod = mqttPeriod;
}

///////////////////////////////////////////////////////////////////
// Publication MQTT progressive et non bloquante                 //
// Appelée une fois par passage dans loop(). Un échec de        //
// publish() (retour 0) est réessayé au passage suivant          //
///////////////////////////////////////////////////////////////////

void mqttService(void)
{
  if ( !mqttEnabled() || !mqttClient.connected() ) return;

  // 1. Discovery : une entité par passage
  if ( mqttDiscoveryNext < MQTT_ENTITY_COUNT ) {
    char topic[112];
    String ip = WiFi.localIP().toString();
    size_t topicLen = mqttDiscoveryTopic(topic, sizeof(topic), mqttDiscoveryNext, mqttId);
    size_t payloadLen = mqttDiscoveryPayload(mqttBuf, sizeof(mqttBuf), mqttDiscoveryNext, mqttId, ip.c_str());
    if ( ( topicLen == 0 ) || ( payloadLen == 0 ) ) mqttDiscoveryNext++;   // ne doit pas arriver
    else if ( mqttClient.publish(topic, 0, true, mqttBuf, payloadLen) != 0 ) mqttDiscoveryNext++;
    return;
  }

  // 2. Réglages (une fois les paramètres EcoPV connus)
  if ( shouldPublishParams ) {
    if ( ecoPVConfig[V_CALIB].length() == 0 ) shouldPublishParams = false;   // republié à la réception de PARAM
    else {
      MqttParams p;
      fillMqttParams(p);
      size_t n = buildParamsJson(mqttBuf, sizeof(mqttBuf), p);
      if ( ( n == 0 ) || ( mqttClient.publish(mqttTopicParams, 0, true, mqttBuf, n) != 0 ) ) shouldPublishParams = false;
    }
    return;
  }

  // 3. Etat
  if ( shouldPublishState ) {
    MqttState s;
    String resetReason = ESP.getResetReason();
    fillMqttState(s, resetReason);
    size_t n = buildStateJson(mqttBuf, sizeof(mqttBuf), s);
    if ( ( n == 0 ) || ( mqttClient.publish(mqttTopicState, 0, true, mqttBuf, n) != 0 ) ) shouldPublishState = false;
  }
}

///////////////////////////////////////////////////////////////////
// Fonctions Callback Ping                                       //
///////////////////////////////////////////////////////////////////

void setPingCallback ( void )
{
  // callback for each answer/timeout of ping
  ping.on(true,[](const AsyncPingResponse& response){
    if (response.answer) {
      // le ping est revenu valide, on ré-initialise le WD
      refTimePingWifi = millis();
      return true; // on s'arrête
    }
    else {
      // le ping n'est pas revenu
      shouldCheckWifi = true;
      return false;
    }
  });
}


///////////////////////////////////////////////////////////////////
// Fonctions du WatchDog Wifi                                    //
///////////////////////////////////////////////////////////////////

void watchDogWifi ( void )
{
  if ( ( millis() - refTimePingWifi ) > PING_WIFI_TIMEOUT ) {
    WiFi.mode(WIFI_OFF);
    delay(20);
    WiFi.mode(WIFI_STA);
    delay(20);
    WiFi.reconnect();
    delay(50);
    refTimePingWifi = millis();
  }
}


///////////////////////////////////////////////////////////////////
// Fonctions de gestion du serveur Web                           //
///////////////////////////////////////////////////////////////////

void startWeb (void)
{
  ElegantOTA.begin(&webServer);
  delay(100);
  setWebHandlers();
  delay(100);
  webServer.begin();
  delay(2000);
}


// Anti-CSRF : un en-tête personnalisé force un preflight CORS, que l'ESP ne satisfait pas
static bool checkMutationHeader(AsyncWebServerRequest *request) {
  if ( !request->hasHeader("X-MaxPV") ) {
    request->send ( 403, "text/plain", F("Forbidden: missing X-MaxPV header") );
    return false;
  }
  return true;
}

static void sendMethodNotAllowed(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse ( 405, "text/plain", F("Method Not Allowed: use POST") );
  response->addHeader("Allow", "POST");
  request->send ( response );
}

// Fichier statique du filesystem, 404 s'il est absent
static void sendFile(AsyncWebServerRequest *request, const char *path, const char *contentType) {
  if ( LittleFS.exists ( path ) ) request->send ( LittleFS, path, contentType );
  else request->send ( 404, "text/plain", F("Not found") );
}

// Page d'accueil, avec un message si le filesystem n'est pas chargé
static void sendIndex(AsyncWebServerRequest *request) {
  if ( LittleFS.exists ( F("/index.html") ) ) {
    request->send ( LittleFS, F("/index.html"), "text/html" );
  }
  else {
    String response = F("Site Web non trouvé. Filesystem non chargé. Allez à : http://");
    response += WiFi.localIP().toString();
    response += F("/update pour uploader le filesystem.");
    request->send ( 200, "text/plain", response );
  }
}

// Valeur d'un champ de formulaire (corps x-www-form-urlencoded)
static bool formString(AsyncWebServerRequest *request, const char *name, String& value) {
  if ( !request->hasParam ( name, true ) ) return false;
  value = request->getParam ( name, true )->value();
  return true;
}

// Entier d'un champ de formulaire, ramené dans les bornes.
// Retour : 0 = champ absent, 1 = ok, -1 = format invalide
static int formLong(AsyncWebServerRequest *request, const char *name, long minVal, long maxVal, long& value) {
  String s;
  if ( !formString ( request, name, s ) ) return 0;
  size_t len = s.length();
  if ( ( len < 1 ) || ( len > 7 ) ) return -1;
  bool digit = false;
  for ( size_t i = 0; i < len; i++ ) {
    char c = s[i];
    if ( ( c >= '0' ) && ( c <= '9' ) ) digit = true;
    else if ( ( c == '-' ) && ( i == 0 ) ) continue;
    else return -1;
  }
  if ( !digit ) return -1;
  value = constrain ( s.toInt(), minVal, maxVal );
  return 1;
}

// Vide ou adresse IPv4 valide
static bool isIpOrEmpty(const String& s) {
  IPAddress a;
  return ( s.length() == 0 ) || a.fromString ( s );
}

// Hôte MQTT : IP ou nom DNS, 64 caractères max
static bool isValidHost(const String& s) {
  if ( s.length() > MQTT_HOST_LENGTH ) return false;
  for ( size_t i = 0; i < s.length(); i++ ) {
    char c = s[i];
    if ( !( ( c >= '0' && c <= '9' ) || ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || c == '.' || c == '-' || c == '_' ) ) return false;
  }
  return true;
}

// Utilisateur ou mot de passe MQTT : longueur bornée, pas de caractère de contrôle
static bool isValidCredential(const String& s, size_t maxLen) {
  if ( s.length() > maxLen ) return false;
  for ( size_t i = 0; i < s.length(); i++ ) {
    if ( ( (unsigned char) s[i] < 0x20 ) || ( s[i] == 0x7f ) ) return false;
  }
  return true;
}

// POST /config : valide tout le corps avant d'appliquer quoi que ce soit
static void handlePostConfig(AsyncWebServerRequest *request) {
  if ( !checkMutationHeader ( request ) ) return;

  String ip = static_ip, gw = static_gw, sn = static_sn, dns1 = static_dns1, dns2 = static_dns2;
  String host = mqttIP, user = mqttUser, pass = mqttPass, t;
  long wifiWd = wtdWifiActive, mqttWd = wtdMqttActive, port = mqttPort, period = mqttPeriod;
  long bDuration = boostDuration, bRatio = boostRatio, mTemp = maxTemp, tLimit = tempLimitActive;
  bool bad = false;

  if ( formString ( request, "ip", t ) )       { if ( isIpOrEmpty ( t ) ) ip = t; else bad = true; }
  if ( formString ( request, "gateway", t ) )  { if ( isIpOrEmpty ( t ) ) gw = t; else bad = true; }
  if ( formString ( request, "subnet", t ) )   { if ( isIpOrEmpty ( t ) ) sn = t; else bad = true; }
  if ( formString ( request, "dns1", t ) )     { if ( isIpOrEmpty ( t ) ) dns1 = t; else bad = true; }
  if ( formString ( request, "dns2", t ) )     { if ( isIpOrEmpty ( t ) ) dns2 = t; else bad = true; }
  if ( formString ( request, "mqtt_ip", t ) )  { if ( isValidHost ( t ) ) host = t; else bad = true; }
  if ( formString ( request, "mqtt_user", t ) ) { if ( isValidCredential ( t, DEFAUT_USER_LENGTH ) ) user = t; else bad = true; }
  // mot de passe vide ou absent : conservé
  if ( formString ( request, "mqtt_pass", t ) ) {
    if ( !isValidCredential ( t, DEFAUT_PWD_LENGTH ) ) bad = true;
    else if ( t.length() > 0 ) pass = t;
  }
  if ( formLong ( request, "watchdogwifi_active", 0, 1, wifiWd ) < 0 )     bad = true;
  if ( formLong ( request, "watchdogmqtt_active", 0, 1, mqttWd ) < 0 )     bad = true;
  if ( formLong ( request, "mqtt_port", 1, 65535, port ) < 0 )             bad = true;
  if ( formLong ( request, "mqtt_period", 1, 3600, period ) < 0 )          bad = true;
  if ( formLong ( request, "boost_duration", 10, 600, bDuration ) < 0 )    bad = true;
  if ( formLong ( request, "boost_ratio", 0, 100, bRatio ) < 0 )           bad = true;
  if ( formLong ( request, "max_temp", 15, 90, mTemp ) < 0 )               bad = true;
  if ( formLong ( request, "temp_limit_active", 0, 1, tLimit ) < 0 )       bad = true;

  // IP fixe : gateway, subnet et dns1 sont obligatoires et valides
  if ( ip.length() > 0 ) {
    IPAddress a;
    if ( !a.fromString ( ip ) || !a.fromString ( gw ) || !a.fromString ( sn ) || !a.fromString ( dns1 ) ) bad = true;
  }

  if ( bad ) {
    request->send ( 400, "text/plain", F("Bad request: invalid configuration") );
    return;
  }

  static_ip = ip;  static_gw = gw;  static_sn = sn;  static_dns1 = dns1;  static_dns2 = dns2;
  mqttIP = host;  mqttUser = user;  mqttPass = pass;
  wtdWifiActive = wifiWd;  wtdMqttActive = mqttWd;  mqttPort = port;  mqttPeriod = period;
  boostDuration = bDuration;  boostRatio = bRatio;  maxTemp = mTemp;  tempLimitActive = tLimit;

  // Sauvegarde puis redémarrage (loop)
  shouldSaveConfig = true;
  shouldReboot = true;
  request->send ( 200, "text/plain", F("Request successfully processed") );
}

// GET /config et GET /backup : configuration sans le mot de passe MQTT
static void sendConfigJson(AsyncWebServerRequest *request, bool attachment) {
  DynamicJsonDocument jsonConfig(JSON_CONFIG_SIZE);
  String out;
  configToJson ( jsonConfig, false );
  serializeJson ( jsonConfig, out );
  AsyncWebServerResponse *response = request->beginResponse ( 200, "application/json", out );
  if ( attachment ) response->addHeader ( "Content-Disposition", "attachment; filename=\"maxpv_config.json\"" );
  request->send ( response );
}

// POST /cmd : même validation et même action que les commandes MQTT
static void handlePostCmd(AsyncWebServerRequest *request) {
  if ( !checkMutationHeader ( request ) ) return;
  if ( !request->hasParam ( F("key"), true ) || !request->hasParam ( F("value"), true ) ) {
    request->send ( 400, "text/plain", F("Bad request") );
    return;
  }
  String key = request->getParam ( F("key"), true )->value();
  String value = request->getParam ( F("value"), true )->value();
  MqttCommand cmd;
  if ( !handleCommand ( key.c_str(), value.c_str(), true, currentCmdContext(), cmd ) ) {
    request->send ( 400, "text/plain", F("Bad request: invalid command") );
    return;
  }
  applyCommand ( cmd );
  request->send ( 200, "text/plain", F("Request successfully processed") );
}

void setWebHandlers (void) {

  // ***********************************************************************
  // ********      DECLARATIONS DES HANDLERS DU SERVEUR WEB         ********
  // ***********************************************************************

  // Toute autre route : 404
  webServer.onNotFound([](AsyncWebServerRequest * request)
  {
    request->send ( 404, "text/plain", F("Not found") );
  });

  // Fichiers du filesystem
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    sendIndex ( request );
  });

  webServer.on("/index.html", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    sendIndex ( request );
  });

  webServer.on("/app.js", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    sendFile ( request, "/app.js", "application/javascript" );
  });

  webServer.on("/pico.min.css", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    sendFile ( request, "/pico.min.css", "text/css" );
  });

  webServer.on("/maxpv.css", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    sendFile ( request, "/maxpv.css", "text/css" );
  });

  webServer.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    sendFile ( request, "/favicon.ico", "image/x-icon" );
  });

  webServer.on("/versionweb.txt", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    sendFile ( request, "/versionweb.txt", "text/plain" );
  });

  delay(5);

  // ***********************************************************************
  // ********          BACKEND INTERNE DE L'INTERFACE WEB           ********
  // ***********************************************************************

  // Etat (même JSON que MQTT) plus quelques informations de l'ESP
  webServer.on("/status", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    MqttState s;
    String resetReason = ESP.getResetReason();
    fillMqttState ( s, resetReason );
    size_t n = buildStateJson ( httpBuf, sizeof(httpBuf), s );
    if ( n < 2 ) {
      request->send ( 500, "text/plain", F("Internal error") );
      return;
    }
    // on remplace l'accolade finale pour ajouter les champs propres à l'ESP
    int m = snprintf ( httpBuf + n - 1, sizeof(httpBuf) - n + 1,
                       ",\"mqtt\":\"%s\",\"ip\":\"%s\",\"maxpv_version\":\"%s\",\"mqtt_id\":\"%s\"}",
                       mqttClient.connected() ? "ON" : "OFF", WiFi.localIP().toString().c_str(), MAXPV_VERSION, mqttId );
    if ( ( m < 0 ) || ( (size_t) m >= ( sizeof(httpBuf) - n + 1 ) ) ) {
      request->send ( 500, "text/plain", F("Internal error") );
      return;
    }
    request->send ( 200, "application/json", httpBuf );
  });

  webServer.on("/params", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    MqttParams p;
    fillMqttParams ( p );
    size_t n = buildParamsJson ( httpBuf, sizeof(httpBuf), p );
    if ( n == 0 ) {
      request->send ( 500, "text/plain", F("Internal error") );
      return;
    }
    request->send ( 200, "application/json", httpBuf );
  });

  webServer.on("/config", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    sendConfigJson ( request, false );
  });

  webServer.on("/config", HTTP_POST, [](AsyncWebServerRequest * request)
  {
    handlePostConfig ( request );
  });

  webServer.on("/backup", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    sendConfigJson ( request, true );
  });

  webServer.on("/cmd", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    sendMethodNotAllowed ( request );
  });

  webServer.on("/cmd", HTTP_POST, [](AsyncWebServerRequest * request)
  {
    handlePostCmd ( request );
  });
}
