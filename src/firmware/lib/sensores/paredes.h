#pragma once

#include "filtro.h"
#include "direcao.h"

enum class Parede : uint8_t
{
    DESCONHECIDA,
    LIVRE,
    PRESENTE
};

class DetectorParede
{
  public:
    DetectorParede(uint16_t limiar = config::PAREDE_LIMIAR_MM,
                   uint16_t histerese = config::PAREDE_HISTERESE_MM);
    Parede atualizar(DistanciaFiltrada leitura);
    void reiniciar();

  private:
    uint16_t limiar_;
    uint16_t histerese_;
    Parede estado_ = Parede::DESCONHECIDA;
};

struct ParedesAbsolutas
{
    uint8_t presentes;
    uint8_t conhecidas;
};

// Traseira so e conhecida apos observacao ou passagem confirmada nessa aresta.
// Desconhecida NAO significa livre. Aplicar ao mapa apenas bits de conhecidas.
ParedesAbsolutas paredesAbsolutas(Parede frente, Parede esquerda, Parede direita,
                                  Direcao orientacao, Parede tras = Parede::DESCONHECIDA);
