#include <unity.h>

#include "direcao.h"

void setUp() {}
void tearDown() {}

void test_quatro_giros_a_direita_voltam_ao_inicio() {
  Direcao d = NORTE;
  for (int i = 0; i < 4; i++) d = girarDireita(d);
  TEST_ASSERT_EQUAL(NORTE, d);
}

void test_esquerda_desfaz_direita() {
  Direcao todas[] = {NORTE, LESTE, SUL, OESTE};
  for (Direcao d : todas) TEST_ASSERT_EQUAL(d, girarEsquerda(girarDireita(d)));
}

void test_oposta() {
  TEST_ASSERT_EQUAL(SUL, oposta(NORTE));
  TEST_ASSERT_EQUAL(OESTE, oposta(LESTE));
}

void test_deslocamento() {
  TEST_ASSERT_EQUAL(1, dx(LESTE));
  TEST_ASSERT_EQUAL(-1, dx(OESTE));
  TEST_ASSERT_EQUAL(1, dy(NORTE));
  TEST_ASSERT_EQUAL(-1, dy(SUL));
  TEST_ASSERT_EQUAL(0, dx(NORTE));
}

void test_letra_do_contrato() {
  TEST_ASSERT_EQUAL_CHAR('N', letra(NORTE));
  TEST_ASSERT_EQUAL_CHAR('L', letra(LESTE));
  TEST_ASSERT_EQUAL_CHAR('S', letra(SUL));
  TEST_ASSERT_EQUAL_CHAR('O', letra(OESTE));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_quatro_giros_a_direita_voltam_ao_inicio);
  RUN_TEST(test_esquerda_desfaz_direita);
  RUN_TEST(test_oposta);
  RUN_TEST(test_deslocamento);
  RUN_TEST(test_letra_do_contrato);
  return UNITY_END();
}
