// Monitor da bateria (#216, #277): regras do Contrato de Telemetria (4.4.7).
#include <unity.h>

#include "monitor_bateria.h"

void setUp() {}
void tearDown() {}

// Alimenta o monitor a 5 Hz por 'ms' milissegundos com a mesma leitura.
// Devolve o primeiro evento diferente de OK (ou OK).
static EventoBateria rodar(MonitorBateria &m, uint32_t &t, uint32_t ms, float v, float ma = 300,
                           bool valida = true)
{
    EventoBateria primeiro = BATERIA_OK;
    for (uint32_t fim = t + ms; t < fim; t += 200)
    {
        EventoBateria e = m.atualizar(t, valida, v, ma);
        if (primeiro == BATERIA_OK)
            primeiro = e;
    }
    return primeiro;
}

void test_bateria_cheia_nao_gera_evento()
{
    MonitorBateria m;
    uint32_t t = 0;
    TEST_ASSERT_EQUAL(BATERIA_OK, rodar(m, t, 5000, 8.2f));
    TEST_ASSERT_TRUE(m.presente());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 8.2f, m.tensao());
}

void test_so_usb_nao_conta_como_bateria()
{
    // Na bancada, sem bateria, o INA226 lê ~5,6 V (USB voltando pelo MP1584).
    MonitorBateria m;
    uint32_t t = 0;
    TEST_ASSERT_EQUAL(BATERIA_OK, rodar(m, t, 5000, 5.6f));
    TEST_ASSERT_FALSE(m.presente());
    TEST_ASSERT_EQUAL_UINT8(0, m.percentual());
}

void test_aviso_depois_de_1_s_abaixo_de_7_v()
{
    MonitorBateria m;
    uint32_t t = 0;
    rodar(m, t, 3000, 7.4f);
    // Média móvel de 8 leituras + 1 s de persistência: aviso em no máximo ~3 s.
    TEST_ASSERT_EQUAL(BATERIA_AVISO, rodar(m, t, 4000, 6.9f));
    TEST_ASSERT_TRUE(m.critica());
}

void test_queda_curta_nao_gera_aviso()
{
    MonitorBateria m;
    uint32_t t = 0;
    rodar(m, t, 3000, 7.4f);
    // Um pico de 0,4 s (motor arrancando) não mexe a média abaixo de 7 V por 1 s.
    TEST_ASSERT_EQUAL(BATERIA_OK, rodar(m, t, 400, 6.5f));
    TEST_ASSERT_EQUAL(BATERIA_OK, rodar(m, t, 5000, 7.4f));
}

void test_aviso_so_repete_depois_de_voltar_acima_de_7_2()
{
    MonitorBateria m;
    uint32_t t = 0;
    rodar(m, t, 3000, 7.4f);
    TEST_ASSERT_EQUAL(BATERIA_AVISO, rodar(m, t, 4000, 6.9f));
    // Oscila entre 7,0 e 7,2: não repete.
    TEST_ASSERT_EQUAL(BATERIA_OK, rodar(m, t, 4000, 7.1f));
    TEST_ASSERT_EQUAL(BATERIA_OK, rodar(m, t, 4000, 6.9f));
    // Volta acima de 7,2 V e cai de novo: repete.
    rodar(m, t, 4000, 7.3f);
    TEST_ASSERT_FALSE(m.critica());
    TEST_ASSERT_EQUAL(BATERIA_AVISO, rodar(m, t, 4000, 6.9f));
}

void test_parada_abaixo_de_6_6_v()
{
    MonitorBateria m;
    uint32_t t = 0;
    rodar(m, t, 3000, 7.4f);
    rodar(m, t, 4000, 6.9f); // aviso antes
    TEST_ASSERT_EQUAL(BATERIA_PARAR, rodar(m, t, 4000, 6.5f));
    // Depois de parar, não gera mais nada.
    TEST_ASSERT_EQUAL(BATERIA_OK, rodar(m, t, 4000, 6.5f));
}

void test_ina226_sem_resposta_para_o_robo()
{
    MonitorBateria m;
    uint32_t t = 0;
    rodar(m, t, 3000, 7.8f);
    TEST_ASSERT_EQUAL(BATERIA_OK, rodar(m, t, 600, 0, 0, false)); // < 1 s: tolera
    rodar(m, t, 400, 7.8f);
    TEST_ASSERT_EQUAL(BATERIA_FALHA_SENSOR, rodar(m, t, 2000, 0, 0, false));
}

void test_sem_ina226_e_sem_bateria_nao_para()
{
    // Bancada sem INA226: não há o que parar.
    MonitorBateria m;
    uint32_t t = 0;
    TEST_ASSERT_EQUAL(BATERIA_OK, rodar(m, t, 5000, 0, 0, false));
}

void test_percentual_linear_igual_ao_painel()
{
    MonitorBateria m;
    uint32_t t = 0;
    rodar(m, t, 2000, 8.4f);
    TEST_ASSERT_EQUAL_UINT8(100, m.percentual());
    MonitorBateria m2;
    t = 0;
    rodar(m2, t, 2000, 7.4f);
    TEST_ASSERT_EQUAL_UINT8(50, m2.percentual());
    MonitorBateria m3;
    t = 0;
    rodar(m3, t, 2000, 6.2f);
    TEST_ASSERT_EQUAL_UINT8(0, m3.percentual());
}

void test_autonomia_pelo_calculo_do_4_2()
{
    // 1500 mAh x 80 % x 50 % de carga = 600 mAh; a 400 mA -> 90 min.
    MonitorBateria m;
    uint32_t t = 0;
    rodar(m, t, 2000, 7.4f, 400);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 90.0f, m.autonomiaMinutos());
}

void test_consumo_medio_da_corrida()
{
    MonitorBateria m;
    uint32_t t = 0;
    rodar(m, t, 2000, 8.0f, 100);
    m.iniciarConsumo();
    rodar(m, t, 1000, 8.0f, 300);
    rodar(m, t, 1000, 8.0f, 500);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 400.0f, m.consumoMedio());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_bateria_cheia_nao_gera_evento);
    RUN_TEST(test_so_usb_nao_conta_como_bateria);
    RUN_TEST(test_aviso_depois_de_1_s_abaixo_de_7_v);
    RUN_TEST(test_queda_curta_nao_gera_aviso);
    RUN_TEST(test_aviso_so_repete_depois_de_voltar_acima_de_7_2);
    RUN_TEST(test_parada_abaixo_de_6_6_v);
    RUN_TEST(test_ina226_sem_resposta_para_o_robo);
    RUN_TEST(test_sem_ina226_e_sem_bateria_nao_para);
    RUN_TEST(test_percentual_linear_igual_ao_painel);
    RUN_TEST(test_autonomia_pelo_calculo_do_4_2);
    RUN_TEST(test_consumo_medio_da_corrida);
    return UNITY_END();
}
