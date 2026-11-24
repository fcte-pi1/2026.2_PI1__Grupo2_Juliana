#pragma once

#include <stdint.h>

// Filtro de ruído e detecção de paredes dos VL53L0X (RF14-S, RF01-S).
// Lógica pura, sem Arduino: roda no robô e nos testes do computador.

// O VL53L0X devolve 8190/8191 quando não há alvo no alcance.
const uint16_t DIST_SEM_ALVO = 8190;
// Acima disso a leitura também conta como "sem alvo": o sensor perde precisão
// e nenhuma parede do labirinto (células de 18 cm) fica tão longe.
const uint16_t DIST_MAX_CONFIAVEL = 1200;

// Mediana das últimas leituras de um sensor. A mediana de 5 ignora até 2
// picos seguidos (vibração, reflexo) sem atrasar a resposta como a média.
// Leitura 0 ou timeout é falha do sensor: não entra na janela e, depois de
// MAX_FALHAS seguidas, o sensor é dado como com defeito (RF14-S).
class FiltroDistancia
{
public:
    static const uint8_t JANELA = 5;
    static const uint8_t MAX_FALHAS = 10;

    void adicionar(uint16_t mm, bool timeout = false);
    void limpar();

    bool pronto() const { return quantidade_ > 0; }   // já tem leitura válida
    uint16_t valor() const;                           // mediana, ou DIST_SEM_ALVO
    bool semAlvo() const { return valor() >= DIST_SEM_ALVO; }
    bool defeito() const { return falhasSeguidas_ >= MAX_FALHAS; }
    uint16_t falhasSeguidas() const { return falhasSeguidas_; }

private:
    uint16_t janela_[JANELA] = {};
    uint8_t proxima_ = 0;
    uint8_t quantidade_ = 0;
    uint16_t falhasSeguidas_ = 0;
};

// Decide se há parede a partir da distância filtrada, com histerese: liga
// abaixo de limiarLiga e só desliga acima de limiarDesliga, para a parede não
// piscar quando a leitura fica perto do limiar (robô vibrando).
class DetectorParede
{
public:
    DetectorParede(uint16_t limiarLiga = 120, uint16_t limiarDesliga = 140)
        : liga_(limiarLiga), desliga_(limiarDesliga) {}

    bool atualizar(uint16_t mm);
    bool parede() const { return parede_; }
    void configurar(uint16_t limiarLiga, uint16_t limiarDesliga);
    uint16_t limiarLiga() const { return liga_; }
    uint16_t limiarDesliga() const { return desliga_; }

private:
    uint16_t liga_;
    uint16_t desliga_;
    bool parede_ = false;
};
