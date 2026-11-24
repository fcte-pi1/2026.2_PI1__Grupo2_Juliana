#pragma once

#include <stdint.h>

#include "direcao.h"

// Leitura dos sensores do robô: 3 VL53L0X (com filtro e detecção de parede),
// BNO085 (yaw) e INA226 (tensão e corrente da bateria). Só roda no ESP32.

enum LadoSensor : uint8_t { LADO_ESQUERDO, LADO_FRONTAL, LADO_DIREITO };

struct LeituraSensores
{
    uint16_t distancia_mm[3]; // filtrada; DIST_SEM_ALVO se nada no alcance
    bool parede[3];
    bool defeito[3];           // VL53L0X sem resposta
    bool imuOk;
    float yawGraus;            // relativo à largada; positivo = giro à esquerda
    bool inaOk;
    float tensao_v;            // ~0 V sem bateria (só USB)
    float corrente_ma;
};

class Sensores
{
public:
    // Liga e configura tudo. Devolve quantos dos 5 sensores responderam
    // (3 VL53L0X, BNO085 e INA226).
    uint8_t iniciar();
    // Lê todos os sensores e atualiza os filtros. Chamar no laço principal.
    void atualizar();
    const LeituraSensores &leitura() const { return leitura_; }
    // A orientação atual vira o "Norte" do mapa (largada).
    void zerarYaw();
    // Orientação absoluta do mapa a partir do yaw (múltiplo de 90° mais perto).
    Direcao orientacao() const;
    // Máscara de paredes do contrato (N=1, L=2, S=4, O=8) vista dos 3 sensores.
    uint8_t mascaraParedes() const;

private:
    void lerImu();
    void lerIna();
    LeituraSensores leitura_ = {};
    float yawBruto_ = 0;
    float yawZero_ = 0;
};
