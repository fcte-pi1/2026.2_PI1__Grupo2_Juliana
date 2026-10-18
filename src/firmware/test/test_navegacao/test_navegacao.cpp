// Valida os desenhos de teste; ainda nao executa o Flood Fill.
#include <unity.h>
#include <string.h>
#include "labirintos.h"

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

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_fixtures_tem_dimensoes_bordas_e_simbolos_validos);
    RUN_TEST(test_objetivos_das_fixtures_sao_alcancaveis);
    return UNITY_END();
}
