#include <unity.h>

#include <chrono>
#include <stdio.h>
#include <string.h>

#include "../robo_simulado.h"
#include "floodfill.h"
#include "labirinto.h"

void setUp() {}
void tearDown() {}

// Labirinto 4x4 desenhado à mão. Rota mais curta: L, L, L, N, N, N.
static const char *LAB_4X4 =
    "+---+---+---+---+\n"
    "|           |   |\n"
    "+---+---+   +   +\n"
    "|       |       |\n"
    "+   +   +---+   +\n"
    "|   |           |\n"
    "+   +---+---+   +\n"
    "|               |\n"
    "+---+---+---+---+\n";

static const char *LAB_4X4_ABERTO =
    "+---+---+---+---+\n"
    "|               |\n"
    "+   +   +   +   +\n"
    "|               |\n"
    "+   +   +   +   +\n"
    "|               |\n"
    "+   +   +   +   +\n"
    "|               |\n"
    "+---+---+---+---+\n";

// Largada com parede à frente: a saída do beco fica à direita (labirinto.h).
static const char *LAB_SAIDA_A_DIREITA =
    "+---+---+---+---+\n"
    "|               |\n"
    "+   +   +   +   +\n"
    "|               |\n"
    "+   +   +   +   +\n"
    "|               |\n"
    "+---+   +   +   +\n"
    "|               |\n"
    "+---+---+---+---+\n";

// Anda a rota no mapa, consultando podeAvancar() a cada passo como a navegação
// faz, e diz se ela chega ao objetivo sem atravessar parede.
static bool percorre(Labirinto lab, const Direcao rota[], uint8_t passos)
{
    for (uint8_t i = 0; i < passos; i++)
    {
        lab.girar(rota[i]);
        if (!lab.podeAvancar() || !lab.avancar())
            return false;
    }
    return lab.noObjetivo();
}

void test_sem_paredes_internas_e_distancia_de_manhattan()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    FloodFill ff;
    ff.recalcular(lab, EXPLORACAO);
    for (uint8_t x = 0; x < 4; x++)
        for (uint8_t y = 0; y < 4; y++)
            TEST_ASSERT_EQUAL_UINT8((3 - x) + (3 - y), ff.distancia(x, y));
}

void test_rapido_trata_desconhecida_como_fechada()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    FloodFill ff;
    ff.recalcular(lab, RAPIDO);
    TEST_ASSERT_EQUAL_UINT8(0, ff.distancia(3, 3));
    TEST_ASSERT_EQUAL_UINT8(INFINITO, ff.distancia(0, 0));
}

void test_recalculo_apos_parede_nova()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    FloodFill ff;
    ff.recalcular(lab, EXPLORACAO);
    TEST_ASSERT_EQUAL_UINT8(1, ff.distancia(3, 2));

    lab.marcarParede(3, 2, NORTE);
    ff.recalcular(lab, EXPLORACAO);
    TEST_ASSERT_EQUAL_UINT8(3, ff.distancia(3, 2)); // contorna por (2,2) e (2,3)
}

void test_distancia_fora_do_mapa_e_infinita()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    FloodFill ff;
    ff.recalcular(lab, EXPLORACAO);
    TEST_ASSERT_EQUAL_UINT8(INFINITO, ff.distancia(MAX_LARG, 0));
    TEST_ASSERT_EQUAL_UINT8(INFINITO, ff.distancia(5, 5)); // dentro da matriz, fora do 4x4
}

void test_valor_atual_e_a_distancia_da_celula_do_robo()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    FloodFill ff;
    ff.recalcular(lab, EXPLORACAO);
    TEST_ASSERT_EQUAL_UINT8(6, ff.valorAtual(lab));
    lab.avancar();
    TEST_ASSERT_EQUAL_UINT8(5, ff.valorAtual(lab));
}

void test_empate_prefere_seguir_reto()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    FloodFill ff;
    ff.recalcular(lab, EXPLORACAO);
    Direcao d;
    TEST_ASSERT_TRUE(ff.proximaDirecao(lab, d));
    TEST_ASSERT_EQUAL(NORTE, d);

    lab.girar(LESTE);
    TEST_ASSERT_TRUE(ff.proximaDirecao(lab, d));
    TEST_ASSERT_EQUAL(LESTE, d);
}

void test_empate_prefere_direita_a_esquerda()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.definirObjetivo(1, 3);
    lab.marcarParede(1, 1, NORTE);
    lab.girar(LESTE);
    lab.avancar(); // (1,0)
    lab.girar(NORTE);
    lab.avancar(); // (1,1), parede à frente
    FloodFill ff;
    ff.recalcular(lab, EXPLORACAO);
    Direcao d;
    TEST_ASSERT_TRUE(ff.proximaDirecao(lab, d));
    TEST_ASSERT_EQUAL(LESTE, d); // (2,1) e (0,1) estão a 3 células
}

void test_beco_sem_saida_volta_por_tras()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.marcarParede(0, 1, NORTE);
    lab.marcarParede(0, 1, LESTE);
    lab.avancar(); // (0,1), virado para o Norte
    FloodFill ff;
    ff.recalcular(lab, EXPLORACAO);
    Direcao d;
    TEST_ASSERT_TRUE(ff.proximaDirecao(lab, d));
    TEST_ASSERT_EQUAL(SUL, d); // giro de 180°
}

void test_largada_com_frente_fechada_sai_pela_direita()
{
    Labirinto lab;
    carregar(LAB_SAIDA_A_DIREITA, lab);
    FloodFill ff;
    ff.recalcular(lab, EXPLORACAO);
    Direcao d;
    TEST_ASSERT_FALSE(lab.podeAvancar());
    TEST_ASSERT_TRUE(ff.proximaDirecao(lab, d));
    TEST_ASSERT_EQUAL(girarDireita(lab.orientacao()), d);
}

void test_sem_caminho_nao_escolhe_direcao()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    FloodFill ff;
    ff.recalcular(lab, RAPIDO);
    Direcao d;
    TEST_ASSERT_FALSE(ff.proximaDirecao(lab, d));
}

void test_carregar_le_as_paredes_do_desenho()
{
    Labirinto lab;
    TEST_ASSERT_TRUE(carregar(LAB_4X4, lab));
    TEST_ASSERT_EQUAL_UINT8(4, lab.largura());
    TEST_ASSERT_EQUAL_UINT8(4, lab.altura());
    TEST_ASSERT_TRUE(lab.temParede(2, 3, LESTE));
    TEST_ASSERT_TRUE(lab.temParede(0, 2, NORTE));
    TEST_ASSERT_TRUE(lab.temParede(0, 1, LESTE));
    TEST_ASSERT_TRUE(lab.temParede(1, 0, NORTE));
    TEST_ASSERT_FALSE(lab.temParede(2, 2, NORTE));
    TEST_ASSERT_FALSE(lab.temParede(0, 0, NORTE));
    TEST_ASSERT_TRUE(lab.conhecida(0, 0, NORTE));
    TEST_ASSERT_TRUE(lab.conhecida(1, 1, LESTE));
}

void test_carregar_le_labirinto_retangular()
{
    Labirinto lab;
    TEST_ASSERT_TRUE(carregar("+---+---+---+---+---+---+---+---+\n"
                              "|                               |\n"
                              "+   +   +   +   +   +   +   +   +\n"
                              "|                               |\n"
                              "+   +   +   +   +   +   +   +   +\n"
                              "|                               |\n"
                              "+   +   +   +   +   +   +   +   +\n"
                              "|   |                           |\n"
                              "+---+---+---+---+---+---+---+---+\n",
                              lab));
    TEST_ASSERT_EQUAL_UINT8(8, lab.largura());
    TEST_ASSERT_EQUAL_UINT8(4, lab.altura());
    TEST_ASSERT_TRUE(lab.temParede(0, 0, LESTE));
    TEST_ASSERT_TRUE(lab.ehObjetivo(7, 3));
}

void test_carregar_ignora_linhas_em_branco_nas_pontas()
{
    Labirinto lab;
    TEST_ASSERT_TRUE(carregar("\n"
                              "+---+---+---+---+\n"
                              "|           |   |  \r\n"
                              "+   +   +   +   +\n"
                              "|               |\n"
                              "+   +   +   +   +\n"
                              "|               |\n"
                              "+   +   +   +   +\n"
                              "|               |\n"
                              "+---+---+---+---+\n"
                              "\n",
                              lab));
    TEST_ASSERT_EQUAL_UINT8(4, lab.largura());
    TEST_ASSERT_TRUE(lab.temParede(2, 3, LESTE));
    TEST_ASSERT_FALSE(lab.temParede(0, 3, LESTE));
}

void test_carregar_recusa_desenho_malformado()
{
    Labirinto lab;
    TEST_ASSERT_FALSE(carregar("", lab));
    TEST_ASSERT_FALSE(carregar("+---+\n|   |\n", lab));           // número par de linhas
    TEST_ASSERT_FALSE(carregar("+---+\n|  |\n+---+\n", lab));      // linhas de tamanhos diferentes
    TEST_ASSERT_FALSE(carregar("+--+\n|  |\n+--+\n", lab));        // célula sem 4 colunas
    TEST_ASSERT_FALSE(carregar("+---+\n|   |\n\n+---+\n", lab));   // linha em branco no meio
}

void test_carregar_recusa_tamanho_fora_do_roteiro()
{
    Labirinto lab;
    TEST_ASSERT_FALSE(carregar("+---+---+\n"
                               "|       |\n"
                               "+---+---+\n",
                               lab)); // 2x1 é bem desenhado, mas não é do roteiro
}

void test_carregar_recusa_labirinto_maior_que_16()
{
    char desenho[3 * 70 + 1];
    char *p = desenho;
    for (int linha = 0; linha < 3; linha++)
    {
        for (int c = 0; c < 4 * 17 + 1; c++)
            *p++ = linha == 1 ? (c == 0 || c == 4 * 17 ? '|' : ' ') : (c % 4 == 0 ? '+' : '-');
        *p++ = '\n';
    }
    *p = '\0';
    Labirinto lab;
    TEST_ASSERT_FALSE(carregar(desenho, lab));
}

void test_imprimir_mapa_com_lados_desconhecidos()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    FloodFill ff;
    ff.recalcular(lab, EXPLORACAO);
    char buf[200];
    TEST_ASSERT_TRUE(imprimirMapa(lab, ff, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("+---+---+---+---+\n"
                             "| 3 . 2 . 1 . 0 |\n"
                             "+ . + . + . + . +\n"
                             "| 4 . 3 . 2 . 1 |\n"
                             "+ . + . + . + . +\n"
                             "| 5 . 4 . 3 . 2 |\n"
                             "+ . + . + . + . +\n"
                             "| 6 . 5 . 4 . 3 |\n"
                             "+---+---+---+---+\n",
                             buf);
}

void test_imprimir_mapa_mostra_paredes_e_distancias()
{
    Labirinto lab;
    carregar(LAB_4X4, lab);
    FloodFill ff;
    ff.recalcular(lab, EXPLORACAO);
    char buf[200];
    TEST_ASSERT_TRUE(imprimirMapa(lab, ff, buf, sizeof(buf)));
    TEST_ASSERT_NOT_NULL(strstr(buf, "| 5   4   3 | 0 |\n")); // fileira do topo, contada à mão
    TEST_ASSERT_NOT_NULL(strstr(buf, "| 6   5   4   3 |\n")); // fileira da largada
}

void test_imprimir_mapa_pode_ser_lido_de_volta()
{
    Labirinto original;
    carregar(LAB_4X4, original);
    FloodFill ff;
    ff.recalcular(original, EXPLORACAO);
    char buf[200];
    imprimirMapa(original, ff, buf, sizeof(buf));

    Labirinto relido;
    TEST_ASSERT_TRUE(carregar(buf, relido));
    for (uint8_t x = 0; x < 4; x++)
        for (uint8_t y = 0; y < 4; y++)
            TEST_ASSERT_EQUAL_UINT8(original.paredesDe(x, y), relido.paredesDe(x, y));
}

void test_imprimir_mapa_sem_caminho_e_buffer_pequeno()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    FloodFill ff;
    ff.recalcular(lab, RAPIDO);
    char buf[200];
    TEST_ASSERT_TRUE(imprimirMapa(lab, ff, buf, sizeof(buf)));
    TEST_ASSERT_NOT_NULL(strstr(buf, "| - . - . - . - |\n"));

    char pequeno[100]; // o 4x4 precisa de 18 * 9 + 1 = 163 bytes
    TEST_ASSERT_FALSE(imprimirMapa(lab, ff, pequeno, sizeof(pequeno)));
}

void test_rota_mais_curta_no_labirinto_conhecido()
{
    Labirinto lab;
    carregar(LAB_4X4, lab);
    FloodFill ff;
    Direcao rota[32];
    uint8_t passos = ff.rotaMaisCurta(lab, rota, 32);
    TEST_ASSERT_EQUAL_UINT8(6, passos);
    const Direcao esperada[] = {LESTE, LESTE, LESTE, NORTE, NORTE, NORTE};
    for (int i = 0; i < 6; i++)
        TEST_ASSERT_EQUAL(esperada[i], rota[i]);
    TEST_ASSERT_TRUE(percorre(lab, rota, passos));
}

void test_rota_desvia_de_parede_nova()
{
    Labirinto lab;
    carregar(LAB_4X4_ABERTO, lab);
    FloodFill ff;
    Direcao rota[32];
    TEST_ASSERT_EQUAL_UINT8(6, ff.rotaMaisCurta(lab, rota, 32));
    TEST_ASSERT_EQUAL(NORTE, rota[0]);

    lab.marcarParede(0, 0, NORTE);
    TEST_ASSERT_EQUAL_UINT8(6, ff.rotaMaisCurta(lab, rota, 32));
    TEST_ASSERT_EQUAL(LESTE, rota[0]);
    TEST_ASSERT_TRUE(percorre(lab, rota, 6));
}

void test_rota_so_usa_lados_conhecidos()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    for (uint8_t x = 0; x < 3; x++)
        lab.marcarAberta(x, 0, LESTE);
    for (uint8_t y = 0; y < 3; y++)
        lab.marcarAberta(3, y, NORTE);
    FloodFill ff;
    Direcao rota[32];
    TEST_ASSERT_EQUAL_UINT8(6, ff.rotaMaisCurta(lab, rota, 32));
    TEST_ASSERT_EQUAL(LESTE, rota[0]); // o Norte é desconhecido, então não serve
}

void test_sem_rota_ou_rota_que_nao_cabe_devolve_zero()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    FloodFill ff;
    Direcao rota[32];
    TEST_ASSERT_EQUAL_UINT8(0, ff.rotaMaisCurta(lab, rota, 32));

    carregar(LAB_4X4, lab);
    TEST_ASSERT_EQUAL_UINT8(0, ff.rotaMaisCurta(lab, rota, 5));
}

void test_rota_garantida_com_o_mapa_completo()
{
    Labirinto lab;
    carregar(LAB_4X4, lab);
    FloodFill ff;
    TEST_ASSERT_TRUE(ff.rotaGarantida(lab));
}

void test_rota_nao_garantida_sem_rota_conhecida()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    FloodFill ff;
    TEST_ASSERT_FALSE(ff.rotaGarantida(lab));
}

void test_rota_nao_garantida_enquanto_um_atalho_e_desconhecido()
{
    // Rota conhecida de 8 passos com um vaivém no começo; a de 6 pelo Leste da
    // largada ainda não foi vista, então pode existir.
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.marcarAberta(0, 0, NORTE);
    lab.marcarAberta(0, 1, LESTE);
    lab.marcarAberta(1, 1, SUL);
    lab.marcarAberta(1, 0, LESTE);
    lab.marcarAberta(2, 0, LESTE);
    for (uint8_t y = 0; y < 3; y++)
        lab.marcarAberta(3, y, NORTE);
    FloodFill ff;
    TEST_ASSERT_FALSE(ff.rotaGarantida(lab));
    TEST_ASSERT_EQUAL_UINT8(8, ff.distancia(0, 0)); // deixou o modo RAPIDO

    lab.marcarAberta(0, 0, LESTE);
    TEST_ASSERT_TRUE(ff.rotaGarantida(lab));
    TEST_ASSERT_EQUAL_UINT8(6, ff.distancia(0, 0));
}

void test_robo_simulado_explora_ate_o_objetivo()
{
    Labirinto gabarito;
    carregar(LAB_4X4, gabarito);
    RoboSimulado robo(gabarito);
    TEST_ASSERT_TRUE(robo.explorar());
    TEST_ASSERT_TRUE(robo.mapa().noObjetivo());
    TEST_ASSERT_TRUE(robo.sincronizado());
    TEST_ASSERT_TRUE(robo.passos() >= 6);
    for (uint8_t x = 0; x < 4; x++)
        for (uint8_t y = 0; y < 4; y++)
            if (robo.mapa().visitada(x, y))
                TEST_ASSERT_EQUAL_UINT8(gabarito.paredesDe(x, y), robo.mapa().paredesDe(x, y));
}

void test_robo_simulado_sai_da_largada_pela_direita()
{
    Labirinto gabarito;
    carregar(LAB_SAIDA_A_DIREITA, gabarito);
    RoboSimulado robo(gabarito);
    TEST_ASSERT_TRUE(robo.explorar());
    TEST_ASSERT_TRUE(robo.mapa().visitada(1, 0));
    TEST_ASSERT_EQUAL_UINT16(6, robo.passos()); // saiu pela direita sem desperdiçar passo
    TEST_ASSERT_EQUAL_UINT16(2, robo.giros());  // direita na largada e esquerda em (3,0)
}

void test_robo_simulado_nao_atravessa_parede_e_conta_giros()
{
    Labirinto gabarito;
    carregar(LAB_4X4, gabarito);
    RoboSimulado robo(gabarito);
    TEST_ASSERT_TRUE(robo.mover(LESTE));   // 1 giro
    TEST_ASSERT_FALSE(robo.mover(NORTE));  // 1 giro, parede em (1,0) Norte
    TEST_ASSERT_TRUE(robo.mover(OESTE));   // 1 giro
    TEST_ASSERT_TRUE(robo.mover(LESTE));   // 2 giros (180°)
    TEST_ASSERT_EQUAL_UINT16(3, robo.passos());
    TEST_ASSERT_EQUAL_UINT16(5, robo.giros());
}

// RNF01: cada recálculo leva menos de 50 ms.
static void conferirTempo(uint8_t largura, uint8_t altura)
{
    Labirinto lab;
    TEST_ASSERT_TRUE(lab.iniciar(largura, altura));
    FloodFill ff;
    double maximo = 0;
    double total = 0;
    for (int i = 0; i < 1000; i++)
    {
        auto inicio = std::chrono::steady_clock::now();
        ff.recalcular(lab, EXPLORACAO);
        auto fim = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(fim - inicio).count();
        total += ms;
        if (ms > maximo)
            maximo = ms;
    }
    TEST_ASSERT_EQUAL_UINT8(largura + altura - 2, ff.distancia(0, 0)); // mediu um mapa de verdade

    // Evidência do RNF01: aparece na saída de pio test -e native -v.
    char relato[80];
    snprintf(relato, sizeof(relato), "RNF01 %ux%u: media %.4f ms, maximo %.4f ms (1000 recalculos)",
             largura, altura, total / 1000, maximo);
    TEST_MESSAGE(relato);

    TEST_ASSERT_TRUE(total / 1000 < 50.0);
    TEST_ASSERT_TRUE(maximo < 50.0);
}

void test_tempo_recalculo_menor_50ms()
{
    conferirTempo(4, 4);
    conferirTempo(8, 4);
    conferirTempo(12, 4);
    conferirTempo(4, 12);
}

void test_tamanho_fixo_do_flood_fill()
{
    TEST_ASSERT_TRUE(sizeof(FloodFill) <= 512);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_sem_paredes_internas_e_distancia_de_manhattan);
    RUN_TEST(test_rapido_trata_desconhecida_como_fechada);
    RUN_TEST(test_recalculo_apos_parede_nova);
    RUN_TEST(test_distancia_fora_do_mapa_e_infinita);
    RUN_TEST(test_valor_atual_e_a_distancia_da_celula_do_robo);
    RUN_TEST(test_empate_prefere_seguir_reto);
    RUN_TEST(test_empate_prefere_direita_a_esquerda);
    RUN_TEST(test_beco_sem_saida_volta_por_tras);
    RUN_TEST(test_largada_com_frente_fechada_sai_pela_direita);
    RUN_TEST(test_sem_caminho_nao_escolhe_direcao);
    RUN_TEST(test_carregar_le_as_paredes_do_desenho);
    RUN_TEST(test_carregar_le_labirinto_retangular);
    RUN_TEST(test_carregar_ignora_linhas_em_branco_nas_pontas);
    RUN_TEST(test_carregar_recusa_desenho_malformado);
    RUN_TEST(test_carregar_recusa_tamanho_fora_do_roteiro);
    RUN_TEST(test_carregar_recusa_labirinto_maior_que_16);
    RUN_TEST(test_imprimir_mapa_com_lados_desconhecidos);
    RUN_TEST(test_imprimir_mapa_mostra_paredes_e_distancias);
    RUN_TEST(test_imprimir_mapa_pode_ser_lido_de_volta);
    RUN_TEST(test_imprimir_mapa_sem_caminho_e_buffer_pequeno);
    RUN_TEST(test_rota_mais_curta_no_labirinto_conhecido);
    RUN_TEST(test_rota_desvia_de_parede_nova);
    RUN_TEST(test_rota_so_usa_lados_conhecidos);
    RUN_TEST(test_sem_rota_ou_rota_que_nao_cabe_devolve_zero);
    RUN_TEST(test_rota_garantida_com_o_mapa_completo);
    RUN_TEST(test_rota_nao_garantida_sem_rota_conhecida);
    RUN_TEST(test_rota_nao_garantida_enquanto_um_atalho_e_desconhecido);
    RUN_TEST(test_robo_simulado_explora_ate_o_objetivo);
    RUN_TEST(test_robo_simulado_sai_da_largada_pela_direita);
    RUN_TEST(test_robo_simulado_nao_atravessa_parede_e_conta_giros);
    RUN_TEST(test_tempo_recalculo_menor_50ms);
    RUN_TEST(test_tamanho_fixo_do_flood_fill);

    return UNITY_END();
}
