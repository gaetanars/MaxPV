// Vérifie que JSON_CONFIG_SIZE accueille une configuration aux longueurs
// maximales de l'UI, en lecture (valeurs en chaînes, comme app.js) et
// en écriture (copies de String, comme configWrite()).
#include <ArduinoJson.h>
#include <string>
#include <vector>
#include <unity.h>
#include "maxpv_defines.h"

static const char *const IP_MAX = "255.255.255.255";

struct Entry {
  const char *key;
  std::string value;
};

static std::string repeat(char c, size_t n) { return std::string(n, c); }

// 16 clés de config.json (R2 de la SPEC ha-mqtt-ui), valeurs à longueur max :
// hôte MQTT 64, user 39, pass 39 avec &#+%"\, IP 15.
static std::vector<Entry> maxConfig() {
  return {
      {"ip", IP_MAX},
      {"gateway", IP_MAX},
      {"subnet", IP_MAX},
      {"dns1", IP_MAX},
      {"dns2", IP_MAX},
      {"watchdogwifi_active", "1"},
      {"watchdogmqtt_active", "1"},
      {"mqtt_ip", repeat('h', 64)},
      {"mqtt_port", "65535"},
      {"mqtt_user", repeat('u', 39)},
      {"mqtt_pass", "&#+%\"\\" + repeat('p', 33)},
      {"mqtt_period", "3600"},
      {"boost_duration", "600"},
      {"boost_ratio", "100"},
      {"max_temp", "90"},
      {"temp_limit_active", "1"},
  };
}

static const int MQTT_PASS_INDEX = 10;

// JSON_CONFIG_SIZE doit etre reduit (ancienne valeur : 2048 pour 29 cles ; 1024 max)
void test_config_size_reduced() {
  TEST_ASSERT_LESS_OR_EQUAL(1024, JSON_CONFIG_SIZE);  // natif 64 bits : ~870 utilises
  TEST_ASSERT_GREATER_THAN(0, JSON_CONFIG_SIZE);
}

void test_deserialize_max_config() {
  std::vector<Entry> cfg = maxConfig();
  TEST_ASSERT_EQUAL(16, cfg.size());
  TEST_ASSERT_EQUAL(39, cfg[MQTT_PASS_INDEX].value.size());

  // JSON tel que l'envoie l'UI (JSON.stringify), puis lu depuis un std::string
  DynamicJsonDocument src(8192);
  for (const Entry &e : cfg) src[e.key] = e.value;
  std::string payload;
  serializeJson(src, payload);

  DynamicJsonDocument doc(JSON_CONFIG_SIZE);
  DeserializationError err = deserializeJson(doc, payload);
  TEST_MESSAGE(("memoryUsage deserialize = " + std::to_string(doc.memoryUsage()) +
                " / " + std::to_string(JSON_CONFIG_SIZE)).c_str());
  TEST_ASSERT_EQUAL_STRING("Ok", err.c_str());
  TEST_ASSERT_EQUAL_STRING(cfg[MQTT_PASS_INDEX].value.c_str(), doc["mqtt_pass"].as<const char *>());
}

void test_write_roundtrip() {
  std::vector<Entry> cfg = maxConfig();

  DynamicJsonDocument doc(JSON_CONFIG_SIZE);
  for (const Entry &e : cfg) doc[e.key] = e.value;  // copie, comme String dans configWrite()
  TEST_MESSAGE(("memoryUsage write = " + std::to_string(doc.memoryUsage()) +
                " / " + std::to_string(JSON_CONFIG_SIZE)).c_str());
  TEST_ASSERT_FALSE(doc.overflowed());

  std::string out;
  serializeJson(doc, out);

  DynamicJsonDocument back(JSON_CONFIG_SIZE);
  DeserializationError err = deserializeJson(back, out);
  TEST_ASSERT_EQUAL_STRING("Ok", err.c_str());
  TEST_ASSERT_EQUAL_STRING(cfg[MQTT_PASS_INDEX].value.c_str(), back["mqtt_pass"].as<const char *>());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_config_size_reduced);
  RUN_TEST(test_deserialize_max_config);
  RUN_TEST(test_write_roundtrip);
  return UNITY_END();
}
