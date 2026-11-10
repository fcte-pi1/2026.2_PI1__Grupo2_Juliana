#pragma once

#include <stdint.h>

#include "pacote.h"

struct EstatisticasTelemetria
{
    bool wifi;          // Wi-Fi associado
    bool servidor;      // WebSocket aberto com o backend
    uint32_t enviados;  // quadros enviados (inclui reenvios)
    uint32_t reenvios;
    uint32_t acks;
    uint32_t quedas;
    uint32_t pendentes;  // no buffer, sem ack
    uint32_t ocupacaoMaxima;
    uint32_t descartes;  // overflow + tentativas esgotadas
    // RTT envio -> ack, em ms (limite superior do atraso robô -> painel)
    uint32_t rttAmostras;
    uint32_t rttUltimo;
    uint32_t rttMedio;
    uint32_t rttMax;
    uint32_t rttAcima500;
};

// Telemetria por Wi-Fi (RF05-S, RF25-S, RNF12-S, RNF15-S) numa tarefa própria,
// separada da navegação: o laço principal só chama publicar(), que nunca
// espera a rede. As mensagens ficam no BufferTelemetria até o ack do servidor;
// se a rede cair, continuam guardadas e são enviadas na volta, depois do
// evento 'reconexao' (Contrato de Telemetria, docs/4.4.7, seção 6).
namespace TelemetriaWifi
{
// Reserva o buffer (~100 KB contínuos). Chamar no começo do setup(), antes de
// qualquer outra alocação grande. false se não houver bloco livre desse tamanho.
bool reservarMemoria();

// Liga o Wi-Fi e cria a tarefa. Sem SSID, não faz nada e devolve false.
bool iniciar(const char *ssid, const char *senha, const char *host, uint16_t porta);

// Começa uma corrida: o seq volta a 0 e o tempo conta a partir de agora.
void novaCorrida(uint32_t corrida, uint32_t inicio_ms);
// Encerra a corrida atual depois do evento 'fim' (sem mais reconexões).
void encerrarCorrida();

// Coloca a mensagem na fila com o próximo seq da corrida. Não bloqueia.
// false se o buffer recusou (cheio só de eventos).
bool publicar(MensagemTelemetria msg);

uint32_t corridaAtual();
void zerarRtt();
EstatisticasTelemetria estatisticas();
} // namespace TelemetriaWifi
