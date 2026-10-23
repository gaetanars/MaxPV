/*
  maxpv_mqtt.h - MQTT / Home Assistant de MaxPV! : etat JSON, discovery, commandes.
  Fonctions pures (aucune dependance au materiel), testees en natif
  (test/test_mqtt). Voir docs/specs/ha-mqtt-ui/SPEC.md (R4, R5, R6).

  This file is a part of MaxPV_ESP.ino, licensed under the GNU Lesser General
  Public License, version 2.1 or (at your option) any later version.
*/

#pragma once

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ArduinoJson.h>
#include "maxpv_defines.h"
#include "maxpv_validate.h"

// Shims pour la compilation native (sur ESP8266 ces symboles viennent du core)
#ifndef ARDUINO
#ifndef PROGMEM
#define PROGMEM
#endif
#define PSTR(s) (s)
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#define memcpy_P memcpy
#define strncpy_P strncpy
#define strlen_P strlen
#endif

#define MQTT_PAYLOAD_MAX 16   // longueur max d'un payload de commande
#define MQTT_ENTITY_COUNT 59  // 27 sensors, 2 binary_sensors, 2 selects, 2 switchs, 20 numbers, 6 buttons

// Tailles des buffers fixes (verifiees par test/test_mqtt)
#define MQTT_DISCOVERY_BUF_SIZE 768
#define MQTT_STATE_BUF_SIZE 768
#define MQTT_PARAMS_BUF_SIZE 640

static_assert(MQTT_DISCOVERY_BUF_SIZE >= MQTT_STATE_BUF_SIZE && MQTT_STATE_BUF_SIZE >= MQTT_PARAMS_BUF_SIZE,
              "un seul buffer de payload est reutilise : le plus grand doit etre MQTT_DISCOVERY_BUF_SIZE");

// ---------------------------------------------------------------------------
// Ecriture bornee dans un buffer (jamais de troncature : ok passe a false)
// ---------------------------------------------------------------------------

struct MqttWriter {
  char *buf;
  size_t size;
  size_t len;
  bool ok;
};

inline void mqttPut(MqttWriter &w, const char *s) {
  size_t n = strlen(s);
  if (!w.ok || w.len + n + 1 > w.size) {
    w.ok = false;
    return;
  }
  memcpy(w.buf + w.len, s, n);
  w.len += n;
}

inline void mqttPutP(MqttWriter &w, const char *s) {  // s en PROGMEM
  size_t n = strlen_P(s);
  if (!w.ok || w.len + n + 1 > w.size) {
    w.ok = false;
    return;
  }
  memcpy_P(w.buf + w.len, s, n);
  w.len += n;
}

inline size_t mqttEnd(MqttWriter &w) {
  if (!w.ok || w.size == 0) return 0;
  w.buf[w.len] = '\0';
  return w.len;
}

// ---------------------------------------------------------------------------
// Table des entites Home Assistant (PROGMEM)
// Une ligne par entite, champs separes par '|' :
//  0 composant | 1 suffixe d'uniq_id | 2 nom | 3 cle (JSON, ou commande pour un
//  button) | 4 source (s = /state, p = /params, - = aucune) | 5 device_class |
//  6 state_class | 7 unite | 8 categorie (d = diagnostic, c = config) |
//  9 actif par defaut (0 = desactive) | 10 min | 11 max | 12 pas
// Les 18 premiers suffixes historiques (sensors, selects, switch Boost) sont
// conserves pour que HA garde les entites. L'ordre des numbers est celui des
// parametres EcoPV (16) puis boost_duration, boost_ratio, max_temp, mqtt_period.
// ---------------------------------------------------------------------------

#define MQTT_FIELD_COUNT 13
#define MQTT_LINE_MAX 176

static const char MQTT_ENTITY_TABLE[] PROGMEM =
    "sensor|Tension|Tension|vrms|s|voltage|measurement|V\n"
    "sensor|Courant|Courant|irms|s|current|measurement|A\n"
    "sensor|PuissanceApparente|Puissance apparente|papp|s|apparent_power|measurement|VA\n"
    "sensor|FacteurPuissance|Facteur de puissance|cosphi|s|power_factor|measurement\n"
    "sensor|PuissanceActive|Puissance active|pact|s|power|measurement|W\n"
    "sensor|PuissanceRoutee|Puissance routée|prouted|s|power|measurement|W\n"
    "sensor|PuissanceImportee|Puissance importée|pimp|s|power|measurement|W\n"
    "sensor|PuissanceExportee|Puissance exportée|pexp|s|power|measurement|W\n"
    "sensor|PuissanceProduite|Puissance produite|pimpulsion|s|power|measurement|W\n"
    "sensor|Temperature|Température ECS|temp|s|temperature|measurement|°C\n"
    "sensor|EnergieRoutee|Energie routée|idx_routed|s|energy|total_increasing|kWh\n"
    "sensor|EnergieImportee|Energie importée|idx_import|s|energy|total_increasing|kWh\n"
    "sensor|EnergieExportee|Energie exportée|idx_export|s|energy|total_increasing|kWh\n"
    "sensor|EnergieProduite|Energie produite|idx_impulsion|s|energy|total_increasing|kWh\n"
    "sensor|TempsFonctionnementRelais|Temps de fonctionnement relais|idx_relay|s|duration|total_increasing|min\n"
    "sensor|RSSI|RSSI|rssi|s|signal_strength|measurement|dBm|d\n"
    "sensor|Uptime|Uptime|uptime|s|duration||s|d\n"
    "sensor|VersionEcoPV|Version EcoPV|ecopv_version|s||||d\n"
    "sensor|MemoireLibre|Mémoire libre|heap|s||measurement|B|d|0\n"
    "sensor|RaisonReset|Raison du dernier reset|reset_reason|s||||d|0\n"
    "sensor|DelayMin|Délai min|delay_min|s||measurement||d|0\n"
    "sensor|DelayAvg|Délai moyen|delay_avg|s||measurement||d|0\n"
    "sensor|DelayMax|Délai max|delay_max|s||measurement||d|0\n"
    "sensor|BiasOffset|Offset du biais|bias_offset|s||measurement||d|0\n"
    "sensor|StatusByte|Octet de statut|status_byte|s||||d|0\n"
    "sensor|OnTime|Temps de marche|ontime|s||total_increasing||d|0\n"
    "sensor|Samples|Échantillons|samples|s||total_increasing||d|0\n"
    "binary_sensor|EtatRelais|État du relais|relay_on|s|power\n"
    "binary_sensor|ContactRouteur|Contact routeur|nano|s|connectivity|||d\n"
    "select|SSR|SSR|triac_mode|s\n"
    "select|Relais|Relais|relay_mode|s\n"
    "switch|Boost|Boost|boost|s\n"
    "switch|LimiteTemperature|Limite de température|temp_limit|p||||c\n"
    "number|V_CALIB|Calibrage tension|v_calib|p||||c||0|2|0.0001\n"
    "number|P_CALIB|Calibrage puissance|p_calib|p||||c||0|1|0.0001\n"
    "number|PHASE_CALIB|Calibrage phase|phase_calib|p||||c||-16|48|1\n"
    "number|P_OFFSET|Offset de puissance|p_offset|p|||W|c||-100|100|1\n"
    "number|P_RESISTANCE|Puissance de la résistance|p_resistance|p|||W|c||100|10000|1\n"
    "number|P_MARGIN|Marge de puissance|p_margin|p|||W|c||-2000|2000|1\n"
    "number|GAIN_P|Gain proportionnel|gain_p|p||||c||0|1000|1\n"
    "number|GAIN_I|Gain intégral|gain_i|p||||c||0|1000|1\n"
    "number|E_RESERVE|Réserve d'énergie|e_reserve|p|||Wh|c||0|200|1\n"
    "number|P_DIV2_ACTIVE|Seuil division par 2 actif|p_div2_active|p|||W|c||0|9999|1\n"
    "number|P_DIV2_IDLE|Seuil division par 2 repos|p_div2_idle|p|||W|c||0|9999|1\n"
    "number|T_DIV2_ON|Durée division par 2 on|t_div2_on|p|||s|c||0|240|1\n"
    "number|T_DIV2_OFF|Durée division par 2 off|t_div2_off|p|||s|c||0|240|1\n"
    "number|T_DIV2_TC|Constante de temps division par 2|t_div2_tc|p|||s|c||0|60|1\n"
    "number|CNT_CALIB|Calibrage compteur|cnt_calib|p||||c||0|1000|0.0001\n"
    "number|P_INSTALLPV|Puissance de l'installation PV|p_installpv|p|||W|c||100|30000|1\n"
    "number|BoostDuree|Durée du BOOST|boost_duration|p|||min|c||10|600|1\n"
    "number|BoostRatio|Puissance du BOOST|boost_ratio|p|||%|c||0|100|1\n"
    "number|TempMax|Température max ECS|max_temp|p|||°C|c||15|90|1\n"
    "number|PeriodeMQTT|Période de publication MQTT|mqtt_period|p|||s|c||1|3600|1\n"
    "button|RedemarrerESP|Redémarrer MaxPV!|restartesp|-|restart\n"
    "button|RedemarrerRouteur|Redémarrer le routeur|resetecopv|-|restart\n"
    "button|SauverParametres|Sauver les paramètres|savecfg|-||||c\n"
    "button|RechargerParametres|Recharger les paramètres|loadcfg|-||||c\n"
    "button|SauverIndex|Sauver les index|saveindx|-||||c\n"
    "button|RAZIndex|Remise à zéro des index|indx0|-||||c|0\n";

// Decoupe la ligne i de la table dans line ; f[] pointe dans line (champs
// absents = chaine vide). Retourne false si i est hors table.
inline bool mqttEntity(size_t i, char *line, size_t lineSize, const char *f[MQTT_FIELD_COUNT]) {
  const char *p = MQTT_ENTITY_TABLE;
  size_t cur = 0;
  while (cur < i) {
    char c = (char)pgm_read_byte(p);
    if (c == '\0') return false;
    p++;
    if (c == '\n') cur++;
  }
  size_t n = 0;
  while (n + 1 < lineSize) {
    char c = (char)pgm_read_byte(p + n);
    if (c == '\0' || c == '\n') break;
    line[n++] = c;
  }
  if (n == 0) return false;
  line[n] = '\0';
  size_t k = 0;
  f[k++] = line;
  for (size_t j = 0; j < n && k < MQTT_FIELD_COUNT; j++) {
    if (line[j] == '|') {
      line[j] = '\0';
      f[k++] = line + j + 1;
    }
  }
  while (k < MQTT_FIELD_COUNT) f[k++] = line + n;  // chaine vide
  return true;
}

// ---------------------------------------------------------------------------
// Discovery (R5)
// ---------------------------------------------------------------------------

// homeassistant/<composant>/<id>/<id><suffixe>/config ; 0 si trop petit
inline size_t mqttDiscoveryTopic(char *buf, size_t size, size_t i, const char *id) {
  char line[MQTT_LINE_MAX];
  const char *f[MQTT_FIELD_COUNT];
  if (buf == NULL || size == 0 || !mqttEntity(i, line, sizeof line, f)) return 0;
  MqttWriter w = {buf, size, 0, true};
  mqttPutP(w, PSTR("homeassistant/"));
  mqttPut(w, f[0]);
  mqttPutP(w, PSTR("/"));
  mqttPut(w, id);
  mqttPutP(w, PSTR("/"));
  mqttPut(w, id);
  mqttPut(w, f[1]);
  mqttPutP(w, PSTR("/config"));
  return mqttEnd(w);
}

// ,"<cle>":"<valeur>" si la valeur n'est pas vide (cle fournie en PROGMEM)
inline void mqttOptStr(MqttWriter &w, const char *keyP, const char *val) {
  if (val[0] == '\0') return;
  mqttPutP(w, keyP);
  mqttPut(w, val);
  mqttPutP(w, PSTR("\""));
}

inline size_t mqttDiscoveryPayload(char *buf, size_t size, size_t i, const char *id, const char *ip) {
  char line[MQTT_LINE_MAX];
  const char *f[MQTT_FIELD_COUNT];
  if (buf == NULL || size == 0 || !mqttEntity(i, line, sizeof line, f)) return 0;
  const bool isButton = strcmp(f[0], "button") == 0;
  const bool isSelect = strcmp(f[0], "select") == 0;
  const bool isSwitch = strcmp(f[0], "switch") == 0;
  const bool isNumber = strcmp(f[0], "number") == 0;
  MqttWriter w = {buf, size, 0, true};

  mqttPutP(w, PSTR("{\"dev\":{\"ids\":\""));
  mqttPut(w, id);
  mqttPutP(w, PSTR("\",\"name\":\"MaxPV\",\"mdl\":\"MaxPV!\",\"mf\":\"JetBlack\",\"sw\":\""));
  mqttPutP(w, PSTR(MAXPV_VERSION));
  mqttPutP(w, PSTR("\",\"cu\":\"http://"));
  mqttPut(w, ip);
  mqttPutP(w, PSTR("/\"},\"avty_t\":\"maxpv/"));
  mqttPut(w, id);
  mqttPutP(w, PSTR("/status\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\",\"uniq_id\":\""));
  mqttPut(w, id);
  mqttPut(w, f[1]);
  mqttPutP(w, PSTR("\",\"name\":\""));
  mqttPut(w, f[2]);
  mqttPutP(w, PSTR("\""));
  mqttOptStr(w, PSTR(",\"dev_cla\":\""), f[5]);
  mqttOptStr(w, PSTR(",\"stat_cla\":\""), f[6]);
  mqttOptStr(w, PSTR(",\"unit_of_meas\":\""), f[7]);
  if (f[8][0] == 'd') mqttPutP(w, PSTR(",\"ent_cat\":\"diagnostic\""));
  else if (f[8][0] == 'c') mqttPutP(w, PSTR(",\"ent_cat\":\"config\""));
  if (f[9][0] == '0') mqttPutP(w, PSTR(",\"en\":false"));

  if (!isButton) {
    mqttPutP(w, PSTR(",\"stat_t\":\"maxpv/"));
    mqttPut(w, id);
    mqttPutP(w, f[4][0] == 'p' ? PSTR("/params\"") : PSTR("/state\""));
    if (isSelect) {  // codes Nano : 0 stop, 1 force, sinon auto
      mqttPutP(w, PSTR(",\"val_tpl\":\"{% if value_json."));
      mqttPut(w, f[3]);
      mqttPutP(w, PSTR(" == 0 %}stop{% elif value_json."));
      mqttPut(w, f[3]);
      mqttPutP(w, PSTR(" == 1 %}force{% else %}auto{% endif %}\""));
    } else if (isSwitch && f[4][0] == 'p') {  // reglage numerique 0/1
      mqttPutP(w, PSTR(",\"val_tpl\":\"{{ 'ON' if value_json."));
      mqttPut(w, f[3]);
      mqttPutP(w, PSTR(" == 1 else 'OFF' }}\""));
    } else {
      mqttPutP(w, PSTR(",\"val_tpl\":\"{{ value_json."));
      mqttPut(w, f[3]);
      mqttPutP(w, PSTR(" }}\""));
    }
  }
  if (isButton || isSelect || isSwitch || isNumber) {
    mqttPutP(w, PSTR(",\"cmd_t\":\"maxpv/"));
    mqttPut(w, id);
    mqttPutP(w, PSTR("/"));
    mqttPut(w, f[3]);
    mqttPutP(w, PSTR("/set\""));
  }
  if (isSelect) mqttPutP(w, PSTR(",\"options\":[\"force\",\"auto\",\"stop\"]"));
  if (isButton) mqttPutP(w, PSTR(",\"pl_prs\":\"PRESS\""));
  if (isNumber) {
    mqttPutP(w, PSTR(",\"mode\":\"box\",\"min\":"));
    mqttPut(w, f[10]);
    mqttPutP(w, PSTR(",\"max\":"));
    mqttPut(w, f[11]);
    mqttPutP(w, PSTR(",\"step\":"));
    mqttPut(w, f[12]);
  }
  mqttPutP(w, PSTR("}"));
  return mqttEnd(w);
}

// ---------------------------------------------------------------------------
// Etat et reglages en JSON (R4)
// ---------------------------------------------------------------------------

struct MqttState {
  double vrms, irms;
  long pact, papp, prouted, pimp, pexp;
  double cosphi;
  long pimpulsion;
  double idxRouted, idxImport, idxExport, idxImpulsion;
  long idxRelay;
  int triacMode, relayMode;  // codes Nano : 0 stop, 1 force, autre auto
  bool relayOn;
  double temp;
  bool boost, nano;
  long rssi;
  unsigned long uptime, heap;
  const char *resetReason;
  const char *ecopvVersion;
  long delayMin, delayAvg, delayMax, biasOffset, statusByte;
  unsigned long onTime, samples;
};

// Retourne la longueur ecrite, 0 si le buffer est trop petit (jamais de troncature)
inline size_t buildStateJson(char *buf, size_t size, const MqttState &s) {
  StaticJsonDocument<JSON_OBJECT_SIZE(32)> doc;
  doc["vrms"] = s.vrms;
  doc["irms"] = s.irms;
  doc["pact"] = s.pact;
  doc["papp"] = s.papp;
  doc["prouted"] = s.prouted;
  doc["pimp"] = s.pimp;
  doc["pexp"] = s.pexp;
  doc["cosphi"] = s.cosphi;
  doc["pimpulsion"] = s.pimpulsion;
  doc["idx_routed"] = s.idxRouted;
  doc["idx_import"] = s.idxImport;
  doc["idx_export"] = s.idxExport;
  doc["idx_impulsion"] = s.idxImpulsion;
  doc["idx_relay"] = s.idxRelay;
  doc["triac_mode"] = s.triacMode;
  doc["relay_mode"] = s.relayMode;
  doc["relay_on"] = s.relayOn ? "ON" : "OFF";
  doc["temp"] = s.temp;
  doc["boost"] = s.boost ? "ON" : "OFF";
  doc["nano"] = s.nano ? "ON" : "OFF";
  doc["rssi"] = s.rssi;
  doc["uptime"] = s.uptime;
  doc["heap"] = s.heap;
  doc["reset_reason"] = s.resetReason ? s.resetReason : "";
  doc["ecopv_version"] = s.ecopvVersion ? s.ecopvVersion : "";
  doc["delay_min"] = s.delayMin;
  doc["delay_avg"] = s.delayAvg;
  doc["delay_max"] = s.delayMax;
  doc["bias_offset"] = s.biasOffset;
  doc["status_byte"] = s.statusByte;
  doc["ontime"] = s.onTime;
  doc["samples"] = s.samples;
  if (doc.overflowed() || buf == NULL || measureJson(doc) >= size) return 0;
  return serializeJson(doc, buf, size);
}

// Cles des 16 parametres EcoPV, dans l'ordre des index V_CALIB .. P_INSTALLPV
static const char *const MQTT_PARAM_KEYS[16] = {
    "v_calib", "p_calib", "phase_calib", "p_offset", "p_resistance", "p_margin",
    "gain_p", "gain_i", "e_reserve", "p_div2_active", "p_div2_idle", "t_div2_on",
    "t_div2_off", "t_div2_tc", "cnt_calib", "p_installpv"};

struct MqttParams {
  const char *ecopv[16];
  long boostDuration, boostRatio, maxTemp, tempLimit, mqttPeriod;
};

inline size_t buildParamsJson(char *buf, size_t size, const MqttParams &p) {
  StaticJsonDocument<JSON_OBJECT_SIZE(21)> doc;
  for (int k = 0; k < 16; k++) doc[MQTT_PARAM_KEYS[k]] = p.ecopv[k] ? p.ecopv[k] : "";
  doc["boost_duration"] = p.boostDuration;
  doc["boost_ratio"] = p.boostRatio;
  doc["max_temp"] = p.maxTemp;
  doc["temp_limit"] = p.tempLimit;
  doc["mqtt_period"] = p.mqttPeriod;
  if (doc.overflowed() || buf == NULL || measureJson(doc) >= size) return 0;
  return serializeJson(doc, buf, size);
}

// ---------------------------------------------------------------------------
// Commandes (R6) : validation pure, l'action est executee par l'appelant
// ---------------------------------------------------------------------------

enum MqttAction {
  CMD_NONE,
  CMD_SSR_MODE,
  CMD_RELAY_MODE,
  CMD_BOOST_ON,
  CMD_BOOST_OFF,
  CMD_SET_PARAM,
  CMD_SET_ESP,
  CMD_RESTART_ESP,
  CMD_RESET_ECOPV,
  CMD_SAVECFG,
  CMD_LOADCFG,
  CMD_SAVEINDX,
  CMD_INDX0,
  CMD_FORMAT,
  CMD_ERASE_CONFIG_ESP
};

enum MqttMode { MQTT_MODE_STOP, MQTT_MODE_FORCE, MQTT_MODE_AUTO };

// Reglages ESP (MqttCommand.index). Les 4 premiers suivent l'ordre des numbers
// de la table (a partir du rang 16).
enum MqttEspSetting {
  MQTT_ESP_BOOST_DURATION,
  MQTT_ESP_BOOST_RATIO,
  MQTT_ESP_MAX_TEMP,
  MQTT_ESP_MQTT_PERIOD,
  MQTT_ESP_TEMP_LIMIT
};

struct MqttCmdContext {
  bool tempLimitActive;
  double temp;
  long maxTemp;
};

struct MqttCommand {
  MqttAction action;
  int mode;     // MqttMode (CMD_SSR_MODE, CMD_RELAY_MODE)
  int index;    // rang du parametre EcoPV (0 a 15) ou MqttEspSetting
  long number;  // valeur d'un reglage ESP
  char value[16];  // valeur d'un parametre EcoPV, telle que recue (validee)
};

// Bornes d'un number de la table, retrouve par sa cle. rank = rang parmi les numbers.
inline bool mqttNumberBounds(const char *key, int &rank, double &mn, double &mx) {
  char line[MQTT_LINE_MAX];
  const char *f[MQTT_FIELD_COUNT];
  int r = 0;
  for (size_t i = 0; i < MQTT_ENTITY_COUNT; i++) {
    if (!mqttEntity(i, line, sizeof line, f)) return false;
    if (strcmp(f[0], "number") != 0) continue;
    if (strcmp(f[3], key) == 0) {
      rank = r;
      mn = strtod(f[10], NULL);
      mx = strtod(f[11], NULL);
      return true;
    }
    r++;
  }
  return false;
}

inline bool handleCommand(const char *key, const char *value, bool fromHttp,
                          const MqttCmdContext &ctx, MqttCommand &out) {
  out.action = CMD_NONE;
  out.mode = 0;
  out.index = 0;
  out.number = 0;
  out.value[0] = '\0';
  if (key == NULL || value == NULL || key[0] == '\0') return false;
  if (strlen(value) > MQTT_PAYLOAD_MAX) return false;

  // selects : egalite exacte
  if (strcmp(key, "triac_mode") == 0 || strcmp(key, "relay_mode") == 0) {
    int mode;
    if (strcmp(value, "stop") == 0) mode = MQTT_MODE_STOP;
    else if (strcmp(value, "force") == 0) mode = MQTT_MODE_FORCE;
    else if (strcmp(value, "auto") == 0) mode = MQTT_MODE_AUTO;
    else return false;
    out.action = (key[0] == 't') ? CMD_SSR_MODE : CMD_RELAY_MODE;
    out.mode = mode;
    return true;
  }

  if (strcmp(key, "boost") == 0) {
    if (strcmp(value, "ON") == 0) {
      if (ctx.tempLimitActive && !(ctx.temp < (double)ctx.maxTemp)) return false;
      out.action = CMD_BOOST_ON;
      return true;
    }
    if (strcmp(value, "OFF") == 0) {
      out.action = CMD_BOOST_OFF;
      return true;
    }
    return false;
  }

  if (strcmp(key, "temp_limit") == 0) {
    if (strcmp(value, "ON") == 0) out.number = 1;
    else if (strcmp(value, "OFF") == 0) out.number = 0;
    else return false;
    out.action = CMD_SET_ESP;
    out.index = MQTT_ESP_TEMP_LIMIT;
    return true;
  }

  // boutons : payload PRESS
  struct Button {
    const char *key;
    MqttAction action;
    bool httpOnly;
  };
  static const Button buttons[] = {
      {"restartesp", CMD_RESTART_ESP, false}, {"resetecopv", CMD_RESET_ECOPV, false},
      {"savecfg", CMD_SAVECFG, false},        {"loadcfg", CMD_LOADCFG, false},
      {"saveindx", CMD_SAVEINDX, false},      {"indx0", CMD_INDX0, false},
      {"format", CMD_FORMAT, true},           {"eraseconfigesp", CMD_ERASE_CONFIG_ESP, true}};
  for (size_t b = 0; b < sizeof buttons / sizeof buttons[0]; b++) {
    if (strcmp(key, buttons[b].key) != 0) continue;
    if (buttons[b].httpOnly && !fromHttp) return false;
    if (strcmp(value, "PRESS") != 0) return false;
    out.action = buttons[b].action;
    return true;
  }

  // parametres EcoPV : valeur numerique sure pour la liaison serie, dans les bornes
  for (int k = 0; k < 16; k++) {
    if (strcmp(key, MQTT_PARAM_KEYS[k]) != 0) continue;
    int rank;
    double mn, mx;
    if (!isNumericParamValue(value) || !mqttNumberBounds(key, rank, mn, mx)) return false;
    double d = strtod(value, NULL);
    if (d < mn || d > mx) return false;
    out.action = CMD_SET_PARAM;
    out.index = k;
    strncpy(out.value, value, sizeof out.value - 1);
    out.value[sizeof out.value - 1] = '\0';
    return true;
  }

  // reglages ESP numeriques : entiers dans les bornes
  static const char *const espKeys[4] = {"boost_duration", "boost_ratio", "max_temp", "mqtt_period"};
  for (int k = 0; k < 4; k++) {
    if (strcmp(key, espKeys[k]) != 0) continue;
    int rank;
    double mn, mx;
    if (!isNumericParamValue(value) || strchr(value, '.') != NULL || !mqttNumberBounds(key, rank, mn, mx))
      return false;
    double d = strtod(value, NULL);
    if (d < mn || d > mx) return false;
    out.action = CMD_SET_ESP;
    out.index = k;
    out.number = (long)d;
    return true;
  }
  return false;
}
