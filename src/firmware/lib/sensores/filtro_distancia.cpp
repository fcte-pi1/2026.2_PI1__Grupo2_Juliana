#include "filtro_distancia.h"

void FiltroDistancia::adicionar(uint16_t mm, bool timeout)
{
    if (timeout || mm == 0)
    {
        if (falhasSeguidas_ < UINT16_MAX)
            falhasSeguidas_++;
        return;
    }
    falhasSeguidas_ = 0;
    if (mm > DIST_MAX_CONFIAVEL)
        mm = DIST_SEM_ALVO;
    janela_[proxima_] = mm;
    proxima_ = (uint8_t)((proxima_ + 1) % JANELA);
    if (quantidade_ < JANELA)
        quantidade_++;
}

void FiltroDistancia::limpar()
{
    proxima_ = 0;
    quantidade_ = 0;
    falhasSeguidas_ = 0;
}

uint16_t FiltroDistancia::valor() const
{
    if (quantidade_ == 0)
        return DIST_SEM_ALVO;
    uint16_t ordenada[JANELA];
    for (uint8_t i = 0; i < quantidade_; i++)
        ordenada[i] = janela_[i];
    // Inserção: no máximo 5 elementos.
    for (uint8_t i = 1; i < quantidade_; i++)
    {
        uint16_t v = ordenada[i];
        int8_t j = (int8_t)(i - 1);
        while (j >= 0 && ordenada[j] > v)
        {
            ordenada[j + 1] = ordenada[j];
            j--;
        }
        ordenada[j + 1] = v;
    }
    return ordenada[quantidade_ / 2];
}

bool DetectorParede::atualizar(uint16_t mm)
{
    if (parede_ && mm > desliga_)
        parede_ = false;
    else if (!parede_ && mm < liga_)
        parede_ = true;
    return parede_;
}

void DetectorParede::configurar(uint16_t limiarLiga, uint16_t limiarDesliga)
{
    liga_ = limiarLiga;
    desliga_ = limiarDesliga < limiarLiga ? limiarLiga : limiarDesliga;
}
