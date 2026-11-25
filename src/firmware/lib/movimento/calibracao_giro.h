#pragma once

#include <stdint.h>

// Calibração do giro sem o BNO085 (comando R). Sem giroscópio, o rumo sai da
// diferença dos encoders: rumo += (dD - dE) / larguraEixoMm. A largura que vale
// não é a medida com a régua: a roda patina girando no lugar. Aqui ela é medida
// pelo próprio giro: o robô gira no lugar umas 1,5 volta e a distância do
// sensor da frente desenha as paredes em volta dele. Esse desenho se repete a cada
// volta de verdade; a diferença dos encoders entre as duas repetições é
// 2 * pi * largura efetiva.
//
// diferencaMm[i]: (encoder direito - encoder esquerdo) em mm desde o começo,
// crescendo (girando à esquerda). distancias[i]: esquerda, frente, direita.
// Devolve a largura efetiva (mm) ou 0 se o desenho não se repetiu (sem
// paredes, o robô saiu do lugar ou girou menos que o necessário).
//
// Robô de frente para uma parede (no chão, uma parede do labirinto em pé na
// frente dele; ou o beco da largada). Numa célula simétrica (só corredor) o
// desenho também se repete a cada meia volta.
float larguraEixoPeloGiro(const float *diferencaMm, const uint16_t (*distancias)[3], uint16_t n,
                          float larguraNominalMm, float *erroMedioMm = nullptr);

// Quanto o robô precisa girar (diferença dos encoders, mm) para a calibração.
float giroNecessarioMm(float larguraNominalMm);
