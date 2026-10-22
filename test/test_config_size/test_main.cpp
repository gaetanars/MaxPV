// Vérifie que JSON_CONFIG_SIZE accueille une configuration aux longueurs
// maximales de l'UI, en lecture (valeurs en chaînes, comme admin.html) et
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

// 29 clés de configWrite(), valeurs à longueur max
static std::vector<Entry> maxConfig() {
  return {
      {"ip", IP_MAX},
      {"gateway", IP_MAX},
      {"subnet", IP_MAX},
      {"dns1", IP_MAX},
      {"dns2", IP_MAX},
      {"http_port", "65535"},
      {"watchdogwifi_active", "1"},
      {"boost_duration", "600"},
      {"boost_ratio", "100"},
      {"mqtt_ip", IP_MAX},
      {"mqtt_port", "65535"},
      {"mqtt_period", "3600"},
      {"mqtt_user", repeat('u', 39)},
      {"mqtt_pass", "&#+%\"\\" + repeat('p', 33)},
      {"mqtt_active", "1"},
      {"boost_timer_hour", "23"},
      {"boost_timer_minute", "59"},
      {"boost_timer_active", "1"},
      {"remoterelay_active", "1"},
      {"remoterelay_ip", IP_MAX},
      {"remoterelay_port", "65535"},
      {"remoterelay_on", repeat('n', 127)},
      {"remoterelay_off", repeat('f', 127)},
      {"RelayPlus_min", "1200"},
      {"RelayPlus_max", "1380"},
      {"RelayPlus_hour", "23"},
      {"RelayPlus_active", "1"},
      {"max_temp", "90"},
      {"temp_limit_active", "1"},
  };
}

void test_deserialize_max_config() {
  std::vector<Entry> cfg = maxConfig();
  TEST_ASSERT_EQUAL(29, cfg.size());

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
  TEST_ASSERT_EQUAL_STRING(cfg[13].value.c_str(), doc["mqtt_pass"].as<const char *>());
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
  TEST_ASSERT_EQUAL_STRING(cfg[13].value.c_str(), back["mqtt_pass"].as<const char *>());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_deserialize_max_config);
  RUN_TEST(test_write_roundtrip);
  return UNITY_END();
}
