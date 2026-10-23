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
    TEST_ASSERT_EQUAL(1, deslocamentoX(LESTE));
    TEST_ASSERT_EQUAL(-1, deslocamentoX(OESTE));
    TEST_ASSERT_EQUAL(1, deslocamentoY(NORTE));
    TEST_ASSERT_EQUAL(-1, deslocamentoY(SUL));
    TEST_ASSERT_EQUAL(0, deslocamentoX(NORTE));
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

void test_aceita_os_tamanhos_do_roteiro()
{
    Labirinto lab;
    TEST_ASSERT_TRUE(lab.iniciar(4, 4));
    TEST_ASSERT_TRUE(lab.iniciar(8, 4));
    TEST_ASSERT_TRUE(lab.iniciar(12, 4));
    TEST_ASSERT_TRUE(lab.iniciar(4, 8));
    TEST_ASSERT_TRUE(lab.iniciar(4, 12));
}

void test_recusa_tamanho_fora_do_roteiro()
{
    Labirinto lab;
    TEST_ASSERT_FALSE(lab.iniciar(0, 4));
    TEST_ASSERT_FALSE(lab.iniciar(20, 4));
    TEST_ASSERT_FALSE(lab.iniciar(5, 7));
    TEST_ASSERT_FALSE(lab.iniciar(8, 8));
    TEST_ASSERT_FALSE(lab.iniciar(16, 16));
}

void test_tamanho_recusado_nao_altera_o_mapa()
{
    Labirinto lab;
    lab.iniciar(8, 4);
    lab.marcarParede(1, 1, LESTE);
    lab.avancar();
    TEST_ASSERT_FALSE(lab.iniciar(5, 7));
    TEST_ASSERT_EQUAL_UINT8(8, lab.largura());
    TEST_ASSERT_EQUAL_UINT8(4, lab.altura());
    TEST_ASSERT_TRUE(lab.temParede(1, 1, LESTE));
    TEST_ASSERT_EQUAL_UINT8(1, lab.y());
    TEST_ASSERT_TRUE(lab.ehObjetivo(7, 3));
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
    lab.marcarParede(9, 9, NORTE);
    TEST_ASSERT_TRUE(lab.temParede(9, 9, NORTE));
    TEST_ASSERT_TRUE(lab.conhecida(9, 9, NORTE));
    TEST_ASSERT_EQUAL_UINT8(0, lab.paredesDe(9, 9));
}

void test_mascara_do_contrato()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.marcarParede(2, 1, NORTE);
    lab.marcarParede(2, 1, OESTE);
    TEST_ASSERT_EQUAL_UINT8(9, lab.paredesDe(2, 1));          
    TEST_ASSERT_EQUAL_UINT8(SUL | OESTE, lab.paredesDe(0, 0)); 
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

void test_parede_na_frente_impede_avanco()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    TEST_ASSERT_TRUE(lab.podeAvancar());
    lab.marcarParede(0, 0, NORTE);
    TEST_ASSERT_FALSE(lab.podeAvancar());
    TEST_ASSERT_EQUAL_UINT8(0, lab.x());
    TEST_ASSERT_EQUAL_UINT8(0, lab.y());
}

// Parede falsa do filtro: o motor andou mesmo assim e a pose acompanha.
void test_avancar_registra_mesmo_com_parede_no_mapa()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.marcarParede(0, 0, NORTE);
    TEST_ASSERT_TRUE(lab.avancar());
    TEST_ASSERT_EQUAL_UINT8(0, lab.x());
    TEST_ASSERT_EQUAL_UINT8(1, lab.y());
    TEST_ASSERT_TRUE(lab.visitada(0, 1));
}

void test_avanco_bloqueado_pela_borda()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.girar(OESTE);
    TEST_ASSERT_FALSE(lab.podeAvancar());
    TEST_ASSERT_FALSE(lab.avancar());
    TEST_ASSERT_EQUAL_UINT8(0, lab.x());
    TEST_ASSERT_EQUAL_UINT8(0, lab.y());
}

void test_girar_troca_a_orientacao()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.girar(girarDireita(lab.orientacao()));
    TEST_ASSERT_EQUAL(LESTE, lab.orientacao());
    TEST_ASSERT_EQUAL_CHAR('L', letra(lab.orientacao()));
}

void test_percurso_registra_celula_e_orientacao()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.girar(LESTE);
    lab.avancar(); 
    lab.avancar(); 
    lab.girar(NORTE);
    lab.avancar(); 
    TEST_ASSERT_EQUAL_UINT8(2, lab.x());
    TEST_ASSERT_EQUAL_UINT8(1, lab.y());
    TEST_ASSERT_EQUAL(NORTE, lab.orientacao());
    TEST_ASSERT_TRUE(lab.visitada(1, 0));
    TEST_ASSERT_FALSE(lab.visitada(1, 1)); 
}

void test_largada_nao_e_objetivo()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    TEST_ASSERT_FALSE(lab.noObjetivo());
}

static void conferirCantoOposto(uint8_t largura, uint8_t altura)
{
    Labirinto lab;
    lab.iniciar(largura, altura);
    TEST_ASSERT_TRUE(lab.ehObjetivo(largura - 1, altura - 1));
    TEST_ASSERT_FALSE(lab.ehObjetivo(0, 0));
    TEST_ASSERT_FALSE(lab.ehObjetivo(largura - 1, 0));
    TEST_ASSERT_FALSE(lab.ehObjetivo(0, altura - 1));
}

void test_objetivo_no_canto_oposto_a_largada()
{
    conferirCantoOposto(4, 4);
    conferirCantoOposto(8, 4);
    conferirCantoOposto(12, 4);
    conferirCantoOposto(4, 8);
}

void test_reconhece_a_chegada()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.girar(LESTE);
    for (int i = 0; i < 3; i++)
        lab.avancar();
    lab.girar(NORTE);
    lab.avancar(); 
    lab.avancar(); 
    TEST_ASSERT_FALSE(lab.noObjetivo());
    lab.avancar();
    TEST_ASSERT_TRUE(lab.noObjetivo());
}

void test_nao_encerra_antes_do_objetivo()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.marcarParede(1, 0, NORTE); 
    lab.marcarParede(1, 0, LESTE);

    lab.girar(LESTE);
    lab.avancar(); 
    TEST_ASSERT_FALSE(lab.noObjetivo());
    TEST_ASSERT_FALSE(lab.podeAvancar());

    lab.girar(OESTE);
    lab.avancar(); 
    lab.girar(NORTE);
    for (int i = 0; i < 3; i++)
    {
        lab.avancar(); 
        TEST_ASSERT_FALSE(lab.noObjetivo());
    }
    lab.girar(LESTE);
    lab.avancar(); 
    lab.avancar(); 
    TEST_ASSERT_FALSE(lab.noObjetivo());
    lab.avancar();
    TEST_ASSERT_TRUE(lab.noObjetivo());
}

void test_definir_objetivo_troca_a_celula()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    TEST_ASSERT_TRUE(lab.definirObjetivo(1, 2));
    TEST_ASSERT_TRUE(lab.ehObjetivo(1, 2));
    TEST_ASSERT_FALSE(lab.ehObjetivo(3, 3));
}

void test_objetivo_fora_do_labirinto_e_recusado()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    TEST_ASSERT_FALSE(lab.definirObjetivo(4, 0));
    TEST_ASSERT_FALSE(lab.ehObjetivo(4, 0));
    TEST_ASSERT_TRUE(lab.ehObjetivo(3, 3)); 
}

void test_iniciar_volta_o_objetivo_para_o_canto_oposto()
{
    Labirinto lab;
    lab.iniciar(4, 4);
    lab.definirObjetivo(1, 1);
    lab.iniciar(8, 4);
    TEST_ASSERT_FALSE(lab.ehObjetivo(1, 1));
    TEST_ASSERT_TRUE(lab.ehObjetivo(7, 3));
}

void test_borda_a_direita_larga_virado_para_o_leste()
{
    Labirinto lab;
    TEST_ASSERT_TRUE(lab.iniciar(4, 4, LESTE));
    TEST_ASSERT_EQUAL(LESTE, lab.orientacao());
    TEST_ASSERT_EQUAL(LESTE, lab.orientacaoInicial());
    TEST_ASSERT_EQUAL_UINT8(0, lab.x());
    TEST_ASSERT_EQUAL_UINT8(0, lab.y());
    TEST_ASSERT_TRUE(lab.temParede(0, 0, OESTE));                    // atrás
    TEST_ASSERT_TRUE(lab.temParede(0, 0, girarDireita(LESTE)));      // à direita
    TEST_ASSERT_TRUE(lab.ehObjetivo(3, 3));
}

void test_recusa_orientacao_inicial_sem_borda_atras()
{
    Labirinto lab;
    lab.iniciar(8, 4);
    TEST_ASSERT_FALSE(lab.iniciar(4, 4, SUL));
    TEST_ASSERT_FALSE(lab.iniciar(4, 4, OESTE));
    TEST_ASSERT_EQUAL_UINT8(8, lab.largura());
}

void test_iniciar_na_largada_converte_o_que_a_equipe_informa()
{
    Labirinto lab;
    TEST_ASSERT_TRUE(lab.iniciarNaLargada(BORDA_ESQUERDA, 4, 8)); // 4 à frente, 8 à direita
    TEST_ASSERT_EQUAL_UINT8(8, lab.largura());
    TEST_ASSERT_EQUAL_UINT8(4, lab.altura());
    TEST_ASSERT_EQUAL(NORTE, lab.orientacao());

    TEST_ASSERT_TRUE(lab.iniciarNaLargada(BORDA_DIREITA, 8, 4)); // 8 à frente, 4 à esquerda
    TEST_ASSERT_EQUAL_UINT8(8, lab.largura());
    TEST_ASSERT_EQUAL_UINT8(4, lab.altura());
    TEST_ASSERT_EQUAL(LESTE, lab.orientacao());

    TEST_ASSERT_TRUE(lab.iniciarNaLargada(BORDA_DIREITA, 4, 12));
    TEST_ASSERT_EQUAL_UINT8(4, lab.largura());
    TEST_ASSERT_EQUAL_UINT8(12, lab.altura());
    TEST_ASSERT_TRUE(lab.ehObjetivo(3, 11));

    TEST_ASSERT_FALSE(lab.iniciarNaLargada(BORDA_DIREITA, 8, 8));
    TEST_ASSERT_FALSE(lab.iniciarNaLargada((LadoBorda)7, 4, 4)); // lado inválido
}

void test_tamanho_fixo_do_labirinto()
{
    TEST_ASSERT_TRUE(sizeof(Labirinto) <= 1024);
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
    RUN_TEST(test_aceita_os_tamanhos_do_roteiro);
    RUN_TEST(test_recusa_tamanho_fora_do_roteiro);
    RUN_TEST(test_tamanho_recusado_nao_altera_o_mapa);
    RUN_TEST(test_interior_comeca_aberto_e_desconhecido);
    RUN_TEST(test_parede_gravada_dos_dois_lados);
    RUN_TEST(test_marcar_aberta_fica_conhecida_sem_parede);
    RUN_TEST(test_borda_nao_pode_ser_aberta);
    RUN_TEST(test_coordenada_fora_do_labirinto_e_ignorada);
    RUN_TEST(test_mascara_do_contrato);
    RUN_TEST(test_comeca_na_largada_virado_para_o_norte);
    RUN_TEST(test_avancar_atualiza_celula_e_marca_visitada);
    RUN_TEST(test_parede_na_frente_impede_avanco);
    RUN_TEST(test_avancar_registra_mesmo_com_parede_no_mapa);
    RUN_TEST(test_avanco_bloqueado_pela_borda);
    RUN_TEST(test_girar_troca_a_orientacao);
    RUN_TEST(test_percurso_registra_celula_e_orientacao);
    RUN_TEST(test_largada_nao_e_objetivo);
    RUN_TEST(test_objetivo_no_canto_oposto_a_largada);
    RUN_TEST(test_reconhece_a_chegada);
    RUN_TEST(test_nao_encerra_antes_do_objetivo);
    RUN_TEST(test_definir_objetivo_troca_a_celula);
    RUN_TEST(test_objetivo_fora_do_labirinto_e_recusado);
    RUN_TEST(test_iniciar_volta_o_objetivo_para_o_canto_oposto);
    RUN_TEST(test_borda_a_direita_larga_virado_para_o_leste);
    RUN_TEST(test_recusa_orientacao_inicial_sem_borda_atras);
    RUN_TEST(test_iniciar_na_largada_converte_o_que_a_equipe_informa);
    RUN_TEST(test_tamanho_fixo_do_labirinto);

    return UNITY_END();
}
