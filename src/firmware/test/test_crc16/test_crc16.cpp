#include <string.h>
#include <unity.h>

#include "crc16.h"

static uint16_t crc_de(const char* texto) {
  return crc16_ccitt((const uint8_t*)texto, strlen(texto));
}

void setUp() {}
void tearDown() {}

void test_valor_de_verificacao() { TEST_ASSERT_EQUAL_HEX16(0x29B1, crc_de("123456789")); }

void test_exemplo_valido_do_contrato() {
  TEST_ASSERT_EQUAL_HEX16(0x8653, crc_de("{\"v\":1,\"t\":\"tel\",\"corrida\":7,\"seq\":42,\"tempo_ms\":8400,\"pos_x\":2,\"pos_y\":1,\"orientacao\":\"L\",\"paredes\":9,\"velocidade\":0.312,\"tensao_bateria\":7.86,\"flood\":3,\"estado\":\"explorando\"}"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_valor_de_verificacao);
  RUN_TEST(test_exemplo_valido_do_contrato);
  return UNITY_END();
}
