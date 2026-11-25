#include "calibracao_giro.h"

#include <math.h>
#include <stdlib.h>

#include "filtro_distancia.h" // DIST_MAX_CONFIAVEL

namespace
{
const float DOIS_PI = 6.2831853f;
// A volta de verdade é procurada entre 0,55 e 1,2 volta da largura nominal: a
// meia volta e 1,5 volta ficam de fora (a nominal pode errar de -15% a +80%).
const float BUSCA_MIN = 0.55f, BUSCA_MAX = 1.2f;
const float JANELA = 0.25f;       // trecho comparado: 1/4 de volta
const uint16_t MAX_PASSOS = 1024; // 1 grau (pela largura nominal) por passo
const float ERRO_MAXIMO_MM = 15;  // erro médio aceito entre as duas voltas
const float CONTRASTE_MINIMO_MM = 30;

uint16_t limitar(uint16_t mm) { return (mm == 0 || mm > DIST_MAX_CONFIAVEL) ? DIST_MAX_CONFIAVEL : mm; }
} // namespace

float giroNecessarioMm(float larguraNominalMm)
{
    return (BUSCA_MAX + JANELA + 0.05f) * DOIS_PI * larguraNominalMm;
}

float larguraEixoPeloGiro(const float *diferencaMm, const uint16_t (*distancias)[3], uint16_t n,
                          float larguraNominalMm, float *erroMedioMm)
{
    if (erroMedioMm)
        *erroMedioMm = -1;
    if (n < 2)
        return 0;

    // Reamostra as distâncias a cada grau (pela largura nominal) de diferença
    // dos encoders: a velocidade do giro varia, o ângulo é o que importa.
    // Recuos pequenos da diferença (folga, ruído) são ignorados.
    const float passo = DOIS_PI * larguraNominalMm / 360;
    static uint16_t r[MAX_PASSOS][3];
    uint16_t k = 0, i = 0;
    float maximo = diferencaMm[0];
    while (k < MAX_PASSOS)
    {
        const float x = k * passo;
        while (i + 1 < n && fmaxf(maximo, diferencaMm[i + 1]) < x)
            maximo = fmaxf(maximo, diferencaMm[++i]);
        if (i + 1 >= n)
            break;
        float a = maximo, b = fmaxf(maximo, diferencaMm[i + 1]);
        float t = b > a ? (x - a) / (b - a) : 0;
        t = t < 0 ? 0 : (t > 1 ? 1 : t);
        for (uint8_t j = 0; j < 3; j++)
        {
            float va = limitar(distancias[i][j]), vb = limitar(distancias[i + 1][j]);
            r[k][j] = (uint16_t)lroundf(va + t * (vb - va));
        }
        k++;
    }

    const float volta = 360; // passos
    const int janela = (int)lroundf(JANELA * volta);
    const int sMin = (int)lroundf(BUSCA_MIN * volta);
    int sMax = (int)lroundf(BUSCA_MAX * volta);
    if (sMax > k - 1 - janela)
        sMax = k - 1 - janela;
    if (janela < 4 || sMax <= sMin + 2)
        return 0;

    // Só o sensor da frente: é o que aponta para a parede de referência
    // (chão aberto, 23/11/2026: os dos lados só viam móveis longe, com ruído).
    const uint8_t F = 1;
    // Sem parede perto, a distância não muda e qualquer volta "bate".
    uint16_t menor = 0xFFFF, maior = 0;
    for (int a = 0; a < janela; a++)
    {
        menor = r[a][F] < menor ? r[a][F] : menor;
        maior = r[a][F] > maior ? r[a][F] : maior;
    }
    if (maior - menor < CONTRASTE_MINIMO_MM)
        return 0;

    auto custo = [&](int s) {
        uint32_t soma = 0;
        for (int a = 0; a < janela; a++)
            soma += (uint32_t)abs((int)r[a][F] - (int)r[a + s][F]);
        return soma / (float)janela;
    };
    int melhor = sMin;
    float menorCusto = 1e9f;
    for (int s = sMin; s <= sMax; s++)
    {
        float c = custo(s);
        if (c < menorCusto)
        {
            menorCusto = c;
            melhor = s;
        }
    }
    if (erroMedioMm)
        *erroMedioMm = menorCusto;
    // No limite da busca o mínimo de verdade pode estar fora dela.
    if (melhor == sMin || melhor == sMax || menorCusto > ERRO_MAXIMO_MM)
        return 0;

    // Parábola pelos três custos em volta do mínimo: fração de passo.
    float c0 = custo(melhor - 1), c2 = custo(melhor + 1);
    float den = c0 - 2 * menorCusto + c2;
    float ajuste = den > 0 ? 0.5f * (c0 - c2) / den : 0;
    return (melhor + ajuste) * passo / DOIS_PI;
}
