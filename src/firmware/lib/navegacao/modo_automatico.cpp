#include "modo_automatico.h"

const char *nomeEstado(EstadoRobo e)
{
    static const char *const NOMES[] = {"explorando", "retornando", "rapido",
                                        "parado", "calibrando", "falha"};
    return e <= ESTADO_FALHA ? NOMES[e] : "falha";
}

void ModoAutomatico::iniciar()
{
    estado_ = ESTADO_CALIBRANDO;
    motivo_ = SEM_FALHA;
    passos_ = 0;
    falhasMover_ = 0;
    viagens_ = 0;
    tamanhoRota_ = 0;
    indiceRota_ = 0;
    for (uint8_t x = 0; x < mapa_.largura(); x++)
        for (uint8_t y = 0; y < mapa_.altura(); y++)
            if (mapa_.ehObjetivo(x, y))
            {
                objetivoX_ = x;
                objetivoY_ = y;
            }
}

EstadoRobo ModoAutomatico::executar()
{
    while (estado_ != ESTADO_PARADO && estado_ != ESTADO_FALHA)
        passo();
    return estado_;
}

EstadoRobo ModoAutomatico::passo()
{
    switch (estado_)
    {
    case ESTADO_CALIBRANDO:
        if (!robo_.calibrar())
        {
            falhar(FALHA_CALIBRACAO);
            break;
        }
        mapa_.marcarVisitada(mapa_.x(), mapa_.y());
        robo_.sentir(mapa_);
        estado_ = ESTADO_EXPLORANDO;
        if (mapa_.noObjetivo())
            chegouAoObjetivo();
        break;

    case ESTADO_EXPLORANDO:
        navegarAte(objetivoX_, objetivoY_);
        if (estado_ == ESTADO_EXPLORANDO && mapa_.noObjetivo())
            chegouAoObjetivo();
        break;

    case ESTADO_RETORNANDO:
        navegarAte(0, 0);
        if (estado_ == ESTADO_RETORNANDO && mapa_.x() == 0 && mapa_.y() == 0)
            chegouNaLargada();
        break;

    case ESTADO_RAPIDO:
        if (indiceRota_ >= tamanhoRota_)
        {
            // A rota acabou fora do objetivo: o mapa estava errado.
            falhar(FALHA_SEM_CAMINHO);
            break;
        }
        if (andar(rota_[indiceRota_]))
            indiceRota_++;
        if (estado_ == ESTADO_RAPIDO && mapa_.noObjetivo())
            estado_ = ESTADO_PARADO;
        break;

    case ESTADO_PARADO:
    case ESTADO_FALHA:
        break;
    }
    return estado_;
}

void ModoAutomatico::falhar(MotivoFalha m)
{
    motivo_ = m;
    estado_ = ESTADO_FALHA;
}

// Uma célula em direção a (x, y), explorando: lado desconhecido conta como
// aberto e o Flood Fill é recalculado a cada célula (RF11-S).
void ModoAutomatico::navegarAte(uint8_t x, uint8_t y)
{
    mapa_.definirObjetivo(x, y);
    floodFill_.recalcular(mapa_, EXPLORACAO);
    Direcao d;
    bool temCaminho = floodFill_.proximaDirecao(mapa_, d);
    mapa_.definirObjetivo(objetivoX_, objetivoY_);
    if (!temCaminho)
    {
        falhar(FALHA_SEM_CAMINHO);
        return;
    }
    andar(d);
}

// Segue o contrato do labirinto.h: consulta podeAvancar() antes do motor e
// registra com avancar() depois que o robô andou de fato.
bool ModoAutomatico::andar(Direcao d)
{
    if (passos_ >= limitePassos_)
    {
        falhar(FALHA_LIMITE);
        return false;
    }
    mapa_.girar(d);
    uint8_t x0 = mapa_.x();
    uint8_t y0 = mapa_.y();
    if (mapa_.podeAvancar() && robo_.mover(d))
    {
        mapa_.marcarAberta(x0, y0, d);
        mapa_.avancar();
        passos_++;
        falhasMover_ = 0;
        robo_.sentir(mapa_);
        return true;
    }
    // Não andou: relê os sensores (pode haver parede nova) e tenta de novo
    // no próximo passo, com o Flood Fill recalculado.
    robo_.sentir(mapa_);
    if (++falhasMover_ >= MAX_TENTATIVAS_MOVER)
        falhar(FALHA_MOVIMENTO);
    return false;
}

void ModoAutomatico::chegouAoObjetivo()
{
    viagens_++;
    estado_ = ESTADO_RETORNANDO;
}

// Na largada: se nenhum lado desconhecido puder encurtar a rota, corre pela
// rota mais curta; senão faz mais uma ida e volta explorando (CT-SE14).
void ModoAutomatico::chegouNaLargada()
{
    if (!floodFill_.rotaGarantida(mapa_))
    {
        estado_ = ESTADO_EXPLORANDO;
        return;
    }
    tamanhoRota_ = floodFill_.rotaMaisCurta(mapa_, rota_, MAX_ROTA);
    indiceRota_ = 0;
    if (tamanhoRota_ == 0)
        falhar(FALHA_SEM_CAMINHO);
    else
        estado_ = ESTADO_RAPIDO;
}
