#pragma once

#include <stddef.h>
#include <stdint.h>

#include "direcao.h"
#include "labirinto.h"

// Como tratar um lado que o robô ainda não viu.
enum ModoFlood : uint8_t
{
    EXPLORACAO, // desconhecido = aberto: o robô vai até lá conferir
    RAPIDO      // desconhecido = fechado: só passa por onde já tem certeza
};

// Distância de uma célula que não alcança o objetivo
const uint8_t INFINITO = 255;

// Flood Fill: distância, em células, de cada célula até o objetivo,
// recalculada a cada parede nova (RF11). Memória fixa (RNF03).
class FloodFill
{
public:
    void recalcular(const Labirinto &lab, ModoFlood modo);
    uint8_t distancia(uint8_t x, uint8_t y) const; 
    uint8_t valorAtual(const Labirinto &lab) const;

    // Direção do vizinho com menor distância, a partir da pose do robô.
    // Empate: reto, direita, esquerda e trás (menos giros). Num beco, só
    // trás é passável e a resposta é o giro de 180° (RF23). Na largada, se a
    // frente estiver fechada, a saída do beco é o lado oposto à borda (ver
    // labirinto.h).
    // false se a célula atual não tem caminho até o objetivo.
    bool proximaDirecao(const Labirinto &lab, Direcao &saida) const;

    // Recalcula em modo RAPIDO e grava em saida as direções da largada (0,0)
    // até o objetivo (RF12), partindo da orientação inicial. Devolve o número
    // de passos, ou 0 se não houver rota ou se ela não couber em max.
    uint8_t rotaMaisCurta(const Labirinto &lab, Direcao saida[], uint8_t max);

    // Se a rota rápida já é a menor possível, mesmo sem conhecer o labirinto
    // inteiro (CT-SE14). EXPLORACAO dá a menor distância que o labirinto real
    // poderia ter e RAPIDO a que ele com certeza tem; se as duas da largada
    // forem iguais, nenhum lado desconhecido encurtaria a rota. Deixa as
    // distâncias calculadas em modo RAPIDO.
    bool rotaGarantida(const Labirinto &lab);

private:
    bool passavel(const Labirinto &lab, uint8_t x, uint8_t y, Direcao d) const;
    bool escolher(const Labirinto &lab, uint8_t x, uint8_t y, Direcao frente,
                  uint8_t limite, Direcao &saida) const;

    uint8_t dist_[MAX_LARG][MAX_ALT];
    ModoFlood modo_ = EXPLORACAO;
};

// Lê um labirinto desenhado em ASCII e grava em gabarito, com todos os
// lados conhecidos. O objetivo fica em (largura-1, altura-1).
// orientacaoInicial é para onde o robô aponta na largada (ver labirinto.h).
bool carregar(const char *ascii, Labirinto &gabarito, Direcao orientacaoInicial = NORTE);

// Desenha o mapa no mesmo formato de carregar(), com a distância do Flood Fill
// dentro de cada célula (evidência do CT-SE11).
bool imprimirMapa(const Labirinto &lab, const FloodFill &ff, char *buf, size_t tamanho);
