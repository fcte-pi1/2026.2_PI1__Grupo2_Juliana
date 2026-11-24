#pragma once

#include <stdint.h>

// Monitor da bateria LiPo 2S pelo INA226 (#216, #277). Lógica pura: roda no
// robô e nos testes do computador. Regras do Contrato de Telemetria (4.4.7,
// "Bateria, dois níveis"), as mesmas do painel:
//  - aviso: < 7,0 V por 1 s -> evento bateria_critica; só repete depois de
//    voltar a >= 7,2 V. A corrida segue.
//  - parada: < 6,6 V por 1 s -> motores param, fim abandonado (bateria_critica).
//  - INA226 sem resposta por 1 s -> motores param, fim abandonado
//    (falha_sensor_bateria).
//  - abaixo de 6,0 V considera que não há bateria (placa só no USB: a
//    tensão que aparece é a do USB voltando pelo MP1584) e não gera evento.

enum EventoBateria : uint8_t
{
    BATERIA_OK,
    BATERIA_AVISO,       // mandar o evento bateria_critica (uma vez)
    BATERIA_PARAR,       // parar os motores e encerrar a corrida
    BATERIA_FALHA_SENSOR // parar: INA226 sem leitura válida
};

class MonitorBateria
{
public:
    // Calibração: valores do contrato e do 4.2 (LiPo 2S 1500 mAh).
    static constexpr float AVISO_V = 7.0f;
    static constexpr float REARME_V = 7.2f;
    static constexpr float PARADA_V = 6.6f;
    static constexpr float MINIMO_V = 6.4f;   // 0 % (igual ao painel)
    static constexpr float CHEIA_V = 8.4f;    // 100 %
    static constexpr float SEM_BATERIA_V = 6.0f;
    static constexpr float CAPACIDADE_MAH = 1500.0f;
    static constexpr float PROFUNDIDADE_DESCARGA = 0.8f; // 80 % utilizável (4.2)
    static const uint32_t PERSISTENCIA_MS = 1000;
    static const uint8_t JANELA = 8; // média móvel (5 Hz -> ~1,6 s)

    // Chamar a cada leitura do INA226. valida = o INA226 respondeu.
    // Devolve o que o firmware precisa fazer agora.
    EventoBateria atualizar(uint32_t agora_ms, bool valida, float tensao_v, float corrente_ma);

    bool presente() const { return presente_; }      // há bateria ligada
    float tensao() const;                              // média móvel, V
    float corrente() const;                            // média móvel, mA
    float consumoMedio() const;                        // média desde iniciarConsumo(), mA
    uint8_t percentual() const;                        // 0 a 100, linear entre 6,4 e 8,4 V
    float autonomiaMinutos() const;                    // estimativa com a corrente média; -1 sem corrente
    bool critica() const { return avisoAtivo_; }
    void iniciarConsumo();                             // zera a média da corrida

private:
    // Condição que precisa durar PERSISTENCIA_MS seguidos.
    struct Persistencia
    {
        bool ativa = false;
        uint32_t desde = 0;
        bool durou(bool condicao, uint32_t agora);
    };

    float tensoes_[JANELA] = {};
    float correntes_[JANELA] = {};
    uint8_t n_ = 0, prox_ = 0;
    bool presente_ = false;
    bool avisoAtivo_ = false;
    bool parado_ = false;
    Persistencia aviso_, parada_, falha_;
    double somaConsumo_ = 0;
    uint32_t amostrasConsumo_ = 0;
};
