#include "filtro.h"

FiltroDistancia::FiltroDistancia(float alpha) : alpha_(alpha) {}

void FiltroDistancia::reiniciar()
{
    quantidade_ = 0;
    proxima_ = 0;
    suavizada_ = 0;
    iniciouEma_ = false;
}

DistanciaFiltrada FiltroDistancia::atualizar(uint16_t brutoMm, bool sensorOk)
{
    if (!(alpha_ > 0 && alpha_ <= 1) || !sensorOk || brutoMm == 0 ||
        brutoMm >= config::TOF_LIMITE_INVALIDO_MM)
    {
        reiniciar();
        return {0, EstadoLeitura::INVALIDA};
    }
    amostras_[proxima_] = brutoMm;
    proxima_ = (proxima_ + 1) % 5;
    if (quantidade_ < 5)
        ++quantidade_;
    if (quantidade_ < 5)
        return {0, EstadoLeitura::AQUECENDO};

    uint16_t ordenadas[5];
    for (uint8_t i = 0; i < 5; ++i)
        ordenadas[i] = amostras_[i];
    for (uint8_t i = 1; i < 5; ++i)
    {
        uint16_t valor = ordenadas[i];
        uint8_t j = i;
        while (j > 0 && ordenadas[j - 1] > valor)
        {
            ordenadas[j] = ordenadas[j - 1];
            --j;
        }
        ordenadas[j] = valor;
    }
    const float mediana = ordenadas[2];
    suavizada_ = iniciouEma_ ? alpha_ * mediana + (1 - alpha_) * suavizada_ : mediana;
    iniciouEma_ = true;
    return {suavizada_, EstadoLeitura::VALIDA};
}
