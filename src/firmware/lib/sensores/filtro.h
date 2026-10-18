#pragma once

#include <stdint.h>
#include "config.h"

enum class EstadoLeitura : uint8_t
{
    AQUECENDO,
    VALIDA,
    INVALIDA
};

struct DistanciaFiltrada
{
    float mm;
    EstadoLeitura estado;
};

// Uma instancia por VL53L0X. O driver deve chamar uma vez por amostra NOVA.
// Timeout, erro de comunicacao/range ou ausencia de dado novo => sensorOk=false.
class FiltroDistancia
{
  public:
    explicit FiltroDistancia(float alpha = config::FILTRO_ALPHA);
    DistanciaFiltrada atualizar(uint16_t brutoMm, bool sensorOk = true);
    void reiniciar();

  private:
    float alpha_;
    uint16_t amostras_[5] = {};
    uint8_t quantidade_ = 0;
    uint8_t proxima_ = 0;
    float suavizada_ = 0;
    bool iniciouEma_ = false;
};
