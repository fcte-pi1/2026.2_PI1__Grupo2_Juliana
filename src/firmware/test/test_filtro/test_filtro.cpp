#include <unity.h>

#include "filtro_distancia.h"

// As séries abaixo vieram da bancada de 10/11/2026 (teste_vl53, 3 VL53L0X
// pelo USB). Ruído medido: ±3–5 mm com o alvo parado.

void setUp() {}
void tearDown() {}

static void encher(FiltroDistancia &f, const uint16_t *serie, int n)
{
    for (int i = 0; i < n; i++)
        f.adicionar(serie[i]);
}

void test_sem_leitura_nao_esta_pronto_e_vale_sem_alvo()
{
    FiltroDistancia f;
    TEST_ASSERT_FALSE(f.pronto());
    TEST_ASSERT_EQUAL_UINT16(DIST_SEM_ALVO, f.valor());
}

void test_mediana_de_serie_real_parada()
{
    // Frontal livre, lendo a parede do fundo da bancada.
    const uint16_t serie[] = {392, 397, 391, 405, 396};
    FiltroDistancia f;
    encher(f, serie, 5);
    TEST_ASSERT_EQUAL_UINT16(396, f.valor());
}

void test_mediana_com_menos_leituras_que_a_janela()
{
    FiltroDistancia f;
    f.adicionar(43);
    TEST_ASSERT_EQUAL_UINT16(43, f.valor());
    f.adicionar(41);
    f.adicionar(42);
    TEST_ASSERT_EQUAL_UINT16(42, f.valor());
}

void test_ignora_dois_picos_seguidos()
{
    // Direito parado em ~64 mm com dois picos (reflexo/vibração).
    const uint16_t serie[] = {64, 8191, 63, 289, 65};
    FiltroDistancia f;
    encher(f, serie, 5);
    TEST_ASSERT_UINT16_WITHIN(2, 64, f.valor());
}

void test_janela_desliza_e_esquece_leituras_antigas()
{
    FiltroDistancia f;
    const uint16_t perto[] = {40, 41, 42, 40, 43};
    const uint16_t longe[] = {510, 506, 512};
    encher(f, perto, 5);
    encher(f, longe, 3); // a mão saiu da frente do sensor
    TEST_ASSERT_UINT16_WITHIN(10, 510, f.valor());
}

void test_leitura_acima_do_confiavel_vira_sem_alvo()
{
    FiltroDistancia f;
    const uint16_t serie[] = {8191, 8190, 1500, 8191, 2000};
    encher(f, serie, 5);
    TEST_ASSERT_TRUE(f.semAlvo());
}

void test_timeout_e_zero_nao_entram_na_janela()
{
    FiltroDistancia f;
    f.adicionar(100);
    f.adicionar(0);
    f.adicionar(555, true);
    TEST_ASSERT_EQUAL_UINT16(100, f.valor());
    TEST_ASSERT_EQUAL_UINT16(2, f.falhasSeguidas());
}

void test_falhas_seguidas_marcam_defeito()
{
    FiltroDistancia f;
    for (int i = 0; i < FiltroDistancia::MAX_FALHAS - 1; i++)
        f.adicionar(0, true);
    TEST_ASSERT_FALSE(f.defeito());
    f.adicionar(0, true);
    TEST_ASSERT_TRUE(f.defeito());
}

void test_leitura_boa_tira_o_defeito()
{
    FiltroDistancia f;
    for (int i = 0; i < FiltroDistancia::MAX_FALHAS; i++)
        f.adicionar(0, true);
    f.adicionar(80);
    TEST_ASSERT_FALSE(f.defeito());
}

void test_limpar_zera_tudo()
{
    FiltroDistancia f;
    f.adicionar(80);
    f.adicionar(0, true);
    f.limpar();
    TEST_ASSERT_FALSE(f.pronto());
    TEST_ASSERT_EQUAL_UINT16(0, f.falhasSeguidas());
}

void test_parede_liga_abaixo_do_limiar()
{
    DetectorParede d(120, 140);
    TEST_ASSERT_FALSE(d.atualizar(300));
    TEST_ASSERT_TRUE(d.atualizar(64));
}

void test_histerese_nao_pisca_perto_do_limiar()
{
    DetectorParede d(120, 140);
    d.atualizar(100);
    // Oscila entre os dois limiares: continua com parede.
    const uint16_t perto[] = {125, 118, 135, 122, 139};
    for (uint16_t mm : perto)
        TEST_ASSERT_TRUE(d.atualizar(mm));
    TEST_ASSERT_FALSE(d.atualizar(141));
    // Volta para a faixa do meio: continua sem parede.
    TEST_ASSERT_FALSE(d.atualizar(130));
}

void test_sem_alvo_nunca_e_parede()
{
    DetectorParede d;
    TEST_ASSERT_FALSE(d.atualizar(DIST_SEM_ALVO));
}

void test_configurar_corrige_limiares_invertidos()
{
    DetectorParede d;
    d.configurar(150, 100);
    TEST_ASSERT_EQUAL_UINT16(150, d.limiarLiga());
    TEST_ASSERT_EQUAL_UINT16(150, d.limiarDesliga());
}

void test_filtro_mais_detector_em_serie_com_pico()
{
    // Sensor lateral com parede a ~64 mm e um pico de "sem alvo" no meio:
    // o detector não pode perder a parede por causa do pico.
    const uint16_t serie[] = {64, 63, 65, 8191, 62, 64, 8191, 63};
    FiltroDistancia f;
    DetectorParede d;
    for (uint16_t mm : serie)
    {
        f.adicionar(mm);
        TEST_ASSERT_TRUE(d.atualizar(f.valor()));
    }
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_sem_leitura_nao_esta_pronto_e_vale_sem_alvo);
    RUN_TEST(test_mediana_de_serie_real_parada);
    RUN_TEST(test_mediana_com_menos_leituras_que_a_janela);
    RUN_TEST(test_ignora_dois_picos_seguidos);
    RUN_TEST(test_janela_desliza_e_esquece_leituras_antigas);
    RUN_TEST(test_leitura_acima_do_confiavel_vira_sem_alvo);
    RUN_TEST(test_timeout_e_zero_nao_entram_na_janela);
    RUN_TEST(test_falhas_seguidas_marcam_defeito);
    RUN_TEST(test_leitura_boa_tira_o_defeito);
    RUN_TEST(test_limpar_zera_tudo);
    RUN_TEST(test_parede_liga_abaixo_do_limiar);
    RUN_TEST(test_histerese_nao_pisca_perto_do_limiar);
    RUN_TEST(test_sem_alvo_nunca_e_parede);
    RUN_TEST(test_configurar_corrige_limiares_invertidos);
    RUN_TEST(test_filtro_mais_detector_em_serie_com_pico);
    return UNITY_END();
}
