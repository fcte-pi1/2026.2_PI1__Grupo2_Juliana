#pragma once

#include <stdint.h>

#include "direcao.h"

// Navegação no núcleo 1.
//
// A ESP32 da placa tem um bit da cache do núcleo 1 grudado em 0 (docs/7.3,
// teste 3.18). O núcleo 1 só roda o que o nucleo1_risco.py libera: a
// compilação falha se alguma função chamada daqui cair num byte defeituoso
// (custom_nucleo1 no platformio.ini). Por isso esta tarefa não chama Arduino,
// Wire, Serial nem printf: os sensores são lidos no núcleo 0 e chegam aqui
// por atualizarSensores().
//
// Na bancada (robô parado na largada) ela:
//  - marca no mapa as paredes que os sensores veem e recalcula o Flood Fill
//    a cada 20 ms; o valor vai na telemetria (valor_flood);
//  - uma vez por segundo roda uma corrida completa simulada com o modo
//    automático, como carga, e confere a rota.
namespace Navegacao
{
struct Estado
{
    int16_t valorFlood;       // distância da largada ao objetivo, -1 se ainda não calculou
    uint8_t nucleo;           // núcleo onde a tarefa está rodando (deve ser 1)
    uint32_t ciclos;          // recálculos do Flood Fill
    uint32_t tempoMedioUs;    // tempo de um recálculo
    uint32_t tempoMaxUs;
    uint32_t corridasOk;      // corridas simuladas com a rota certa
    uint32_t corridasErradas;
};

// Cria a tarefa no núcleo 1, com o mapa do tamanho do labirinto.
void iniciar(uint8_t largura, uint8_t altura);
// Núcleo 0 -> núcleo 1: o que os sensores veem agora, já com a orientação.
void atualizarSensores(bool paredeFrente, bool paredeEsquerda, bool paredeDireita, Direcao orientacao);
Estado estado();
} // namespace Navegacao
