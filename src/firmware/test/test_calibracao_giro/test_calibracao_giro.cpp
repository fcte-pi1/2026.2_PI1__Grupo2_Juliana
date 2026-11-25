// Calibração do giro sem BNO085 (comando R): a largura efetiva do eixo sai do
// desenho das paredes que se repete a cada volta.
#include <math.h>
#include <stdlib.h>
#include <unity.h>

#include "calibracao_giro.h"

void setUp() {}
void tearDown() {}

static const uint16_t N = 600;
static float diferenca[N];
static uint16_t dist[N][3];

// Raio do ponto (x, y) na direção a até as paredes de um beco de 18 cm de
// largura: fundo em y = -90, lados em x = ±90, aberto até y = 630.
static float raio(float x, float y, float a)
{
    float c = cosf(a), s = sinf(a), t = 1e9f;
    if (c > 1e-6f)
        t = fminf(t, (90 - x) / c);
    if (c < -1e-6f)
        t = fminf(t, (-90 - x) / c);
    if (s > 1e-6f)
        t = fminf(t, (630 - y) / s);
    if (s < -1e-6f)
        t = fminf(t, (-90 - y) / s);
    return t;
}

// Gira até 'total' mm de diferença dos encoders; o rumo de verdade é
// diferença / larguraReal. Sensores a 40 mm do centro, ruído de ±3 mm e passos
// irregulares dos encoders.
// Chão aberto: só uma parede do labirinto (18 cm) em pé a 10 cm na frente.
static bool soUmaParede = false;
static float raioUmaParede(float x, float y, float a)
{
    float s = sinf(a);
    if (s < 1e-6f)
        return 1200;
    float t = (100 - y) / s, xp = x + t * cosf(a);
    return fabsf(xp) <= 90 ? t : 1200;
}

static uint16_t simularGiro(float larguraReal, float total, bool semParedes = false)
{
    srand(7);
    float d = 0;
    uint16_t n = 0;
    while (d < total && n < N)
    {
        float a = 1.5707963f + d / larguraReal; // começa virado para a saída (+y)
        float x = 4 * sinf(d / 50), y = -6 + 3 * cosf(d / 70); // o centro balança um pouco
        const float lados[3] = {a + 1.5707963f, a, a - 1.5707963f}; // esquerda, frente, direita
        for (int j = 0; j < 3; j++)
        {
            float mm = semParedes ? 1200 : (soUmaParede ? raioUmaParede(x, y, lados[j]) : raio(x, y, lados[j])) - 40 + (rand() % 7 - 3);
            dist[n][j] = (uint16_t)(mm > 1200 ? 1200 : (mm < 1 ? 1 : mm));
        }
        diferenca[n++] = d;
        d += 1.0f + (rand() % 30) / 10.0f; // 1 a 4 mm por amostra
    }
    return n;
}

void test_mede_a_largura_efetiva_maior_que_a_nominal()
{
    uint16_t n = simularGiro(104, giroNecessarioMm(95));
    float erro = 0;
    float l = larguraEixoPeloGiro(diferenca, dist, n, 95, &erro);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 104, l); // 1% = 3,6° por volta
    TEST_ASSERT_TRUE(erro >= 0 && erro < 10);
}

void test_mede_a_largura_efetiva_menor_que_a_nominal()
{
    uint16_t n = simularGiro(88, giroNecessarioMm(95));
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 88, larguraEixoPeloGiro(diferenca, dist, n, 95));
}

void test_sem_paredes_nao_calibra()
{
    uint16_t n = simularGiro(104, giroNecessarioMm(95), true);
    TEST_ASSERT_EQUAL_FLOAT(0, larguraEixoPeloGiro(diferenca, dist, n, 95));
}

void test_giro_curto_demais_nao_calibra()
{
    uint16_t n = simularGiro(104, 0.9f * 6.2831853f * 95);
    TEST_ASSERT_EQUAL_FLOAT(0, larguraEixoPeloGiro(diferenca, dist, n, 95));
}

void test_mede_com_uma_parede_so_na_frente_no_chao_aberto()
{
    soUmaParede = true;
    uint16_t n = simularGiro(150, giroNecessarioMm(170));
    soUmaParede = false;
    TEST_ASSERT_FLOAT_WITHIN(1.5f, 150, larguraEixoPeloGiro(diferenca, dist, n, 170));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_mede_a_largura_efetiva_maior_que_a_nominal);
    RUN_TEST(test_mede_a_largura_efetiva_menor_que_a_nominal);
    RUN_TEST(test_sem_paredes_nao_calibra);
    RUN_TEST(test_giro_curto_demais_nao_calibra);
    RUN_TEST(test_mede_com_uma_parede_so_na_frente_no_chao_aberto);
    return UNITY_END();
}
