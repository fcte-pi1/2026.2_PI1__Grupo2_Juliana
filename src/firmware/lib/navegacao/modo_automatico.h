#pragma once

#include <stdint.h>

#include "direcao.h"
#include "floodfill.h"
#include "labirinto.h"

// O que o modo automático precisa do robô. O robô físico implementa com os
// drivers (motores, VL53L0X, BNO085); os testes implementam com um labirinto
// desenhado. Assim a mesma máquina de estados roda nos dois.
class InterfaceRobo
{
public:
    virtual ~InterfaceRobo() {}
    // Prepara sensores e giroscópio na largada. false se algo não respondeu.
    virtual bool calibrar() = 0;
    // Lê os sensores e marca no mapa as paredes da célula atual (frente,
    // esquerda e direita da orientação registrada no mapa).
    virtual void sentir(Labirinto &mapa) = 0;
    // Vira para d e anda uma célula. false se não conseguiu andar.
    virtual bool mover(Direcao d) = 0;
};

// Estados da corrida (RF13-S). Os quatro primeiros nomes são os do campo
// "estado" do Contrato de Telemetria (docs/4.4.7).
enum EstadoRobo : uint8_t
{
    ESTADO_EXPLORANDO,
    ESTADO_RETORNANDO,
    ESTADO_RAPIDO,
    ESTADO_PARADO,
    ESTADO_CALIBRANDO,
    ESTADO_FALHA
};

enum MotivoFalha : uint8_t
{
    SEM_FALHA,
    FALHA_CALIBRACAO,  // sensor ou giroscópio não respondeu na largada
    FALHA_SEM_CAMINHO, // o mapa não tem caminho até o destino
    FALHA_MOVIMENTO,   // o robô não conseguiu andar várias vezes seguidas
    FALHA_LIMITE       // passos demais: proteção contra laço infinito
};

const char *nomeEstado(EstadoRobo e);

// Corrida inteira sem intervenção depois de ligado (RF13-S):
// calibrar -> explorar até o objetivo -> voltar à largada (explorando no
// caminho) -> repetir a ida e volta até a rota rápida estar garantida ->
// correr pela rota mais curta (RF12-S) -> parar no objetivo.
class ModoAutomatico
{
public:
    static const uint8_t MAX_ROTA = 192;           // 12x4 ida e volta, com folga
    static const uint8_t MAX_TENTATIVAS_MOVER = 3; // falhas seguidas antes de desistir

    ModoAutomatico(InterfaceRobo &robo, Labirinto &mapa, uint16_t limitePassos = 2000)
        : robo_(robo), mapa_(mapa), limitePassos_(limitePassos) {}

    // Começa uma corrida. O mapa já deve estar iniciado (tamanho e largada).
    void iniciar();
    // Executa uma etapa: a calibração ou uma célula. Devolve o novo estado.
    EstadoRobo passo();
    // Roda até ESTADO_PARADO ou ESTADO_FALHA.
    EstadoRobo executar();

    EstadoRobo estado() const { return estado_; }
    MotivoFalha motivo() const { return motivo_; }
    uint16_t passos() const { return passos_; }          // células andadas no total
    uint8_t passosRapido() const { return tamanhoRota_; } // tamanho da rota rápida
    uint8_t viagens() const { return viagens_; }          // idas e voltas de exploração
    const FloodFill &floodFill() const { return floodFill_; }

private:
    void falhar(MotivoFalha m);
    void navegarAte(uint8_t x, uint8_t y);
    bool andar(Direcao d);
    void chegouAoObjetivo();
    void chegouNaLargada();

    InterfaceRobo &robo_;
    Labirinto &mapa_;
    FloodFill floodFill_;
    EstadoRobo estado_ = ESTADO_PARADO;
    MotivoFalha motivo_ = SEM_FALHA;
    uint16_t limitePassos_;
    uint16_t passos_ = 0;
    uint8_t falhasMover_ = 0;
    uint8_t objetivoX_ = 0;
    uint8_t objetivoY_ = 0;
    uint8_t viagens_ = 0;
    Direcao rota_[MAX_ROTA];
    uint8_t tamanhoRota_ = 0;
    uint8_t indiceRota_ = 0;
};
