// Vérifie la validation des valeurs de paramètres EcoPV (P1-4) : aucun
// caractère hors "-0-9." ne doit pouvoir atteindre la liaison série.
#include <unity.h>
#include "maxpv_validate.h"

void test_valid_values() {
  TEST_ASSERT_TRUE(isNumericParamValue("5"));
  TEST_ASSERT_TRUE(isNumericParamValue("-3.5"));
  TEST_ASSERT_TRUE(isNumericParamValue("0.001"));
  TEST_ASSERT_TRUE(isNumericParamValue(".5"));
  TEST_ASSERT_TRUE(isNumericParamValue("1000"));
  TEST_ASSERT_TRUE(isNumericParamValue("123456789012345"));
}

void test_rejected_values() {
  TEST_ASSERT_FALSE(isNumericParamValue(nullptr));
  TEST_ASSERT_FALSE(isNumericParamValue(""));
  TEST_ASSERT_FALSE(isNumericParamValue("-"));
  TEST_ASSERT_FALSE(isNumericParamValue("."));
  TEST_ASSERT_FALSE(isNumericParamValue("-."));
  TEST_ASSERT_FALSE(isNumericParamValue("5#FORMAT"));
  TEST_ASSERT_FALSE(isNumericParamValue("1,END#FORMAT"));
  TEST_ASSERT_FALSE(isNumericParamValue("1.2.3"));
  TEST_ASSERT_FALSE(isNumericParamValue("3-"));
  TEST_ASSERT_FALSE(isNumericParamValue("--1"));
  TEST_ASSERT_FALSE(isNumericParamValue(" 5"));
  TEST_ASSERT_FALSE(isNumericParamValue("1e5"));
  TEST_ASSERT_FALSE(isNumericParamValue("Infinity"));
  TEST_ASSERT_FALSE(isNumericParamValue("NaN"));
  TEST_ASSERT_FALSE(isNumericParamValue("1234567890123456"));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_valid_values);
  RUN_TEST(test_rejected_values);
  return UNITY_END();
}
