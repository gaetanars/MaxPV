// Tests natifs de MaxPV3/maxpv_mqtt.h (SPEC ha-mqtt-ui R4, R5, R6) :
// etat JSON dans un buffer fixe, discovery HA par table PROGMEM, handleCommand.
//
// API attendue de maxpv_mqtt.h (fixee par docs/specs/ha-mqtt-ui/plan.md) :
//   MQTT_ENTITY_COUNT, MQTT_DISCOVERY_BUF_SIZE, MQTT_STATE_BUF_SIZE,
//   MQTT_PARAMS_BUF_SIZE, MQTT_PAYLOAD_MAX (16)
//   size_t mqttDiscoveryTopic(char *buf, size_t size, size_t i, const char *id);
//   size_t mqttDiscoveryPayload(char *buf, size_t size, size_t i, const char *id, const char *ip);
//     -> longueur ecrite, 0 si le buffer est trop petit
//   struct MqttState, size_t buildStateJson(char *buf, size_t size, const MqttState &s);
//   struct MqttParams, size_t buildParamsJson(char *buf, size_t size, const MqttParams &p);
//   struct MqttCmdContext, struct MqttCommand,
//   bool handleCommand(const char *key, const char *value, bool fromHttp,
//                      const MqttCmdContext &ctx, MqttCommand &out);
#include <ArduinoJson.h>
#include <string.h>
#include <string>
#include <unity.h>
#include "maxpv_defines.h"
#include "maxpv_mqtt.h"

static const char *const ID = "maxpv16777215";  // chipId max
static const char *const IP = "255.255.255.255";

static const char *const PARAM_KEYS[16] = {
    "v_calib", "p_calib", "phase_calib", "p_offset", "p_resistance", "p_margin",
    "gain_p", "gain_i", "e_reserve", "p_div2_active", "p_div2_idle", "t_div2_on",
    "t_div2_off", "t_div2_tc", "cnt_calib", "p_installpv"};
static const char *const PARAM_SUFFIX[16] = {
    "V_CALIB", "P_CALIB", "PHASE_CALIB", "P_OFFSET", "P_RESISTANCE", "P_MARGIN",
    "GAIN_P", "GAIN_I", "E_RESERVE", "P_DIV2_ACTIVE", "P_DIV2_IDLE", "T_DIV2_ON",
    "T_DIV2_OFF", "T_DIV2_TC", "CNT_CALIB", "P_INSTALLPV"};
static const double PARAM_MIN[16] = {0, 0.0001, -16, -100, 100, -2000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 100};
static const double PARAM_MAX[16] = {2, 1, 48, 100, 10000, 2000, 1000, 1000, 200, 9999, 9999, 240, 240, 60, 1000, 30000};

struct Existing {
  const char *suffix;
  const char *component;
};
static const Existing EXISTING[18] = {
    {"Tension", "sensor"}, {"Courant", "sensor"}, {"PuissanceApparente", "sensor"},
    {"FacteurPuissance", "sensor"}, {"PuissanceActive", "sensor"}, {"PuissanceRoutee", "sensor"},
    {"PuissanceImportee", "sensor"}, {"PuissanceExportee", "sensor"}, {"PuissanceProduite", "sensor"},
    {"Temperature", "sensor"}, {"EnergieRoutee", "sensor"}, {"EnergieImportee", "sensor"},
    {"EnergieExportee", "sensor"}, {"EnergieProduite", "sensor"}, {"TempsFonctionnementRelais", "sensor"},
    {"SSR", "select"}, {"Relais", "select"}, {"Boost", "switch"}};

static const char *const DISABLED[10] = {"MemoireLibre", "RaisonReset", "DelayMin", "DelayAvg",
                                         "DelayMax", "BiasOffset", "StatusByte", "OnTime",
                                         "Samples", "RAZIndex"};

// ---- helpers ----
static std::string topicOf(size_t i) {
  char buf[MQTT_DISCOVERY_BUF_SIZE];
  size_t n = mqttDiscoveryTopic(buf, sizeof buf, i, ID);
  return std::string(buf, n);
}

static bool payloadOf(size_t i, DynamicJsonDocument &doc, std::string &raw) {
  char buf[MQTT_DISCOVERY_BUF_SIZE];
  size_t n = mqttDiscoveryPayload(buf, sizeof buf, i, ID, IP);
  if (n == 0 || n >= sizeof buf) return false;
  raw.assign(buf, n);
  return deserializeJson(doc, raw) == DeserializationError::Ok;
}

// composant extrait de homeassistant/<component>/<id>/<id><suffixe>/config
static std::string componentOf(const std::string &topic) {
  const std::string pre = "homeassistant/";
  size_t a = pre.size();
  return topic.substr(a, topic.find('/', a) - a);
}

static int findBySuffix(const char *suffix) {
  std::string want = std::string(ID) + suffix;
  for (size_t i = 0; i < MQTT_ENTITY_COUNT; i++) {
    DynamicJsonDocument doc(2048);
    std::string raw;
    if (payloadOf(i, doc, raw) && want == (doc["uniq_id"] | "")) return (int)i;
  }
  return -1;
}

// ---- R5 discovery ----
void test_entity_count() {
  // 27 sensors + 2 binary_sensors + 2 selects + 2 switchs + 20 numbers + 6 buttons
  TEST_ASSERT_EQUAL(59, MQTT_ENTITY_COUNT);
}

void test_every_payload_is_valid_json_and_fits() {
  for (size_t i = 0; i < MQTT_ENTITY_COUNT; i++) {
    DynamicJsonDocument doc(2048);
    std::string raw;
    char msg[64];
    snprintf(msg, sizeof msg, "entite %u", (unsigned)i);
    TEST_ASSERT_TRUE_MESSAGE(payloadOf(i, doc, raw), msg);
    TEST_ASSERT_FALSE_MESSAGE(doc.overflowed(), msg);
    // topic coherent avec uniq_id et composant
    std::string topic = topicOf(i);
    std::string uniq = doc["uniq_id"] | "";
    TEST_ASSERT_EQUAL_STRING_MESSAGE(("homeassistant/" + componentOf(topic) + "/" + ID + "/" + uniq + "/config").c_str(),
                                     topic.c_str(), msg);
    TEST_ASSERT_EQUAL(0, uniq.find(ID));
    TEST_ASSERT_TRUE_MESSAGE(uniq.size() > strlen(ID), msg);
    // availability, device
    const std::string avty = std::string("maxpv/") + ID + "/status";
    TEST_ASSERT_EQUAL_STRING_MESSAGE(avty.c_str(), (doc["avty_t"] | ""), msg);
    TEST_ASSERT_EQUAL_STRING(ID, doc["dev"]["ids"] | "");
    TEST_ASSERT_EQUAL_STRING("MaxPV", doc["dev"]["name"] | "");
    TEST_ASSERT_EQUAL_STRING("MaxPV!", doc["dev"]["mdl"] | "");
    TEST_ASSERT_EQUAL_STRING(MAXPV_VERSION, doc["dev"]["sw"] | "");
    TEST_ASSERT_EQUAL_STRING((std::string("http://") + IP + "/").c_str(), doc["dev"]["cu"] | "");
    TEST_ASSERT_TRUE(doc.containsKey("name"));
    // aucun secret
    TEST_ASSERT_TRUE(raw.find("mqtt_pass") == std::string::npos);
  }
}

void test_state_topic_and_template() {
  const std::string state = std::string("maxpv/") + ID + "/state";
  const std::string params = std::string("maxpv/") + ID + "/params";
  for (size_t i = 0; i < MQTT_ENTITY_COUNT; i++) {
    DynamicJsonDocument doc(2048);
    std::string raw;
    TEST_ASSERT_TRUE(payloadOf(i, doc, raw));
    std::string comp = componentOf(topicOf(i));
    if (comp == "button") continue;  // pas d'etat
    std::string st = doc["stat_t"] | "";
    TEST_ASSERT_TRUE_MESSAGE(st == state || st == params, raw.c_str());
    std::string tpl = doc["val_tpl"] | "";
    TEST_ASSERT_TRUE_MESSAGE(tpl.find("value_json.") != std::string::npos, raw.c_str());
    if (comp == "number") TEST_ASSERT_EQUAL_STRING(params.c_str(), st.c_str());
    if (comp == "select") {  // mapping 0 stop, 1 force, sinon auto
      TEST_ASSERT_TRUE(tpl.find("stop") != std::string::npos);
      TEST_ASSERT_TRUE(tpl.find("force") != std::string::npos);
      TEST_ASSERT_TRUE(tpl.find("auto") != std::string::npos);
    }
  }
}

void test_existing_uniq_ids_and_components_kept() {
  for (const Existing &e : EXISTING) {
    int i = findBySuffix(e.suffix);
    TEST_ASSERT_TRUE_MESSAGE(i >= 0, e.suffix);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(e.component, componentOf(topicOf((size_t)i)).c_str(), e.suffix);
  }
  // device_class / unites historiques
  DynamicJsonDocument d(2048);
  std::string raw;
  TEST_ASSERT_TRUE(payloadOf((size_t)findBySuffix("Tension"), d, raw));
  TEST_ASSERT_EQUAL_STRING("voltage", d["dev_cla"] | "");
  TEST_ASSERT_EQUAL_STRING("measurement", d["stat_cla"] | "");
  TEST_ASSERT_EQUAL_STRING("V", d["unit_of_meas"] | "");
  DynamicJsonDocument e(2048);
  TEST_ASSERT_TRUE(payloadOf((size_t)findBySuffix("EnergieRoutee"), e, raw));
  TEST_ASSERT_EQUAL_STRING("energy", e["dev_cla"] | "");
  TEST_ASSERT_EQUAL_STRING("total_increasing", e["stat_cla"] | "");
}

void test_new_entities_present() {
  static const char *const SENSORS[] = {"RSSI", "Uptime", "VersionEcoPV", "MemoireLibre", "RaisonReset",
                                        "DelayMin", "DelayAvg", "DelayMax", "BiasOffset", "StatusByte",
                                        "OnTime", "Samples"};
  for (const char *s : SENSORS) {
    int i = findBySuffix(s);
    TEST_ASSERT_TRUE_MESSAGE(i >= 0, s);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("sensor", componentOf(topicOf((size_t)i)).c_str(), s);
  }
  TEST_ASSERT_EQUAL_STRING("binary_sensor", componentOf(topicOf((size_t)findBySuffix("EtatRelais"))).c_str());
  TEST_ASSERT_EQUAL_STRING("binary_sensor", componentOf(topicOf((size_t)findBySuffix("ContactRouteur"))).c_str());
  TEST_ASSERT_EQUAL_STRING("switch", componentOf(topicOf((size_t)findBySuffix("LimiteTemperature"))).c_str());
  static const char *const BUTTONS[] = {"RedemarrerESP", "RedemarrerRouteur", "SauverParametres",
                                        "RechargerParametres", "SauverIndex", "RAZIndex"};
  for (const char *s : BUTTONS) {
    int i = findBySuffix(s);
    TEST_ASSERT_TRUE_MESSAGE(i >= 0, s);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("button", componentOf(topicOf((size_t)i)).c_str(), s);
  }
  DynamicJsonDocument d(2048);
  std::string raw;
  TEST_ASSERT_TRUE(payloadOf((size_t)findBySuffix("RSSI"), d, raw));
  TEST_ASSERT_EQUAL_STRING("signal_strength", d["dev_cla"] | "");
  TEST_ASSERT_EQUAL_STRING("dBm", d["unit_of_meas"] | "");
  TEST_ASSERT_TRUE(payloadOf((size_t)findBySuffix("Uptime"), d, raw));
  TEST_ASSERT_EQUAL_STRING("duration", d["dev_cla"] | "");
  TEST_ASSERT_TRUE(payloadOf((size_t)findBySuffix("EtatRelais"), d, raw));
  TEST_ASSERT_EQUAL_STRING("power", d["dev_cla"] | "");
  TEST_ASSERT_TRUE(payloadOf((size_t)findBySuffix("ContactRouteur"), d, raw));
  TEST_ASSERT_EQUAL_STRING("connectivity", d["dev_cla"] | "");
  TEST_ASSERT_EQUAL_STRING("diagnostic", d["ent_cat"] | "");
}

void test_disabled_by_default() {
  int disabled = 0;
  for (size_t i = 0; i < MQTT_ENTITY_COUNT; i++) {
    DynamicJsonDocument doc(2048);
    std::string raw;
    TEST_ASSERT_TRUE(payloadOf(i, doc, raw));
    if (doc.containsKey("en") && doc["en"].as<bool>() == false) disabled++;
  }
  TEST_ASSERT_EQUAL(10, disabled);
  for (const char *s : DISABLED) {
    DynamicJsonDocument doc(2048);
    std::string raw;
    TEST_ASSERT_TRUE(payloadOf((size_t)findBySuffix(s), doc, raw));
    TEST_ASSERT_TRUE_MESSAGE(doc.containsKey("en") && !doc["en"].as<bool>(), s);
  }
}

void test_number_entities_bounds_and_step() {
  for (int k = 0; k < 16; k++) {
    int i = findBySuffix(PARAM_SUFFIX[k]);
    TEST_ASSERT_TRUE_MESSAGE(i >= 0, PARAM_SUFFIX[k]);
    DynamicJsonDocument doc(2048);
    std::string raw;
    TEST_ASSERT_TRUE(payloadOf((size_t)i, doc, raw));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("number", componentOf(topicOf((size_t)i)).c_str(), PARAM_SUFFIX[k]);
    TEST_ASSERT_EQUAL_STRING("box", doc["mode"] | "");
    TEST_ASSERT_EQUAL_STRING("config", doc["ent_cat"] | "");
    TEST_ASSERT_EQUAL_DOUBLE(PARAM_MIN[k], doc["min"].as<double>());
    TEST_ASSERT_EQUAL_DOUBLE(PARAM_MAX[k], doc["max"].as<double>());
    bool fine = (k == 0 || k == 1 || k == 14);  // V_CALIB, P_CALIB, CNT_CALIB
    TEST_ASSERT_EQUAL_DOUBLE(fine ? 0.0001 : 1.0, doc["step"].as<double>());
    TEST_ASSERT_EQUAL_STRING((std::string("{{ value_json.") + PARAM_KEYS[k] + " }}").c_str(), doc["val_tpl"] | "");
    TEST_ASSERT_EQUAL_STRING((std::string("maxpv/") + ID + "/" + PARAM_KEYS[k] + "/set").c_str(), doc["cmd_t"] | "");
  }
  struct { const char *s; double mn, mx; } esp[] = {{"BoostDuree", 10, 600}, {"BoostRatio", 0, 100},
                                                     {"TempMax", 15, 90}, {"PeriodeMQTT", 1, 3600}};
  for (auto &e : esp) {
    DynamicJsonDocument doc(2048);
    std::string raw;
    TEST_ASSERT_TRUE_MESSAGE(findBySuffix(e.s) >= 0, e.s);
    TEST_ASSERT_TRUE(payloadOf((size_t)findBySuffix(e.s), doc, raw));
    TEST_ASSERT_EQUAL_DOUBLE(e.mn, doc["min"].as<double>());
    TEST_ASSERT_EQUAL_DOUBLE(e.mx, doc["max"].as<double>());
    TEST_ASSERT_EQUAL_STRING("box", doc["mode"] | "");
  }
  int numbers = 0;
  for (size_t i = 0; i < MQTT_ENTITY_COUNT; i++)
    if (componentOf(topicOf(i)) == "number") numbers++;
  TEST_ASSERT_EQUAL(20, numbers);
}

void test_small_buffer_is_refused() {
  char tiny[24];
  TEST_ASSERT_EQUAL(0, mqttDiscoveryPayload(tiny, sizeof tiny, 0, ID, IP));
  TEST_ASSERT_EQUAL(0, mqttDiscoveryTopic(tiny, 10, 0, ID));
}

// Chaque entite commandable : sa cmd_t doit etre acceptee par handleCommand
void test_discovery_commands_are_handled() {
  const std::string pre = std::string("maxpv/") + ID + "/";
  MqttCmdContext ctx{false, 20.0, 45};
  int cmds = 0;
  for (size_t i = 0; i < MQTT_ENTITY_COUNT; i++) {
    DynamicJsonDocument doc(2048);
    std::string raw;
    TEST_ASSERT_TRUE(payloadOf(i, doc, raw));
    if (!doc.containsKey("cmd_t")) continue;
    cmds++;
    std::string t = doc["cmd_t"] | "";
    TEST_ASSERT_EQUAL(0, t.find(pre));
    TEST_ASSERT_TRUE(t.size() > pre.size() + 4 && t.substr(t.size() - 4) == "/set");
    std::string key = t.substr(pre.size(), t.size() - pre.size() - 4);
    std::string comp = componentOf(topicOf(i));
    std::string value;
    if (comp == "button") value = "PRESS";
    else if (comp == "switch") value = "ON";
    else if (comp == "select") value = "auto";
    else {
      char mn[24];
      snprintf(mn, sizeof mn, "%g", doc["min"].as<double>());
      value = mn;
    }
    MqttCommand out;
    memset(&out, 0, sizeof out);
    TEST_ASSERT_TRUE_MESSAGE(handleCommand(key.c_str(), value.c_str(), false, ctx, out), raw.c_str());
  }
  TEST_ASSERT_EQUAL(30, cmds);  // 2 selects + 2 switchs + 20 numbers + 6 buttons
}

// ---- R4 etat et params ----
static void fillMaxState(MqttState &s) {
  s.vrms = 999.99; s.irms = 99.99; s.pact = -2147483647L; s.papp = -2147483647L;
  s.prouted = -2147483647L; s.pimp = -2147483647L; s.pexp = -2147483647L;
  s.cosphi = -1.00; s.pimpulsion = -2147483647L;
  s.idxRouted = 99999999.99; s.idxImport = 99999999.99; s.idxExport = 99999999.99;
  s.idxImpulsion = 99999999.99; s.idxRelay = 2147483647L;
  s.triacMode = 2; s.relayMode = 2; s.relayOn = true; s.temp = -127.00;
  s.boost = true; s.nano = true; s.rssi = -100; s.uptime = 4294967295UL;
  s.heap = 4294967295UL; s.resetReason = "Hardware Watchdog 1234567890";
  s.ecopvVersion = "3.99.99-abcdefg"; s.delayMin = -2147483647L; s.delayAvg = -2147483647L;
  s.delayMax = -2147483647L; s.biasOffset = -2147483647L; s.statusByte = 255;
  s.onTime = 4294967295UL; s.samples = 4294967295UL;
}

void test_state_json_max_lengths_fit() {
  MqttState s;
  memset(&s, 0, sizeof s);
  fillMaxState(s);
  char buf[MQTT_STATE_BUF_SIZE];
  size_t n = buildStateJson(buf, sizeof buf, s);
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_TRUE(n < sizeof buf);
  DynamicJsonDocument doc(2048);
  TEST_ASSERT_EQUAL_STRING("Ok", deserializeJson(doc, buf, n).c_str());
  static const char *const KEYS[] = {
      "vrms", "irms", "pact", "papp", "prouted", "pimp", "pexp", "cosphi", "pimpulsion",
      "idx_routed", "idx_import", "idx_export", "idx_impulsion", "idx_relay", "triac_mode",
      "relay_mode", "relay_on", "temp", "boost", "nano", "rssi", "uptime", "heap",
      "reset_reason", "ecopv_version", "delay_min", "delay_avg", "delay_max", "bias_offset",
      "status_byte", "ontime", "samples"};
  for (const char *k : KEYS) TEST_ASSERT_TRUE_MESSAGE(doc.containsKey(k), k);
  TEST_ASSERT_EQUAL(32, doc.size());
  TEST_ASSERT_EQUAL_STRING("ON", doc["relay_on"] | "");
  TEST_ASSERT_EQUAL_STRING("ON", doc["boost"] | "");
  TEST_ASSERT_EQUAL_STRING("ON", doc["nano"] | "");
}

void test_state_json_off_and_too_small() {
  MqttState s;
  memset(&s, 0, sizeof s);
  s.resetReason = "";
  s.ecopvVersion = "";
  char buf[MQTT_STATE_BUF_SIZE];
  TEST_ASSERT_TRUE(buildStateJson(buf, sizeof buf, s) > 0);
  DynamicJsonDocument doc(2048);
  TEST_ASSERT_EQUAL_STRING("Ok", deserializeJson(doc, buf).c_str());
  TEST_ASSERT_EQUAL_STRING("OFF", doc["relay_on"] | "");
  TEST_ASSERT_EQUAL_STRING("OFF", doc["boost"] | "");
  TEST_ASSERT_EQUAL_STRING("OFF", doc["nano"] | "");
  fillMaxState(s);
  char tiny[32];
  TEST_ASSERT_EQUAL(0, buildStateJson(tiny, sizeof tiny, s));  // refuse plutot que tronquer
}

void test_params_json() {
  MqttParams p;
  memset(&p, 0, sizeof p);
  static const char *const VAL = "-123456789012.5";  // 15 car.
  for (int k = 0; k < 16; k++) p.ecopv[k] = VAL;
  p.boostDuration = 600; p.boostRatio = 100; p.maxTemp = 90; p.tempLimit = 1; p.mqttPeriod = 3600;
  char buf[MQTT_PARAMS_BUF_SIZE];
  size_t n = buildParamsJson(buf, sizeof buf, p);
  TEST_ASSERT_TRUE(n > 0 && n < sizeof buf);
  DynamicJsonDocument doc(2048);
  TEST_ASSERT_EQUAL_STRING("Ok", deserializeJson(doc, buf, n).c_str());
  for (int k = 0; k < 16; k++) TEST_ASSERT_TRUE_MESSAGE(doc.containsKey(PARAM_KEYS[k]), PARAM_KEYS[k]);
  TEST_ASSERT_EQUAL(600, doc["boost_duration"].as<int>());
  TEST_ASSERT_EQUAL(100, doc["boost_ratio"].as<int>());
  TEST_ASSERT_EQUAL(90, doc["max_temp"].as<int>());
  TEST_ASSERT_EQUAL(1, doc["temp_limit"].as<int>());
  TEST_ASSERT_EQUAL(3600, doc["mqtt_period"].as<int>());
  TEST_ASSERT_EQUAL(21, doc.size());
}

// ---- R6 handleCommand ----
static bool cmd(const char *key, const char *value, bool http, MqttCommand &out,
                bool limit = false, double temp = 20.0, long maxT = 45,
                bool ssrStop = false, bool boostActive = false) {
  MqttCmdContext ctx{limit, temp, maxT, ssrStop, boostActive};
  memset(&out, 0, sizeof out);
  out.action = CMD_NONE;
  return handleCommand(key, value, http, ctx, out);
}

void test_modes_exact_match() {
  MqttCommand o;
  TEST_ASSERT_TRUE(cmd("triac_mode", "stop", false, o));
  TEST_ASSERT_EQUAL(CMD_SSR_MODE, o.action);
  TEST_ASSERT_EQUAL(MQTT_MODE_STOP, o.mode);
  TEST_ASSERT_TRUE(cmd("triac_mode", "force", false, o));
  TEST_ASSERT_EQUAL(MQTT_MODE_FORCE, o.mode);
  TEST_ASSERT_TRUE(cmd("triac_mode", "auto", false, o));
  TEST_ASSERT_EQUAL(MQTT_MODE_AUTO, o.mode);
  TEST_ASSERT_TRUE(cmd("relay_mode", "force", true, o));
  TEST_ASSERT_EQUAL(CMD_RELAY_MODE, o.action);
  TEST_ASSERT_EQUAL(MQTT_MODE_FORCE, o.mode);
  const char *bad[] = {"STOP", "Stop", "stopx", "stop ", " stop", "sto", "", "1", "forc", "auto\n", "stopforce"};
  for (const char *b : bad) {
    TEST_ASSERT_FALSE_MESSAGE(cmd("triac_mode", b, false, o), b);
    TEST_ASSERT_FALSE_MESSAGE(cmd("relay_mode", b, true, o), b);
  }
}

void test_boost() {
  MqttCommand o;
  TEST_ASSERT_TRUE(cmd("boost", "ON", false, o));
  TEST_ASSERT_EQUAL(CMD_BOOST_ON, o.action);
  TEST_ASSERT_TRUE(cmd("boost", "OFF", false, o));
  TEST_ASSERT_EQUAL(CMD_BOOST_OFF, o.action);
  TEST_ASSERT_FALSE(cmd("boost", "on", false, o));
  TEST_ASSERT_FALSE(cmd("boost", "ONN", false, o));
  TEST_ASSERT_FALSE(cmd("boost", "", false, o));
  // limite de temperature active : refuse si temp >= max_temp, accepte en dessous
  TEST_ASSERT_FALSE(cmd("boost", "ON", false, o, true, 45.0, 45));
  TEST_ASSERT_FALSE(cmd("boost", "ON", false, o, true, 80.0, 45));
  TEST_ASSERT_TRUE(cmd("boost", "ON", false, o, true, 44.9, 45));
  // limite inactive : accepte meme chaud
  TEST_ASSERT_TRUE(cmd("boost", "ON", false, o, false, 80.0, 45));
  // OFF toujours accepte
  TEST_ASSERT_TRUE(cmd("boost", "OFF", false, o, true, 80.0, 45));
}

void test_boost_ssr_stop() {
  MqttCommand o;
  // SSR en STOP sans BOOST en cours : ON refuse
  TEST_ASSERT_FALSE(cmd("boost", "ON", false, o, false, 20.0, 45, true, false));
  TEST_ASSERT_FALSE(cmd("boost", "ON", true, o, false, 20.0, 45, true, false));
  // BOOST en cours : relancable
  TEST_ASSERT_TRUE(cmd("boost", "ON", false, o, false, 20.0, 45, true, true));
  // OFF toujours accepte
  TEST_ASSERT_TRUE(cmd("boost", "OFF", false, o, false, 20.0, 45, true, false));
}

void test_boost_sonde_defaut() {
  MqttCommand o;
  // limite active : sonde absente (-99) ou valeur vide (0) = refus
  TEST_ASSERT_FALSE(cmd("boost", "ON", false, o, true, -99.0, 45));
  TEST_ASSERT_FALSE(cmd("boost", "ON", false, o, true, 0.0, 45));
  TEST_ASSERT_TRUE(cmd("boost", "ON", false, o, true, 20.0, 45));
  // limite inactive : accepte
  TEST_ASSERT_TRUE(cmd("boost", "ON", false, o, false, -99.0, 45));
}

void test_p_calib_positive() {
  MqttCommand o;
  TEST_ASSERT_FALSE(cmd("p_calib", "0", false, o));
  TEST_ASSERT_FALSE(cmd("p_calib", "0", true, o));
  TEST_ASSERT_TRUE(cmd("p_calib", "0.0001", false, o));
  TEST_ASSERT_TRUE(cmd("p_calib", "0.111", false, o));
}

void test_ecopv_params() {
  MqttCommand o;
  for (int k = 0; k < 16; k++) {
    char lo[24], hi[24];
    snprintf(lo, sizeof lo, "%g", PARAM_MIN[k]);
    snprintf(hi, sizeof hi, "%g", PARAM_MAX[k]);
    TEST_ASSERT_TRUE_MESSAGE(cmd(PARAM_KEYS[k], lo, false, o), PARAM_KEYS[k]);
    TEST_ASSERT_EQUAL(CMD_SET_PARAM, o.action);
    TEST_ASSERT_EQUAL(k, o.index);
    TEST_ASSERT_EQUAL_STRING(lo, o.value);
    TEST_ASSERT_TRUE_MESSAGE(cmd(PARAM_KEYS[k], hi, true, o), PARAM_KEYS[k]);
    TEST_ASSERT_EQUAL_STRING(hi, o.value);
    // hors bornes : refuse (hypothese : rejet, pas de clamp, cf. plan.md)
    char below[24], above[24];
    snprintf(below, sizeof below, "%g", PARAM_MIN[k] - 1);
    snprintf(above, sizeof above, "%g", PARAM_MAX[k] + 1);
    TEST_ASSERT_FALSE_MESSAGE(cmd(PARAM_KEYS[k], below, false, o), PARAM_KEYS[k]);
    TEST_ASSERT_FALSE_MESSAGE(cmd(PARAM_KEYS[k], above, false, o), PARAM_KEYS[k]);
  }
  TEST_ASSERT_TRUE(cmd("v_calib", "1.2345", false, o));
  TEST_ASSERT_EQUAL_STRING("1.2345", o.value);
  // injection serie et formats invalides
  const char *bad[] = {"5#FORMAT", "1,END#", "1,2", "#", ",", "abc", "", "-", ".", "1e2", " 5",
                       "1.2.3", "--1", "5;", "1234567890123456"};
  for (const char *b : bad) {
    TEST_ASSERT_FALSE_MESSAGE(cmd("gain_p", b, false, o), b);
    TEST_ASSERT_FALSE_MESSAGE(cmd("gain_p", b, true, o), b);
  }
  TEST_ASSERT_FALSE(cmd("unknown_key", "1", false, o));
  TEST_ASSERT_FALSE(cmd("", "1", false, o));
  TEST_ASSERT_FALSE(cmd("gain_p/set", "1", false, o));
  TEST_ASSERT_FALSE(cmd(nullptr, "1", false, o));
  TEST_ASSERT_FALSE(cmd("gain_p", nullptr, false, o));
}

void test_esp_settings() {
  MqttCommand o;
  struct { const char *key; int id; long mn, mx; } t[] = {
      {"boost_duration", MQTT_ESP_BOOST_DURATION, 10, 600}, {"boost_ratio", MQTT_ESP_BOOST_RATIO, 0, 100},
      {"max_temp", MQTT_ESP_MAX_TEMP, 15, 90}, {"mqtt_period", MQTT_ESP_MQTT_PERIOD, 1, 3600}};
  for (auto &e : t) {
    char lo[16], hi[16], below[16], above[16];
    snprintf(lo, sizeof lo, "%ld", e.mn);
    snprintf(hi, sizeof hi, "%ld", e.mx);
    snprintf(below, sizeof below, "%ld", e.mn - 1);
    snprintf(above, sizeof above, "%ld", e.mx + 1);
    TEST_ASSERT_TRUE_MESSAGE(cmd(e.key, lo, false, o), e.key);
    TEST_ASSERT_EQUAL(CMD_SET_ESP, o.action);
    TEST_ASSERT_EQUAL(e.id, o.index);
    TEST_ASSERT_EQUAL(e.mn, o.number);
    TEST_ASSERT_TRUE_MESSAGE(cmd(e.key, hi, true, o), e.key);
    TEST_ASSERT_EQUAL(e.mx, o.number);
    TEST_ASSERT_FALSE_MESSAGE(cmd(e.key, below, false, o), e.key);
    TEST_ASSERT_FALSE_MESSAGE(cmd(e.key, above, false, o), e.key);
    TEST_ASSERT_FALSE_MESSAGE(cmd(e.key, "12,3", false, o), e.key);
    TEST_ASSERT_FALSE_MESSAGE(cmd(e.key, "abc", false, o), e.key);
  }
  TEST_ASSERT_TRUE(cmd("temp_limit", "ON", false, o));
  TEST_ASSERT_EQUAL(CMD_SET_ESP, o.action);
  TEST_ASSERT_EQUAL(MQTT_ESP_TEMP_LIMIT, o.index);
  TEST_ASSERT_EQUAL(1, o.number);
  TEST_ASSERT_TRUE(cmd("temp_limit", "OFF", false, o));
  TEST_ASSERT_EQUAL(0, o.number);
  TEST_ASSERT_FALSE(cmd("temp_limit", "on", false, o));
  TEST_ASSERT_FALSE(cmd("temp_limit", "1", false, o));
}

void test_buttons_press_only() {
  struct { const char *key; MqttAction a; } t[] = {
      {"restartesp", CMD_RESTART_ESP}, {"resetecopv", CMD_RESET_ECOPV}, {"savecfg", CMD_SAVECFG},
      {"loadcfg", CMD_LOADCFG}, {"saveindx", CMD_SAVEINDX}, {"indx0", CMD_INDX0}};
  MqttCommand o;
  for (auto &e : t) {
    TEST_ASSERT_TRUE_MESSAGE(cmd(e.key, "PRESS", false, o), e.key);
    TEST_ASSERT_EQUAL(e.a, o.action);
    TEST_ASSERT_TRUE_MESSAGE(cmd(e.key, "PRESS", true, o), e.key);
    TEST_ASSERT_FALSE_MESSAGE(cmd(e.key, "press", false, o), e.key);
    TEST_ASSERT_FALSE_MESSAGE(cmd(e.key, "PRESSED", false, o), e.key);
    TEST_ASSERT_FALSE_MESSAGE(cmd(e.key, "", false, o), e.key);
    TEST_ASSERT_FALSE_MESSAGE(cmd(e.key, "ON", false, o), e.key);
  }
}

void test_destructive_actions_http_only() {
  MqttCommand o;
  TEST_ASSERT_FALSE(cmd("format", "PRESS", false, o));
  TEST_ASSERT_FALSE(cmd("eraseconfigesp", "PRESS", false, o));
  TEST_ASSERT_TRUE(cmd("format", "PRESS", true, o));
  TEST_ASSERT_EQUAL(CMD_FORMAT, o.action);
  TEST_ASSERT_TRUE(cmd("eraseconfigesp", "PRESS", true, o));
  TEST_ASSERT_EQUAL(CMD_ERASE_CONFIG_ESP, o.action);
  TEST_ASSERT_FALSE(cmd("format", "press", true, o));
  TEST_ASSERT_FALSE(cmd("format", "", true, o));
  TEST_ASSERT_FALSE(cmd("eraseconfigesp", "ON", true, o));
}

void test_payload_too_long_is_refused() {
  MqttCommand o;
  // MQTT_PAYLOAD_MAX = 16 : 17 caracteres refuses meme pour une valeur par ailleurs valide
  TEST_ASSERT_EQUAL(16, MQTT_PAYLOAD_MAX);
  TEST_ASSERT_FALSE(cmd("gain_p", "00000000000000000", false, o));
  TEST_ASSERT_FALSE(cmd("triac_mode", "autoautoautoautoauto", false, o));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_entity_count);
  RUN_TEST(test_every_payload_is_valid_json_and_fits);
  RUN_TEST(test_state_topic_and_template);
  RUN_TEST(test_existing_uniq_ids_and_components_kept);
  RUN_TEST(test_new_entities_present);
  RUN_TEST(test_disabled_by_default);
  RUN_TEST(test_number_entities_bounds_and_step);
  RUN_TEST(test_small_buffer_is_refused);
  RUN_TEST(test_discovery_commands_are_handled);
  RUN_TEST(test_state_json_max_lengths_fit);
  RUN_TEST(test_state_json_off_and_too_small);
  RUN_TEST(test_params_json);
  RUN_TEST(test_modes_exact_match);
  RUN_TEST(test_boost);
  RUN_TEST(test_boost_ssr_stop);
  RUN_TEST(test_boost_sonde_defaut);
  RUN_TEST(test_p_calib_positive);
  RUN_TEST(test_ecopv_params);
  RUN_TEST(test_esp_settings);
  RUN_TEST(test_buttons_press_only);
  RUN_TEST(test_destructive_actions_http_only);
  RUN_TEST(test_payload_too_long_is_refused);
  return UNITY_END();
}
