#pragma once

#include <stdint.h>

namespace config
{
// TODO: calibrar na montagem real.
constexpr float FILTRO_ALPHA = 0.3f;
constexpr uint16_t PAREDE_LIMIAR_MM = 120;
constexpr uint16_t PAREDE_HISTERESE_MM = 10;
constexpr uint16_t TOF_LIMITE_INVALIDO_MM = 8190;
static_assert(FILTRO_ALPHA > 0 && FILTRO_ALPHA <= 1, "alpha fora de (0,1]");
static_assert(PAREDE_HISTERESE_MM < PAREDE_LIMIAR_MM, "histerese >= limiar");
static_assert(static_cast<uint32_t>(PAREDE_LIMIAR_MM) + PAREDE_HISTERESE_MM <
                  TOF_LIMITE_INVALIDO_MM,
              "limiar acima do alcance");
} // namespace config
