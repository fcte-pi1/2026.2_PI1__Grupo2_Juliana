#include "monitor_bateria.h"

bool MonitorBateria::Persistencia::durou(bool condicao, uint32_t agora)
{
    if (!condicao)
    {
        ativa = false;
        return false;
    }
    if (!ativa)
    {
        ativa = true;
        desde = agora;
    }
    return agora - desde >= PERSISTENCIA_MS;
}

EventoBateria MonitorBateria::atualizar(uint32_t agora, bool valida, float tensao_v, float corrente_ma)
{
    if (parado_)
        return BATERIA_OK; // já mandou parar; só volta com reset

    // INA226 sem resposta: só conta se havia bateria (no USB pode nem ter INA).
    if (falha_.durou(!valida && presente_, agora))
    {
        parado_ = true;
        return BATERIA_FALHA_SENSOR;
    }
    if (!valida)
        return BATERIA_OK;

    tensoes_[prox_] = tensao_v;
    correntes_[prox_] = corrente_ma;
    prox_ = (uint8_t)((prox_ + 1) % JANELA);
    if (n_ < JANELA)
        n_++;
    somaConsumo_ += corrente_ma;
    amostrasConsumo_++;

    float v = tensao();
    presente_ = v >= SEM_BATERIA_V;
    if (!presente_)
    {
        aviso_.ativa = parada_.ativa = false;
        return BATERIA_OK;
    }

    if (parada_.durou(v < PARADA_V, agora))
    {
        parado_ = true;
        return BATERIA_PARAR;
    }
    if (v >= REARME_V)
        avisoAtivo_ = false;
    if (!avisoAtivo_ && aviso_.durou(v < AVISO_V, agora))
    {
        avisoAtivo_ = true;
        return BATERIA_AVISO;
    }
    return BATERIA_OK;
}

float MonitorBateria::tensao() const
{
    float s = 0;
    for (uint8_t i = 0; i < n_; i++)
        s += tensoes_[i];
    return n_ ? s / n_ : 0;
}

float MonitorBateria::corrente() const
{
    float s = 0;
    for (uint8_t i = 0; i < n_; i++)
        s += correntes_[i];
    return n_ ? s / n_ : 0;
}

void MonitorBateria::iniciarConsumo()
{
    somaConsumo_ = 0;
    amostrasConsumo_ = 0;
}

float MonitorBateria::consumoMedio() const
{
    if (!amostrasConsumo_)
        return 0;
    float m = (float)(somaConsumo_ / amostrasConsumo_);
    return m < 0 ? 0 : m;
}

uint8_t MonitorBateria::percentual() const
{
    float p = (tensao() - MINIMO_V) / (CHEIA_V - MINIMO_V) * 100.0f;
    if (!presente_ || p < 0)
        return 0;
    return p > 100 ? 100 : (uint8_t)(p + 0.5f);
}

// Cálculo do 4.2: capacidade utilizável (80 %) proporcional à carga, dividida
// pela corrente média. A curva da LiPo não é linear: é uma estimativa (#277).
float MonitorBateria::autonomiaMinutos() const
{
    float i = corrente();
    if (!presente_ || i <= 1.0f)
        return -1;
    float restante_mah = CAPACIDADE_MAH * PROFUNDIDADE_DESCARGA * percentual() / 100.0f;
    return restante_mah / i * 60.0f;
}
