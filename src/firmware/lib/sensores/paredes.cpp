#include "paredes.h"
#include <math.h>

DetectorParede::DetectorParede(uint16_t limiar, uint16_t histerese)
    : limiar_(limiar), histerese_(histerese)
{
}

void DetectorParede::reiniciar() { estado_ = Parede::DESCONHECIDA; }

Parede DetectorParede::atualizar(DistanciaFiltrada leitura)
{
    if (leitura.estado != EstadoLeitura::VALIDA || !isfinite(leitura.mm) || leitura.mm <= 0 ||
        leitura.mm >= config::TOF_LIMITE_INVALIDO_MM || limiar_ <= histerese_ ||
        static_cast<uint32_t>(limiar_) + histerese_ >= config::TOF_LIMITE_INVALIDO_MM)
    {
        reiniciar();
        return estado_;
    }
    if (leitura.mm < limiar_ - histerese_)
        estado_ = Parede::PRESENTE;
    else if (leitura.mm > limiar_ + histerese_)
        estado_ = Parede::LIVRE;
    // Dentro da banda mantem o estado; no inicio permanece desconhecido.
    return estado_;
}

ParedesAbsolutas paredesAbsolutas(Parede frente, Parede esquerda, Parede direita,
                                  Direcao orientacao, Parede tras)
{
    ParedesAbsolutas resultado = {0, 0};
    if (orientacao != NORTE && orientacao != LESTE && orientacao != SUL && orientacao != OESTE)
        return resultado;
    const Parede relativas[] = {frente, esquerda, direita, tras};
    const Direcao absolutas[] = {orientacao, girarEsquerda(orientacao), girarDireita(orientacao),
                                 oposta(orientacao)};
    for (uint8_t i = 0; i < 4; ++i)
    {
        if (relativas[i] == Parede::PRESENTE)
        {
            resultado.presentes |= absolutas[i];
            resultado.conhecidas |= absolutas[i];
        }
        else if (relativas[i] == Parede::LIVRE)
        {
            resultado.conhecidas |= absolutas[i];
        }
    }
    return resultado;
}
