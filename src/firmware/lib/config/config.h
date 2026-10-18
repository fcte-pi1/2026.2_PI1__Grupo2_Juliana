#pragma once

#include <stdint.h>

namespace config {
// TODO: calibrar com a montagem real; nao sao evidencias de precisao fisica.
constexpr float FILTRO_ALPHA = 0.3f;
constexpr uint16_t PAREDE_LIMIAR_MM = 120;
constexpr uint16_t PAREDE_HISTERESE_MM = 10;
// Sentinelas/leituras fora de faixa do planejamento da #211.
constexpr uint16_t TOF_LIMITE_INVALIDO_MM = 8190;
}
