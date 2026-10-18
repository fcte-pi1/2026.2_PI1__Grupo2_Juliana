// Testes de aceitacao da navegacao (#164): CT-SE09, CT-SE11 e CT-SE14 por
// simulacao, com os desenhos de desenhos_labirintos.h. Os dois primeiros
// testes validam os proprios desenhos.
#include <unity.h>
#include <string.h>
#include "../robo_simulado.h"
#include "desenhos_labirintos.h"
#include "floodfill.h"
#include "labirinto.h"

void setUp() {}
void tearDown() {}

static char caractere(const LabirintoTeste &f, int linha, int coluna)
{
    return f.ascii[linha * (4 * f.largura + 2) + coluna];
}

void test_fixtures_tem_dimensoes_bordas_e_simbolos_validos()
{
    for (const auto &f : LABIRINTOS_TESTE)
    {
        TEST_ASSERT_EQUAL_UINT32((2 * f.altura + 1) * (4 * f.largura + 2), strlen(f.ascii));
        TEST_ASSERT_TRUE(f.objetivoX < f.largura && f.objetivoY < f.altura);
        for (int r = 0; r <= 2 * f.altura; ++r)
        {
            TEST_ASSERT_EQUAL_CHAR('\n', caractere(f, r, 4 * f.largura + 1));
            for (int c = 0; c <= 4 * f.largura; ++c)
            {
                const char ch = caractere(f, r, c);
                if (r % 2 == 0 && c % 4 == 0)
                    TEST_ASSERT_EQUAL_CHAR('+', ch);
                else if (r % 2 == 0)
                {
                    TEST_ASSERT_TRUE(ch == '-' || ch == ' ');
                    if (r == 0 || r == 2 * f.altura)
                        TEST_ASSERT_EQUAL_CHAR('-', ch);
                }
                else if (c % 4 == 0)
                {
                    TEST_ASSERT_TRUE(ch == '|' || ch == ' ');
                    if (c == 0 || c == 4 * f.largura)
                        TEST_ASSERT_EQUAL_CHAR('|', ch);
                }
                else
                    TEST_ASSERT_EQUAL_CHAR(' ', ch);
            }
        }
    }
}

void test_objetivos_das_fixtures_sao_alcancaveis()
{
    // Confere conectividade, sem calcular a rota minima usada como referencia.
    for (const auto &f : LABIRINTOS_TESTE)
    {
        bool visitada[48] = {};
        uint8_t pilha[48] = {}, n = 1;
        visitada[0] = true;
        while (n)
        {
            const int celula = pilha[--n];
            const int x = celula % f.largura, y = celula / f.largura;
            const int dx[] = {0, 1, 0, -1}, dy[] = {1, 0, -1, 0};
            for (int d = 0; d < 4; ++d)
            {
                const int u = x + dx[d], v = y + dy[d];
                if (u < 0 || u >= f.largura || v < 0 || v >= f.altura)
                    continue;
                const int r = 2 * (f.altura - 1 - y) + 1 - dy[d];
                const int c = 4 * x + 2 + 2 * dx[d];
                const int vizinho = v * f.largura + u;
                if (caractere(f, r, c) == ' ' && !visitada[vizinho])
                {
                    visitada[vizinho] = true;
                    TEST_ASSERT_LESS_THAN_UINT8(48, n);
                    pilha[n++] = vizinho;
                }
            }
        }
        TEST_ASSERT_TRUE_MESSAGE(visitada[f.objetivoY * f.largura + f.objetivoX], f.nome);
    }
}

static const Direcao TODAS[] = {NORTE, LESTE, SUL, OESTE};

// Desenhos explorados no CT-SE11 e no CT-SE14: os tres tamanhos do roteiro,
// com e sem armadilha, e o 4x4 com mais de uma rota.
static const char *const EXPLORADOS[] = {
    "MISTO_4X4", "BARREIRAS_8X4", "BARREIRAS_12X4", "DUAS_ROTAS",
    "ARMADILHA_4X4", "ARMADILHA_8X4", "ARMADILHA_12X4",
};
static const char *const ARMADILHAS[] = {"ARMADILHA_4X4", "ARMADILHA_8X4", "ARMADILHA_12X4"};

static const LabirintoTeste &desenho(const char *nome)
{
    for (const auto &f : LABIRINTOS_TESTE)
        if (strcmp(f.nome, nome) == 0)
            return f;
    TEST_FAIL_MESSAGE(nome);
    return LABIRINTOS_TESTE[0];
}

static void carregarDesenho(const char *nome, Labirinto &gabarito)
{
    const LabirintoTeste &f = desenho(nome);
    TEST_ASSERT_TRUE_MESSAGE(carregar(f.ascii, gabarito), nome);
    TEST_ASSERT_TRUE_MESSAGE(gabarito.definirObjetivo(f.objetivoX, f.objetivoY), nome);
}

// Script de referencia do CT-SE14: busca em largura direto nas paredes do
// gabarito, sem passar pelo Flood Fill. INFINITO se o objetivo nao e alcancavel.
static uint8_t menorCaminho(const Labirinto &gabarito)
{
    uint8_t dist[MAX_LARG][MAX_ALT];
    memset(dist, INFINITO, sizeof(dist));
    uint8_t filaX[MAX_LARG * MAX_ALT];
    uint8_t filaY[MAX_LARG * MAX_ALT];
    uint16_t inicio = 0;
    uint16_t fim = 0;
    dist[0][0] = 0;
    filaX[fim] = 0;
    filaY[fim] = 0;
    fim++;
    while (inicio < fim)
    {
        uint8_t x = filaX[inicio];
        uint8_t y = filaY[inicio];
        inicio++;
        if (gabarito.ehObjetivo(x, y))
            return dist[x][y];
        for (Direcao d : TODAS)
        {
            if (gabarito.temParede(x, y, d))
                continue;
            uint8_t vizinhoX = x + deslocamentoX(d);
            uint8_t vizinhoY = y + deslocamentoY(d);
            if (dist[vizinhoX][vizinhoY] != INFINITO)
                continue;
            dist[vizinhoX][vizinhoY] = dist[x][y] + 1;
            filaX[fim] = vizinhoX;
            filaY[fim] = vizinhoY;
            fim++;
        }
    }
    return INFINITO;
}

// A rota sai da largada, nunca atravessa parede do gabarito e termina no objetivo.
static bool rotaValida(const Labirinto &gabarito, const Direcao rota[], uint8_t passos)
{
    uint8_t x = 0;
    uint8_t y = 0;
    for (uint8_t i = 0; i < passos; i++)
    {
        if (gabarito.temParede(x, y, rota[i]))
            return false;
        x += deslocamentoX(rota[i]);
        y += deslocamentoY(rota[i]);
    }
    return gabarito.ehObjetivo(x, y);
}

// O mapa so tem o que o robo viu: todo lado conhecido e igual ao gabarito, a
// celula visitada tem os 4 lados conhecidos e um lado de celula nao visitada
// so e conhecido se for borda ou se o vizinho foi visitado.
static void conferirMapa(const char *nome, const Labirinto &gabarito, const Labirinto &mapa)
{
    for (uint8_t x = 0; x < gabarito.largura(); x++)
        for (uint8_t y = 0; y < gabarito.altura(); y++)
            for (Direcao d : TODAS)
            {
                if (!mapa.conhecida(x, y, d))
                {
                    TEST_ASSERT_FALSE_MESSAGE(mapa.visitada(x, y), nome);
                    continue;
                }
                TEST_ASSERT_EQUAL_MESSAGE(gabarito.temParede(x, y, d), mapa.temParede(x, y, d), nome);
                int vizinhoX = x + deslocamentoX(d);
                int vizinhoY = y + deslocamentoY(d);
                bool borda = vizinhoX < 0 || vizinhoY < 0 || vizinhoX >= gabarito.largura() ||
                             vizinhoY >= gabarito.altura();
                if (!mapa.visitada(x, y) && !borda)
                    TEST_ASSERT_TRUE_MESSAGE(mapa.visitada(vizinhoX, vizinhoY), nome);
            }
}

// CT-SE09: largada pela convencao do labirinto.h (borda atras e a esquerda).
static void conferirSaidaDaLargada(const char *nome, Direcao saida, uint8_t x, uint8_t y,
                                   uint16_t giros)
{
    Labirinto gabarito;
    carregarDesenho(nome, gabarito);
    RoboSimulado robo(gabarito);
    robo.floodFill().recalcular(robo.mapa(), EXPLORACAO);
    Direcao d;
    TEST_ASSERT_TRUE_MESSAGE(robo.floodFill().proximaDirecao(robo.mapa(), d), nome);
    TEST_ASSERT_EQUAL_MESSAGE(saida, d, nome);
    TEST_ASSERT_TRUE_MESSAGE(robo.mover(d), nome); // sem tentar atravessar parede
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(x, robo.mapa().x(), nome);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(y, robo.mapa().y(), nome);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(giros, robo.giros(), nome);
    TEST_ASSERT_TRUE_MESSAGE(robo.explorar(), nome); // segue explorando sozinho
}

void test_ct_se09_largada_com_saida_a_frente()
{
    conferirSaidaDaLargada("BECO_LARGADA", NORTE, 0, 1, 0);
}

void test_ct_se09_largada_com_saida_a_direita()
{
    conferirSaidaDaLargada("SAIDA_DIREITA", LESTE, 1, 0, 1);
}

// CT-SE11: o mapa montado so com os sensores bate com o desenho.
void test_ct_se11_mapa_montado_igual_ao_desenho()
{
    for (const char *nome : EXPLORADOS)
    {
        Labirinto gabarito;
        carregarDesenho(nome, gabarito);
        RoboSimulado robo(gabarito);
        TEST_ASSERT_TRUE_MESSAGE(robo.explorarAteRotaGarantida(), nome);
        TEST_ASSERT_TRUE_MESSAGE(robo.sincronizado(), nome);
        conferirMapa(nome, gabarito, robo.mapa());
    }
}

// CT-SE11, passo 1: nas armadilhas o robo revisita celulas sem corromper o mapa.
void test_ct_se11_armadilhas_obrigam_a_revisitar()
{
    for (const char *nome : ARMADILHAS)
    {
        Labirinto gabarito;
        carregarDesenho(nome, gabarito);
        RoboSimulado robo(gabarito);
        TEST_ASSERT_TRUE_MESSAGE(robo.explorar(), nome);
        uint16_t visitadas = 0;
        for (uint8_t x = 0; x < gabarito.largura(); x++)
            for (uint8_t y = 0; y < gabarito.altura(); y++)
                if (robo.mapa().visitada(x, y))
                    visitadas++;
        TEST_ASSERT_TRUE_MESSAGE(robo.passos() + 1 > visitadas, nome); // +1: a largada
        TEST_ASSERT_TRUE_MESSAGE(robo.passos() > menorCaminho(gabarito), nome);
        conferirMapa(nome, gabarito, robo.mapa());
    }
}

// CT-SE14, passos 1 e 2: depois de explorar, a rota tem o tamanho da referencia
// e so passa por celulas ligadas. A conferencia humana fica em
// docs/guias/conferencia-manual-labirintos.md.
void test_ct_se14_rota_igual_a_referencia()
{
    for (const char *nome : EXPLORADOS)
    {
        Labirinto gabarito;
        carregarDesenho(nome, gabarito);
        RoboSimulado robo(gabarito);
        TEST_ASSERT_TRUE_MESSAGE(robo.explorarAteRotaGarantida(), nome);
        Direcao rota[64];
        uint8_t passos = robo.floodFill().rotaMaisCurta(robo.mapa(), rota, 64);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(menorCaminho(gabarito), passos, nome);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(desenho(nome).movimentosConferidos, passos, nome);
        TEST_ASSERT_TRUE_MESSAGE(rotaValida(gabarito, rota, passos), nome);
    }
}

// CT-SE14: a referencia manual e o script de referencia concordam em todos os
// desenhos. Se discordarem, alguem errou e a ficha precisa ser revista.
void test_ct_se14_conferencia_manual_bate_com_o_script()
{
    for (const auto &f : LABIRINTOS_TESTE)
    {
        Labirinto gabarito;
        carregarDesenho(f.nome, gabarito);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(f.movimentosConferidos, menorCaminho(gabarito), f.nome);
    }
}

// CT-SE14, passo 3: uma parede nova fecha a rota no meio do caminho. A nova
// rota desvia e e a menor do mapa atualizado, ou 0 se nao sobrou caminho.
void test_ct_se14_parede_nova_bloqueia_a_rota()
{
    static const char *const BLOQUEADOS[] = {
        "MISTO_4X4", "BARREIRAS_8X4", "BARREIRAS_12X4", "DUAS_ROTAS",
        "ARMADILHA_4X4", "ARMADILHA_8X4", "ARMADILHA_12X4", "CORREDOR_RETO",
    };
    for (const char *nome : BLOQUEADOS)
    {
        Labirinto mapa;
        carregarDesenho(nome, mapa); // mapa completo, como no fim da exploracao
        FloodFill ff;
        Direcao rota[64];
        uint8_t passos = ff.rotaMaisCurta(mapa, rota, 64);
        TEST_ASSERT_TRUE_MESSAGE(passos > 0, nome);

        uint8_t x = 0;
        uint8_t y = 0;
        for (uint8_t i = 0; i < passos / 2; i++)
        {
            x += deslocamentoX(rota[i]);
            y += deslocamentoY(rota[i]);
        }
        mapa.marcarParede(x, y, rota[passos / 2]);

        uint8_t esperado = menorCaminho(mapa);
        uint8_t novo = ff.rotaMaisCurta(mapa, rota, 64);
        if (esperado == INFINITO)
        {
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, novo, nome);
            continue;
        }
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(esperado, novo, nome);
        TEST_ASSERT_TRUE_MESSAGE(rotaValida(mapa, rota, novo), nome);
    }
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_fixtures_tem_dimensoes_bordas_e_simbolos_validos);
    RUN_TEST(test_objetivos_das_fixtures_sao_alcancaveis);
    RUN_TEST(test_ct_se09_largada_com_saida_a_frente);
    RUN_TEST(test_ct_se09_largada_com_saida_a_direita);
    RUN_TEST(test_ct_se11_mapa_montado_igual_ao_desenho);
    RUN_TEST(test_ct_se11_armadilhas_obrigam_a_revisitar);
    RUN_TEST(test_ct_se14_rota_igual_a_referencia);
    RUN_TEST(test_ct_se14_conferencia_manual_bate_com_o_script);
    RUN_TEST(test_ct_se14_parede_nova_bloqueia_a_rota);
    return UNITY_END();
}
