#include <unity.h>
#include <math.h>
#include <initializer_list>
#include "filtro.h"
#include "paredes.h"
#include "labirinto.h"

void setUp() {}
void tearDown() {}

static DistanciaFiltrada aquecer(FiltroDistancia& filtro, uint16_t mm) {
    DistanciaFiltrada resultado = {};
    for (int i = 0; i < 5; ++i) resultado = filtro.atualizar(mm);
    return resultado;
}

void test_aquecimento_nao_inventa_distancia() {
    FiltroDistancia filtro;
    for (int i = 0; i < 4; ++i)
        TEST_ASSERT_EQUAL(EstadoLeitura::AQUECENDO, filtro.atualizar(80).estado);
    auto leitura = filtro.atualizar(80);
    TEST_ASSERT_EQUAL(EstadoLeitura::VALIDA, leitura.estado);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 80, leitura.mm);
}

void test_mediana_remove_dois_picos_na_janela() {
    FiltroDistancia filtro;
    aquecer(filtro, 80);
    const uint16_t serie[] = {80, 800, 80, 800, 80, 80, 80, 80};
    for (uint16_t bruto : serie)
        TEST_ASSERT_FLOAT_WITHIN(0.01, 80, filtro.atualizar(bruto).mm);
}

void test_ema_responde_a_mudanca_real_sem_salto() {
    FiltroDistancia filtro;
    aquecer(filtro, 80);
    filtro.atualizar(280);
    filtro.atualizar(280);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 140, filtro.atualizar(280).mm);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 182, filtro.atualizar(280).mm);
    for (int i = 0; i < 30; ++i) filtro.atualizar(280);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 280, filtro.atualizar(280).mm);
}

void test_invalida_limpa_historico_e_exige_cinco_amostras_novas() {
    const uint16_t invalidas[] = {0, 8190, 8191, 65535};
    for (uint16_t bruto : invalidas) {
        FiltroDistancia filtro;
        aquecer(filtro, 80);
        TEST_ASSERT_EQUAL(EstadoLeitura::INVALIDA, filtro.atualizar(bruto).estado);
        for (int i = 0; i < 4; ++i)
            TEST_ASSERT_EQUAL(EstadoLeitura::AQUECENDO, filtro.atualizar(300).estado);
        TEST_ASSERT_FLOAT_WITHIN(0.01, 300, filtro.atualizar(300).mm);
    }
}

void test_timeout_ou_dado_sem_atualizacao_e_invalido() {
    FiltroDistancia filtro;
    aquecer(filtro, 80);
    TEST_ASSERT_EQUAL(EstadoLeitura::INVALIDA, filtro.atualizar(80, false).estado);
    TEST_ASSERT_EQUAL(EstadoLeitura::AQUECENDO, filtro.atualizar(80).estado);
}

void test_parede_estacionaria_nao_e_falso_defeito() {
    FiltroDistancia filtro;
    aquecer(filtro, 80);
    for (int i = 0; i < 200; ++i)
        TEST_ASSERT_EQUAL(EstadoLeitura::VALIDA, filtro.atualizar(80).estado);
}

void test_alpha_invalido_e_rejeitado() {
    const float invalidos[] = {0, -0.1f, 1.1f, NAN, INFINITY};
    for (float alpha : invalidos) {
        FiltroDistancia filtro(alpha);
        TEST_ASSERT_EQUAL(EstadoLeitura::INVALIDA, aquecer(filtro, 80).estado);
    }
    FiltroDistancia semEma(1);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 80, aquecer(semEma, 80).mm);
}

void test_histerese_evitar_oscilacao_e_inicio_ambiguo() {
    DetectorParede detector;
    TEST_ASSERT_EQUAL(Parede::DESCONHECIDA, detector.atualizar({120, EstadoLeitura::VALIDA}));
    TEST_ASSERT_EQUAL(Parede::PRESENTE, detector.atualizar({109, EstadoLeitura::VALIDA}));
    for (float mm : {110.f, 119.f, 121.f, 130.f})
        TEST_ASSERT_EQUAL(Parede::PRESENTE, detector.atualizar({mm, EstadoLeitura::VALIDA}));
    TEST_ASSERT_EQUAL(Parede::LIVRE, detector.atualizar({131, EstadoLeitura::VALIDA}));
    TEST_ASSERT_EQUAL(Parede::LIVRE, detector.atualizar({110, EstadoLeitura::VALIDA}));
    TEST_ASSERT_EQUAL(Parede::PRESENTE, detector.atualizar({109, EstadoLeitura::VALIDA}));
}

void test_detector_nao_preserva_parede_apos_falha() {
    DetectorParede detector;
    detector.atualizar({80, EstadoLeitura::VALIDA});
    TEST_ASSERT_EQUAL(Parede::DESCONHECIDA, detector.atualizar({80, EstadoLeitura::INVALIDA}));
    TEST_ASSERT_EQUAL(Parede::DESCONHECIDA, detector.atualizar({120, EstadoLeitura::VALIDA}));
    const float invalidas[] = {0, -1, 8190, NAN, INFINITY};
    for (float mm : invalidas)
        TEST_ASSERT_EQUAL(Parede::DESCONHECIDA, detector.atualizar({mm, EstadoLeitura::VALIDA}));
    TEST_ASSERT_EQUAL(Parede::DESCONHECIDA, detector.atualizar({80, EstadoLeitura::AQUECENDO}));
    DetectorParede faixaInvalida(10, 10), acimaDoSensor(8185, 10);
    TEST_ASSERT_EQUAL(Parede::DESCONHECIDA, faixaInvalida.atualizar({80, EstadoLeitura::VALIDA}));
    TEST_ASSERT_EQUAL(Parede::DESCONHECIDA, acimaDoSensor.atualizar({80, EstadoLeitura::VALIDA}));
}

void test_oito_configuracoes_em_quatro_orientacoes_ct_se03() {
    // Tabela independente, explicita, na ordem frente/esquerda/direita.
    const Direcao orientacoes[] = {NORTE, LESTE, SUL, OESTE};
    const uint8_t bits[4][3] = {{1,8,2}, {2,1,4}, {4,2,8}, {8,4,1}};
    const uint8_t conhecidos[] = {11,7,14,13};
    for (int o = 0; o < 4; ++o) {
        for (int caso = 0; caso < 8; ++caso) {
            Parede p[3];
            uint8_t esperado = 0;
            for (int i = 0; i < 3; ++i) {
                p[i] = caso & (1 << i) ? Parede::PRESENTE : Parede::LIVRE;
                if (caso & (1 << i)) esperado |= bits[o][i];
            }
            auto resultado = paredesAbsolutas(p[0], p[1], p[2], orientacoes[o]);
            TEST_ASSERT_EQUAL_UINT8(esperado, resultado.presentes);
            TEST_ASSERT_EQUAL_UINT8(conhecidos[o], resultado.conhecidas);
        }
    }
}

void test_tras_e_sensor_invalido_nao_viram_passagem_livre() {
    auto p = paredesAbsolutas(Parede::DESCONHECIDA, Parede::PRESENTE, Parede::LIVRE, NORTE);
    TEST_ASSERT_EQUAL_UINT8(8, p.presentes);
    TEST_ASSERT_EQUAL_UINT8(10, p.conhecidas);
    p = paredesAbsolutas(Parede::LIVRE, Parede::LIVRE, Parede::LIVRE, NORTE, Parede::PRESENTE);
    TEST_ASSERT_EQUAL_UINT8(4, p.presentes);
    TEST_ASSERT_EQUAL_UINT8(15, p.conhecidas);
    p = paredesAbsolutas(Parede::LIVRE, Parede::LIVRE, Parede::LIVRE, NORTE, Parede::LIVRE);
    TEST_ASSERT_EQUAL_UINT8(0, p.presentes);
    TEST_ASSERT_EQUAL_UINT8(15, p.conhecidas);
    p = paredesAbsolutas(Parede::PRESENTE, Parede::PRESENTE, Parede::PRESENTE, static_cast<Direcao>(0));
    TEST_ASSERT_EQUAL_UINT8(0, p.conhecidas);
}

void test_serie_sintetica_so_esquerda_ct_se04() {
    FiltroDistancia f, e, d;
    DetectorParede pf, pe, pd;
    aquecer(f, 300); aquecer(e, 80); aquecer(d, 300);
    // Amostras sinteticas: nao representam ensaio com lanterna no hardware.
    const uint16_t esquerda[] = {80, 81, 600, 80, 79, 80, 80, 80, 81, 80};
    for (uint16_t mm : esquerda) {
        auto p = paredesAbsolutas(pf.atualizar(f.atualizar(300)),
                                  pe.atualizar(e.atualizar(mm)),
                                  pd.atualizar(d.atualizar(300)), NORTE);
        TEST_ASSERT_EQUAL_UINT8(8, p.presentes);
        TEST_ASSERT_EQUAL_UINT8(11, p.conhecidas);
    }
}

void test_integracao_preserva_desconhecidas_e_espelha_paredes() {
    Labirinto lab;
    TEST_ASSERT_TRUE(lab.iniciar(4, 4));
    auto p = paredesAbsolutas(Parede::PRESENTE, Parede::DESCONHECIDA, Parede::LIVRE, NORTE);
    const Direcao direcoes[] = {NORTE, LESTE, SUL, OESTE};
    for (Direcao dir : direcoes) {
        if (!(p.conhecidas & dir)) continue;
        if (p.presentes & dir) lab.marcarParede(1, 1, dir);
        else lab.marcarAberta(1, 1, dir);
    }
    TEST_ASSERT_TRUE(lab.temParede(1, 2, SUL));
    TEST_ASSERT_TRUE(lab.conhecida(2, 1, OESTE));
    TEST_ASSERT_FALSE(lab.temParede(2, 1, OESTE));
    TEST_ASSERT_FALSE(lab.conhecida(1, 1, OESTE));
    TEST_ASSERT_FALSE(lab.conhecida(1, 1, SUL));
}

void test_reiniciar_apos_mudar_pose_nao_mistura_celulas() {
    FiltroDistancia filtro;
    DetectorParede detector;
    detector.atualizar(aquecer(filtro, 80));
    filtro.reiniciar(); detector.reiniciar();
    TEST_ASSERT_EQUAL(Parede::DESCONHECIDA, detector.atualizar(filtro.atualizar(300)));
    TEST_ASSERT_EQUAL(Parede::LIVRE, detector.atualizar(aquecer(filtro, 300)));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_aquecimento_nao_inventa_distancia);
    RUN_TEST(test_mediana_remove_dois_picos_na_janela);
    RUN_TEST(test_ema_responde_a_mudanca_real_sem_salto);
    RUN_TEST(test_invalida_limpa_historico_e_exige_cinco_amostras_novas);
    RUN_TEST(test_timeout_ou_dado_sem_atualizacao_e_invalido);
    RUN_TEST(test_parede_estacionaria_nao_e_falso_defeito);
    RUN_TEST(test_alpha_invalido_e_rejeitado);
    RUN_TEST(test_histerese_evitar_oscilacao_e_inicio_ambiguo);
    RUN_TEST(test_detector_nao_preserva_parede_apos_falha);
    RUN_TEST(test_oito_configuracoes_em_quatro_orientacoes_ct_se03);
    RUN_TEST(test_tras_e_sensor_invalido_nao_viram_passagem_livre);
    RUN_TEST(test_serie_sintetica_so_esquerda_ct_se04);
    RUN_TEST(test_integracao_preserva_desconhecidas_e_espelha_paredes);
    RUN_TEST(test_reiniciar_apos_mudar_pose_nao_mistura_celulas);
    return UNITY_END();
}
