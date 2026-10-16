#include <unity.h>

#include "direcao.h"
#include "labirinto.h"

void setUp() {}
void tearDown() {}

void test_quatro_giros_a_direita_voltam_ao_inicio()
{
    Direcao d = NORTE;
    for (int i = 0; i < 4; i++)
        d = girarDireita(d);
    TEST_ASSERT_EQUAL(NORTE, d);
}

void test_esquerda_desfaz_direita()
{
    Direcao todas[] = {NORTE, LESTE, SUL, OESTE};
    for (Direcao d : todas)
        TEST_ASSERT_EQUAL(d, girarEsquerda(girarDireita(d)));
}

void test_oposta()
{
    TEST_ASSERT_EQUAL(SUL, oposta(NORTE));
    TEST_ASSERT_EQUAL(OESTE, oposta(LESTE));
}

void test_deslocamento()
{
    TEST_ASSERT_EQUAL(1, dx(LESTE));
    TEST_ASSERT_EQUAL(-1, dx(OESTE));
    TEST_ASSERT_EQUAL(1, dy(NORTE));
    TEST_ASSERT_EQUAL(-1, dy(SUL));
    TEST_ASSERT_EQUAL(0, dx(NORTE));
}

void test_letra_do_contrato()
{
    TEST_ASSERT_EQUAL_CHAR('N', letra(NORTE));
    TEST_ASSERT_EQUAL_CHAR('L', letra(LESTE));
    TEST_ASSERT_EQUAL_CHAR('S', letra(SUL));
    TEST_ASSERT_EQUAL_CHAR('O', letra(OESTE));
}

static void conferirBordas(uint8_t largura, uint8_t altura)
{
    Labirinto lab;
    lab.iniciar(largura, altura);
    for (uint8_t x = 0; x < largura; x++)
    {
        TEST_ASSERT_TRUE(lab.temParede(x, 0, SUL));
        TEST_ASSERT_TRUE(lab.temParede(x, altura - 1, NORTE));
        TEST_ASSERT_TRUE(lab.conhecida(x, 0, SUL));
    }
    for (uint8_t y = 0; y < altura; y++)
    {
        TEST_ASSERT_TRUE(lab.temParede(0, y, OESTE));
        TEST_ASSERT_TRUE(lab.temParede(largura - 1, y, LESTE));
    }
}

void test_bordas_fechadas_nos_labirintos_do_roteiro()
{
    conferirBordas(4, 4);
    conferirBordas(8, 4);
    conferirBordas(12, 4);
}

void test_interior_comeca_aberto_e_desconhecido()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    TEST_ASSERT_FALSE(lab.temParede(1, 1, NORTE));
    TEST_ASSERT_FALSE(lab.conhecida(1, 1, NORTE));
}

void test_parede_gravada_dos_dois_lados()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.marcarParede(1, 1, LESTE);
    TEST_ASSERT_TRUE(lab.temParede(1, 1, LESTE));
    TEST_ASSERT_TRUE(lab.temParede(2, 1, OESTE));
    TEST_ASSERT_TRUE(lab.conhecida(2, 1, OESTE));
}

void test_marcar_aberta_fica_conhecida_sem_parede()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.marcarAberta(1, 1, NORTE);
    TEST_ASSERT_FALSE(lab.temParede(1, 2, SUL));
    TEST_ASSERT_TRUE(lab.conhecida(1, 2, SUL));
}

void test_borda_nao_pode_ser_aberta()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.marcarAberta(0, 0, SUL);
    TEST_ASSERT_TRUE(lab.temParede(0, 0, SUL));
}

void test_coordenada_fora_do_labirinto_e_ignorada()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.marcarParede(9, 9, NORTE); // não pode travar nem escrever fora da matriz
    TEST_ASSERT_TRUE(lab.temParede(9, 9, NORTE));
    TEST_ASSERT_EQUAL_UINT8(0, lab.paredesDe(9, 9));
}

void test_mascara_do_contrato()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.marcarParede(2, 1, NORTE);
    lab.marcarParede(2, 1, OESTE);
    TEST_ASSERT_EQUAL_UINT8(9, lab.paredesDe(2, 1));           // exemplo do docs/4.4.7
    TEST_ASSERT_EQUAL_UINT8(SUL | OESTE, lab.paredesDe(0, 0)); // canto da largada
}

void test_comeca_na_largada_virado_para_o_norte()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    TEST_ASSERT_EQUAL_UINT8(0, lab.x());
    TEST_ASSERT_EQUAL_UINT8(0, lab.y());
    TEST_ASSERT_EQUAL(NORTE, lab.orientacao());
    TEST_ASSERT_TRUE(lab.visitada(0, 0));
    TEST_ASSERT_FALSE(lab.visitada(0, 1));
}

void test_avancar_atualiza_celula_e_marca_visitada()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    TEST_ASSERT_TRUE(lab.avancar());
    TEST_ASSERT_EQUAL_UINT8(0, lab.x());
    TEST_ASSERT_EQUAL_UINT8(1, lab.y());
    TEST_ASSERT_TRUE(lab.visitada(0, 1));
}

void test_avanco_bloqueado_por_parede_nao_move()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.marcarParede(0, 0, NORTE);
    TEST_ASSERT_FALSE(lab.avancar());
    TEST_ASSERT_EQUAL_UINT8(0, lab.x());
    TEST_ASSERT_EQUAL_UINT8(0, lab.y());
}

void test_avanco_bloqueado_pela_borda()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.girar(OESTE);
    TEST_ASSERT_FALSE(lab.avancar());
}

void test_girar_troca_a_orientacao()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.girar(girarDireita(lab.orientacao()));
    TEST_ASSERT_EQUAL(LESTE, lab.orientacao());
    TEST_ASSERT_EQUAL_CHAR('L', letra(lab.orientacao()));
}

// CT-SE10: a sequência de células e orientações bate com o percurso feito.
void test_percurso_registra_celula_e_orientacao()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.girar(LESTE);
    lab.avancar(); // (1,0)
    lab.avancar(); // (2,0)
    lab.girar(NORTE);
    lab.avancar(); // (2,1)
    TEST_ASSERT_EQUAL_UINT8(2, lab.x());
    TEST_ASSERT_EQUAL_UINT8(1, lab.y());
    TEST_ASSERT_EQUAL(NORTE, lab.orientacao());
    TEST_ASSERT_TRUE(lab.visitada(1, 0));
    TEST_ASSERT_FALSE(lab.visitada(1, 1)); // não passou por ali
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_quatro_giros_a_direita_voltam_ao_inicio);
    RUN_TEST(test_esquerda_desfaz_direita);
    RUN_TEST(test_oposta);
    RUN_TEST(test_deslocamento);
    RUN_TEST(test_letra_do_contrato);
    RUN_TEST(test_bordas_fechadas_nos_labirintos_do_roteiro);
    RUN_TEST(test_interior_comeca_aberto_e_desconhecido);
    RUN_TEST(test_parede_gravada_dos_dois_lados);
    RUN_TEST(test_marcar_aberta_fica_conhecida_sem_parede);
    RUN_TEST(test_borda_nao_pode_ser_aberta);
    RUN_TEST(test_coordenada_fora_do_labirinto_e_ignorada);
    RUN_TEST(test_mascara_do_contrato);
    RUN_TEST(test_comeca_na_largada_virado_para_o_norte);
    RUN_TEST(test_avancar_atualiza_celula_e_marca_visitada);
    RUN_TEST(test_avanco_bloqueado_por_parede_nao_move);
    RUN_TEST(test_avanco_bloqueado_pela_borda);
    RUN_TEST(test_girar_troca_a_orientacao);
    RUN_TEST(test_percurso_registra_celula_e_orientacao);

    return UNITY_END();
}
