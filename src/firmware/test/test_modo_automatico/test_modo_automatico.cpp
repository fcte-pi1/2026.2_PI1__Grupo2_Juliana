// Modo automático (#210, RF13-S) testado no computador: a corrida inteira,
// de calibrar até parar no objetivo, com um robô que anda num labirinto
// desenhado (desenhos_labirintos.h) e só enxerga frente, esquerda e direita.
#include <unity.h>
#include <string.h>

#include "desenhos_labirintos.h"
#include "floodfill.h"
#include "labirinto.h"
#include "modo_automatico.h"

void setUp() {}
void tearDown() {}

// Robô de teste: a pose verdadeira fica aqui, separada do mapa do modo
// automático, para os testes pegarem qualquer dessincronia.
class RoboTeste : public InterfaceRobo
{
public:
    explicit RoboTeste(const Labirinto &gabarito) : gabarito_(gabarito) {}

    bool calibrar() override
    {
        calibracoes++;
        return calibraOk;
    }

    void sentir(Labirinto &mapa) override
    {
        Direcao frente = mapa.orientacao();
        revelar(mapa, frente);
        revelar(mapa, girarDireita(frente));
        revelar(mapa, girarEsquerda(frente));
    }

    bool mover(Direcao d) override
    {
        if (travado || gabarito_.temParede(x, y, d))
            return false;
        x = (uint8_t)(x + deslocamentoX(d));
        y = (uint8_t)(y + deslocamentoY(d));
        return true;
    }

    uint8_t x = 0, y = 0;
    bool calibraOk = true;
    bool travado = false; // motor que não anda
    int calibracoes = 0;

private:
    void revelar(Labirinto &mapa, Direcao d)
    {
        if (gabarito_.temParede(x, y, d))
            mapa.marcarParede(x, y, d);
        else
            mapa.marcarAberta(x, y, d);
    }

    const Labirinto &gabarito_;
};

static const Direcao ORIENTACOES[] = {NORTE, LESTE};

static void carregarFixture(const LabirintoTeste &f, Labirinto &gabarito, Direcao o)
{
    TEST_ASSERT_TRUE_MESSAGE(carregar(f.ascii, gabarito, o), f.nome);
    gabarito.definirObjetivo(f.objetivoX, f.objetivoY);
}

static void prepararMapa(const Labirinto &gabarito, Labirinto &mapa)
{
    mapa.iniciar(gabarito.largura(), gabarito.altura(), gabarito.orientacaoInicial());
    for (uint8_t x = 0; x < gabarito.largura(); x++)
        for (uint8_t y = 0; y < gabarito.altura(); y++)
            if (gabarito.ehObjetivo(x, y))
                mapa.definirObjetivo(x, y);
}

void test_corrida_completa_em_todos_os_desenhos_sem_intervencao()
{
    for (const auto &f : LABIRINTOS_TESTE)
        for (Direcao o : ORIENTACOES)
        {
            Labirinto gabarito, mapa;
            carregarFixture(f, gabarito, o);
            prepararMapa(gabarito, mapa);
            RoboTeste robo(gabarito);
            ModoAutomatico modo(robo, mapa);
            modo.iniciar();
            // Uma única chamada: nenhuma intervenção depois de ligado.
            TEST_ASSERT_EQUAL_MESSAGE(ESTADO_PARADO, modo.executar(), f.nome);
            TEST_ASSERT_EQUAL_MESSAGE(SEM_FALHA, modo.motivo(), f.nome);
            TEST_ASSERT_TRUE_MESSAGE(mapa.noObjetivo(), f.nome);
            // A pose do mapa bate com a posição real do robô.
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(robo.x, mapa.x(), f.nome);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(robo.y, mapa.y(), f.nome);
        }
}

void test_corrida_rapida_usa_a_rota_mais_curta_conferida()
{
    for (const auto &f : LABIRINTOS_TESTE)
        for (Direcao o : ORIENTACOES)
        {
            Labirinto gabarito, mapa;
            carregarFixture(f, gabarito, o);
            prepararMapa(gabarito, mapa);
            RoboTeste robo(gabarito);
            ModoAutomatico modo(robo, mapa);
            modo.iniciar();
            modo.executar();
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(f.movimentosConferidos, modo.passosRapido(), f.nome);
        }
}

void test_sequencia_de_estados_da_corrida()
{
    Labirinto gabarito, mapa;
    carregarFixture(LABIRINTOS_TESTE[0], gabarito, NORTE);
    prepararMapa(gabarito, mapa);
    RoboTeste robo(gabarito);
    ModoAutomatico modo(robo, mapa);
    modo.iniciar();
    TEST_ASSERT_EQUAL(ESTADO_CALIBRANDO, modo.estado());

    // Grava cada troca de estado.
    EstadoRobo trocas[64];
    int n = 0;
    trocas[n++] = modo.estado();
    while (modo.estado() != ESTADO_PARADO && modo.estado() != ESTADO_FALHA && n < 64)
    {
        EstadoRobo e = modo.passo();
        if (e != trocas[n - 1])
            trocas[n++] = e;
    }
    TEST_ASSERT_TRUE(n >= 5);
    TEST_ASSERT_EQUAL(ESTADO_CALIBRANDO, trocas[0]);
    TEST_ASSERT_EQUAL(ESTADO_EXPLORANDO, trocas[1]);
    TEST_ASSERT_EQUAL(ESTADO_RETORNANDO, trocas[2]);
    TEST_ASSERT_EQUAL(ESTADO_RAPIDO, trocas[n - 2]);
    TEST_ASSERT_EQUAL(ESTADO_PARADO, trocas[n - 1]);
    // Entre a primeira volta e a corrida rápida só pode haver idas e voltas.
    for (int i = 3; i < n - 2; i++)
        TEST_ASSERT_TRUE(trocas[i] == ESTADO_EXPLORANDO || trocas[i] == ESTADO_RETORNANDO);
}

void test_corrida_rapida_comeca_na_largada()
{
    Labirinto gabarito, mapa;
    carregarFixture(LABIRINTOS_TESTE[1], gabarito, NORTE);
    prepararMapa(gabarito, mapa);
    RoboTeste robo(gabarito);
    ModoAutomatico modo(robo, mapa);
    modo.iniciar();
    while (modo.estado() != ESTADO_RAPIDO && modo.estado() != ESTADO_FALHA)
        modo.passo();
    TEST_ASSERT_EQUAL(ESTADO_RAPIDO, modo.estado());
    TEST_ASSERT_EQUAL_UINT8(0, mapa.x());
    TEST_ASSERT_EQUAL_UINT8(0, mapa.y());
    TEST_ASSERT_TRUE(modo.viagens() >= 1);
}

void test_falha_na_calibracao_nao_sai_do_lugar()
{
    Labirinto gabarito, mapa;
    carregarFixture(LABIRINTOS_TESTE[0], gabarito, NORTE);
    prepararMapa(gabarito, mapa);
    RoboTeste robo(gabarito);
    robo.calibraOk = false;
    ModoAutomatico modo(robo, mapa);
    modo.iniciar();
    TEST_ASSERT_EQUAL(ESTADO_FALHA, modo.executar());
    TEST_ASSERT_EQUAL(FALHA_CALIBRACAO, modo.motivo());
    TEST_ASSERT_EQUAL_UINT16(0, modo.passos());
    TEST_ASSERT_EQUAL(1, robo.calibracoes);
}

void test_motor_que_nao_anda_vira_falha_depois_das_tentativas()
{
    Labirinto gabarito, mapa;
    carregarFixture(LABIRINTOS_TESTE[0], gabarito, NORTE);
    prepararMapa(gabarito, mapa);
    RoboTeste robo(gabarito);
    robo.travado = true;
    ModoAutomatico modo(robo, mapa);
    modo.iniciar();
    TEST_ASSERT_EQUAL(ESTADO_FALHA, modo.executar());
    TEST_ASSERT_EQUAL(FALHA_MOVIMENTO, modo.motivo());
    TEST_ASSERT_EQUAL_UINT16(0, modo.passos());
}

void test_limite_de_passos_impede_laco_infinito()
{
    Labirinto gabarito, mapa;
    carregarFixture(LABIRINTOS_TESTE[2], gabarito, NORTE); // 12x4, a mais longa
    prepararMapa(gabarito, mapa);
    RoboTeste robo(gabarito);
    ModoAutomatico modo(robo, mapa, 5);
    modo.iniciar();
    TEST_ASSERT_EQUAL(ESTADO_FALHA, modo.executar());
    TEST_ASSERT_EQUAL(FALHA_LIMITE, modo.motivo());
    TEST_ASSERT_EQUAL_UINT16(5, modo.passos());
}

// Objetivo fechado por paredes: não existe caminho.
static const char OBJETIVO_FECHADO[] = "+---+---+---+---+\n"
                                       "|           |   |\n"
                                       "+   +   +   +---+\n"
                                       "|               |\n"
                                       "+   +   +   +   +\n"
                                       "|               |\n"
                                       "+   +   +   +   +\n"
                                       "|               |\n"
                                       "+---+---+---+---+\n";

void test_objetivo_sem_caminho_vira_falha()
{
    Labirinto gabarito, mapa;
    TEST_ASSERT_TRUE(carregar(OBJETIVO_FECHADO, gabarito, NORTE));
    prepararMapa(gabarito, mapa);
    RoboTeste robo(gabarito);
    ModoAutomatico modo(robo, mapa);
    modo.iniciar();
    TEST_ASSERT_EQUAL(ESTADO_FALHA, modo.executar());
    TEST_ASSERT_EQUAL(FALHA_SEM_CAMINHO, modo.motivo());
}

void test_nomes_dos_estados_seguem_o_contrato()
{
    TEST_ASSERT_EQUAL_STRING("explorando", nomeEstado(ESTADO_EXPLORANDO));
    TEST_ASSERT_EQUAL_STRING("retornando", nomeEstado(ESTADO_RETORNANDO));
    TEST_ASSERT_EQUAL_STRING("rapido", nomeEstado(ESTADO_RAPIDO));
    TEST_ASSERT_EQUAL_STRING("parado", nomeEstado(ESTADO_PARADO));
    TEST_ASSERT_EQUAL_STRING("calibrando", nomeEstado(ESTADO_CALIBRANDO));
    TEST_ASSERT_EQUAL_STRING("falha", nomeEstado(ESTADO_FALHA));
}

void test_parado_e_falha_nao_mudam_mais()
{
    Labirinto gabarito, mapa;
    carregarFixture(LABIRINTOS_TESTE[0], gabarito, NORTE);
    prepararMapa(gabarito, mapa);
    RoboTeste robo(gabarito);
    ModoAutomatico modo(robo, mapa);
    modo.iniciar();
    modo.executar();
    uint16_t passos = modo.passos();
    TEST_ASSERT_EQUAL(ESTADO_PARADO, modo.passo());
    TEST_ASSERT_EQUAL_UINT16(passos, modo.passos());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_corrida_completa_em_todos_os_desenhos_sem_intervencao);
    RUN_TEST(test_corrida_rapida_usa_a_rota_mais_curta_conferida);
    RUN_TEST(test_sequencia_de_estados_da_corrida);
    RUN_TEST(test_corrida_rapida_comeca_na_largada);
    RUN_TEST(test_falha_na_calibracao_nao_sai_do_lugar);
    RUN_TEST(test_motor_que_nao_anda_vira_falha_depois_das_tentativas);
    RUN_TEST(test_limite_de_passos_impede_laco_infinito);
    RUN_TEST(test_objetivo_sem_caminho_vira_falha);
    RUN_TEST(test_nomes_dos_estados_seguem_o_contrato);
    RUN_TEST(test_parado_e_falha_nao_mudam_mais);
    return UNITY_END();
}
