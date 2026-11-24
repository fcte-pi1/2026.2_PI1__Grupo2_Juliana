#include "controle_movimento.h"

#include <math.h>

namespace
{
const float RAD_GRAUS = 57.2957795f;
const uint8_t ESQ = 0, FRENTE = 1, DIR = 2; // ordem de LadoSensor
// A parede da frente só assume a parada quando a distância dela bate com o
// fim do movimento (folga para o erro dos encoders), não a duas células.
const float JANELA_FRENTE_MM = 60;

float limitar(float v, float lim) { return v > lim ? lim : (v < -lim ? -lim : v); }
float sinal(float v) { return v < 0 ? -1.0f : 1.0f; }

// Rumo absoluto das direções do mapa: Norte 0, Oeste 90, Sul 180, Leste -90.
float anguloAbsoluto(Direcao d)
{
    switch (d)
    {
    case NORTE: return 0;
    case OESTE: return 90;
    case SUL: return 180;
    default: return -90;
    }
}

bool leituraValida(uint16_t mm) { return mm > 0 && mm < DIST_MAX_CONFIAVEL; }
} // namespace

const char *nomeMovimento(EstadoMovimento e)
{
    static const char *const NOMES[] = {"ocioso", "andando", "girando", "concluido", "bloqueado", "tempo esgotado"};
    return e <= MOV_TEMPO_ESGOTADO ? NOMES[e] : "?";
}

float normalizarGraus(float graus)
{
    while (graus > 180)
        graus -= 360;
    while (graus <= -180)
        graus += 360;
    return graus;
}

float anguloDe(Direcao d, Direcao orientacaoInicial)
{
    return normalizarGraus(anguloAbsoluto(d) - anguloAbsoluto(orientacaoInicial));
}

Direcao direcaoDe(float graus, Direcao orientacaoInicial)
{
    const Direcao todas[] = {NORTE, LESTE, SUL, OESTE};
    Direcao melhor = orientacaoInicial;
    float menor = 1000;
    for (Direcao d : todas)
    {
        float erro = fabsf(normalizarGraus(graus - anguloDe(d, orientacaoInicial)));
        if (erro < menor)
        {
            menor = erro;
            melhor = d;
        }
    }
    return melhor;
}

uint8_t medirLateral(const EntradaMovimento &e, const ParametrosMovimento &p, float &lateral)
{
    bool pe = e.parede[ESQ] && leituraValida(e.distancia_mm[ESQ]);
    bool pd = e.parede[DIR] && leituraValida(e.distancia_mm[DIR]);
    float desvioE = (float)e.distancia_mm[ESQ] - p.esquerdaCentroMm;
    float desvioD = (float)e.distancia_mm[DIR] - p.direitaCentroMm;
    if (pe && pd)
    {
        lateral = (desvioE - desvioD) / 2;
        return 1;
    }
    if (pe && p.esquerdaCentroMm > 0)
    {
        lateral = desvioE;
        return 2;
    }
    if (pd && p.direitaCentroMm > 0)
    {
        lateral = -desvioD;
        return 3;
    }
    return 0;
}

bool medirLongitudinal(const EntradaMovimento &e, const ParametrosMovimento &p, float &longitudinal)
{
    if (!e.parede[FRENTE] || p.frontalCentroMm == 0 || !leituraValida(e.distancia_mm[FRENTE]))
        return false;
    longitudinal = (float)p.frontalCentroMm - e.distancia_mm[FRENTE];
    return true;
}

void ControleMovimento::reiniciar(const EntradaMovimento &e, float rumoGraus)
{
    rumo_ = normalizarGraus(rumoGraus);
    reforcoE_ = reforcoD_ = 0;
    yawAnteriorOk_ = e.yawOk;
    yawAnterior_ = e.yawGraus;
    ultimoE_ = e.encoderEsquerdo;
    ultimoD_ = e.encoderDireito;
    temLeitura_ = true;
    estado_ = MOV_OCIOSO;
}

void ControleMovimento::andar(float mm, float rumoGraus, float lateralInicialMm, bool corrigirPeloFundo)
{
    alvoMm_ = mm;
    corrigirPeloFundo_ = corrigirPeloFundo;
    lateral_ = lateralInicialMm;
    rumoAlvo_ = normalizarGraus(rumoGraus);
    estado_ = MOV_ANDANDO;
    comecou_ = false;
}

void ControleMovimento::girar(float rumoGraus)
{
    rumoAlvo_ = normalizarGraus(rumoGraus);
    estado_ = MOV_GIRANDO;
    comecou_ = false;
}

// Rumo pela variação do yaw do BNO085 quando ele responde; sem ele, pela
// diferença dos encoders (o BNO085 ainda falha na bateria, docs/7.3). Soma só
// variações, então uma falha ou um reinício do BNO085 no meio da corrida não
// perde o rumo.
void ControleMovimento::odometria(const EntradaMovimento &e)
{
    if (!temLeitura_)
    {
        ultimoE_ = e.encoderEsquerdo;
        ultimoD_ = e.encoderDireito;
        temLeitura_ = true;
    }
    float dE = (e.encoderEsquerdo - ultimoE_) / p_.pulsosPorMm;
    float dD = (e.encoderDireito - ultimoD_) / p_.pulsosPorMm;
    if (e.encoderEsquerdo != ultimoE_ || e.encoderDireito != ultimoD_)
        ultimoPulso_ = e.agora_ms;
    if (e.encoderEsquerdo != ultimoE_)
        ultimoPulsoE_ = e.agora_ms;
    if (e.encoderDireito != ultimoD_)
        ultimoPulsoD_ = e.agora_ms;
    ultimoE_ = e.encoderEsquerdo;
    ultimoD_ = e.encoderDireito;
    avancoUltimo_ = (dE + dD) / 2;
    float variacaoYaw = normalizarGraus(e.yawGraus - yawAnterior_);
    if (e.yawOk && yawAnteriorOk_ && fabsf(variacaoYaw) < SALTO_YAW_GRAUS)
        rumo_ = normalizarGraus(rumo_ + variacaoYaw);
    else
    {
        // Largura efetiva por sentido (calibração Z): girando à direita a roda
        // direita vai para trás e patina diferente da esquerda.
        float largura = dD - dE < 0 ? p_.larguraEixoDireitaMm : p_.larguraEixoEsquerdaMm;
        rumo_ = normalizarGraus(rumo_ + (dD - dE) / (largura > 0 ? largura : p_.larguraEixoMm) * RAD_GRAUS);
    }
    yawAnteriorOk_ = e.yawOk;
    yawAnterior_ = e.yawGraus;
}

void ControleMovimento::comecar(const EntradaMovimento &e)
{
    comecou_ = true;
    ultimoPulsoE_ = ultimoPulsoD_ = ultimoCiclo_ = e.agora_ms;
    ambasParadasDesde_ = 0;
    inicioE_ = e.encoderEsquerdo;
    inicioD_ = e.encoderDireito;
    inicio_ = e.agora_ms;
    ultimoPulso_ = e.agora_ms;
    dentro_ = false;
    percorrido_ = 0;
    erroLateral_ = 0;
    temLateral_ = false;
    correcao_ = 0;
    usouFrente_ = false;
    fonteRef_ = 0;
    // Parado no começo do comando, a leitura das paredes já está estável.
    float medida = 0;
    fonteAtual_ = medirLateral(e, p_, medida);
    medidaAnterior_ = medida;
    trocaFonte_ = -p_.estabilizarParedeMm;
    if (estado_ == MOV_GIRANDO)
        lateral_ = 0;
    // Andando para a frente, parede n células depois do destino (até
    // alcanceFundoMm): o centro do destino fica a (frente - centro - n
    // células). Num corredor sem parede logo à frente só os encoders contavam
    // e o erro de uma manobra ficava (labirinto, 24/11/2026: parou ~9 cm além e
    // a borda do fim virou parede da própria célula, "sem caminho"). Medido
    // uma vez, parado (quem chama diz se está: corrigirPeloFundo): andando, a
    // mediana do VL53L0X atrasa ~10 mm. Só corrige
    // alinhado (torto, o cone pega a parede do lado) e se discordar dos
    // encoders em menos de janelaFundoMm (abaixo de meia célula, sem dúvida de n).
    usouFundo_ = false;
    if (corrigirPeloFundo_ && estado_ == MOV_ANDANDO && alvoMm_ > 0 && p_.frontalCentroMm > 0 && leituraValida(e.distancia_mm[FRENTE]) &&
        e.distancia_mm[FRENTE] <= p_.alcanceFundoMm && fabsf(normalizarGraus(rumoAlvo_ - rumo_)) < p_.alinhadoFundoGraus)
    {
        float pelaFrente = (float)e.distancia_mm[FRENTE] - p_.frontalCentroMm;
        float n = roundf((pelaFrente - alvoMm_) / CELULA_MM);
        float pelaParede = pelaFrente - n * CELULA_MM;
        if (n >= 1 && fabsf(pelaParede - alvoMm_) < p_.janelaFundoMm)
        {
            alvoMm_ = pelaParede;
            usouFundo_ = true;
        }
    }
}

SaidaMovimento ControleMovimento::atualizar(const EntradaMovimento &e)
{
    odometria(e);
    if (!ocupado())
        return {0, 0, false};
    if (!comecou_)
        comecar(e);
    SaidaMovimento s = estado_ == MOV_ANDANDO ? andando(e) : girando(e);
    if (!s.frear)
        reforcarRodasParadas(e, s);
    ultimoCiclo_ = e.agora_ms;
    return s;
}

// Roda com potência e sem pulsos há rodaParadaMs: a outra roda espera parada
// (senão o robô pivota nela) e a potência mínima da parada sobe aos poucos até
// ela andar, no máximo potenciaArranqueMaxima. Esse mínimo fica aprendido para
// os próximos movimentos (é o atrito daquele motor) e zera em reiniciar().
// Quando uma roda pediria menos que o seu mínimo, as duas sobem o mesmo tanto:
// a diferença entre elas, que é o que faz o robô virar, não muda.
void ControleMovimento::reforcarRodasParadas(const EntradaMovimento &e, SaidaMovimento &s)
{
    float passo = p_.reforcoPorSegundo * (e.agora_ms - ultimoCiclo_) / 1000.0f;
    const bool girando = estado_ == MOV_GIRANDO;
    // Arrancar do zero (girando ou andando) pode passar do teto do PWM até trancoGiro: no
    // chão, com PWM 80, o robô às vezes não saía do lugar andando reto (23/11/2026).
    const float limite = p_.trancoGiro > p_.potenciaArranqueMaxima ? p_.trancoGiro : p_.potenciaArranqueMaxima;
    bool paradaE = s.esquerdo != 0 && e.agora_ms - ultimoPulsoE_ > p_.rodaParadaMs;
    bool paradaD = s.direito != 0 && e.agora_ms - ultimoPulsoD_ > p_.rodaParadaMs;
    // As duas rodas travadas juntas (robô engatado numa emenda do piso ou
    // encostado): depois de 300 ms, um tranco curto forte nas duas, acima do
    // reforço. Labirinto novo, 24/11/2026: em (2,0), reto e centrado, as duas
    // a 0,6 não davam um pulso e o robô desistia.
    if (paradaE && paradaD)
    {
        if (ambasParadasDesde_ == 0)
            ambasParadasDesde_ = e.agora_ms;
        uint32_t t = e.agora_ms - ambasParadasDesde_;
        if (t >= p_.trancoForteAposMs && t < (uint32_t)p_.trancoForteAposMs + p_.trancoForteMs)
        {
            s.esquerdo = sinal(s.esquerdo) * p_.trancoForte;
            s.direito = sinal(s.direito) * p_.trancoForte;
            return;
        }
    }
    else
        ambasParadasDesde_ = 0;
    if (paradaE)
        reforcoE_ = fminf((reforcoE_ > fabsf(s.esquerdo) ? reforcoE_ : fabsf(s.esquerdo)) + passo, limite);
    if (paradaD)
        reforcoD_ = fminf((reforcoD_ > fabsf(s.direito) ? reforcoD_ : fabsf(s.direito)) + passo, limite);
    // Acima do teto o reforço é só o tranco para sair do lugar: com a roda já
    // girando, ele desce de volta ao teto (o robô não fica rápido depois de arrancar).
    if (!paradaE && reforcoE_ > p_.potenciaArranqueMaxima)
        reforcoE_ = fmaxf(reforcoE_ - 2 * passo, p_.potenciaArranqueMaxima);
    if (!paradaD && reforcoD_ > p_.potenciaArranqueMaxima)
        reforcoD_ = fmaxf(reforcoD_ - 2 * passo, p_.potenciaArranqueMaxima);

    // Uma parada e a outra andando: a que anda espera. Contar a espera como
    // pulso evita que ela também pareça travada; o bloqueio continua vindo do
    // tempo sem pulso nenhum (vigiar). Espera só até esperaMaximaMs: no chão a
    // roda esquerda (mais dura) sozinha não arrasta o robô, nem com 0,6
    // (23 e 24/11/2026: "bloqueado" e recuo); passado isso, as duas vão juntas.
    if (!(paradaE ^ paradaD))
        esperaDesde_ = 0;
    else if (esperaDesde_ == 0)
        esperaDesde_ = e.agora_ms;
    const bool esperar = !(esperaDesde_ && e.agora_ms - esperaDesde_ > p_.esperaMaximaMs);

    // Girando, o reforço vai só para a roda travada e a outra espera parada:
    // somado nas duas, empurrava a que anda e o robô pivotava na travada até a
    // parede (labirinto, 24/11/2026: a esquerda parou freando e a direita seguiu
    // ~14 mm sozinha). As duas travadas, ou a espera esgotada: cada uma com o seu reforço.
    if (girando)
    {
        if (paradaE || !esperar)
            s.esquerdo = sinal(s.esquerdo) * fmaxf(fabsf(s.esquerdo), reforcoE_);
        if (paradaD || !esperar)
            s.direito = sinal(s.direito) * fmaxf(fabsf(s.direito), reforcoD_);
        if (!esperar)
            return;
        if (paradaE && !paradaD && s.direito != 0)
        {
            s.direito = 0;
            ultimoPulsoD_ = e.agora_ms; // esperando de propósito, não travada
        }
        else if (paradaD && !paradaE && s.esquerdo != 0)
        {
            s.esquerdo = 0;
            ultimoPulsoE_ = e.agora_ms;
        }
        return;
    }

    float falta = 0;
    if (s.esquerdo != 0 && reforcoE_ - fabsf(s.esquerdo) > falta)
        falta = reforcoE_ - fabsf(s.esquerdo);
    if (s.direito != 0 && reforcoD_ - fabsf(s.direito) > falta)
        falta = reforcoD_ - fabsf(s.direito);
    if (falta > 0)
    {
        float e2 = s.esquerdo != 0 ? fabsf(s.esquerdo) + falta : 0;
        float d2 = s.direito != 0 ? fabsf(s.direito) + falta : 0;
        float excesso = fmaxf(e2, d2) - limite; // acima do teto: desce as duas juntas
        if (excesso > 0)
        {
            e2 = e2 > 0 ? fmaxf(e2 - excesso, 0.01f) : 0;
            d2 = d2 > 0 ? fmaxf(d2 - excesso, 0.01f) : 0;
        }
        s.esquerdo = sinal(s.esquerdo) * e2 * (s.esquerdo != 0);
        s.direito = sinal(s.direito) * d2 * (s.direito != 0);
    }
    // A roda travada recebe pelo menos o próprio reforço, mesmo que a diferença
    // da correção de rumo encolha: com a outra no teto, subir as duas juntas a
    // deixava presa abaixo do que precisa para sair (labirinto novo, 24/11/2026,
    // (1,2): esquerda em 0,40 e direita em 0,60 por 10 s, sem um pulso na esquerda).
    if (paradaE)
        s.esquerdo = sinal(s.esquerdo) * fmaxf(fabsf(s.esquerdo), reforcoE_);
    if (paradaD)
        s.direito = sinal(s.direito) * fmaxf(fabsf(s.direito), reforcoD_);
    if (!esperar)
        return;
    if (paradaE && !paradaD && s.direito != 0)
    {
        s.direito = 0;
        ultimoPulsoD_ = e.agora_ms;
    }
    else if (paradaD && !paradaE && s.esquerdo != 0)
    {
        s.esquerdo = 0;
        ultimoPulsoE_ = e.agora_ms;
    }
}

bool ControleMovimento::frenteMuitoPerto(const EntradaMovimento &e) const
{
    return e.parede[FRENTE] && leituraValida(e.distancia_mm[FRENTE]) &&
           e.distancia_mm[FRENTE] < p_.distanciaMinimaFrenteMm;
}

SaidaMovimento ControleMovimento::terminar(EstadoMovimento fim)
{
    estado_ = fim;
    correcao_ = 0;
    return {0, 0, true};
}

bool ControleMovimento::vigiar(const EntradaMovimento &e)
{
    if (e.agora_ms - inicio_ > p_.tempoMaximoMs)
    {
        terminar(MOV_TEMPO_ESGOTADO);
        return false;
    }
    if (e.agora_ms - ultimoPulso_ > p_.tempoParadoMs)
    {
        terminar(MOV_BLOQUEADO);
        return false;
    }
    return true;
}

// Sem o BNO085, o rumo pelos encoders acumula o erro de cada giro (largura do
// eixo, patinada). Andando ao lado de uma parede, o erro lateral muda na
// proporção do ângulo entre o robô e o corredor: tan(ângulo) = Δlateral /
// Δavanço. Esse ângulo puxa o rumo estimado para o certo. A referência
// recomeça se a parede some ou se a medida passa a vir de outra parede (as
// referências de cada lado podem não bater exatamente).
void ControleMovimento::corrigirRumoPelaParede(const EntradaMovimento &e, uint8_t fonte)
{
    if (e.yawOk || p_.pesoRumoParede <= 0 || fonte != fonteRef_)
    {
        fonteRef_ = fonte;
        lateralRef_ = erroLateral_;
        avancoRef_ = percorrido_;
        return;
    }
    if (fonte == 0)
        return;
    float avanco = percorrido_ - avancoRef_;
    if (fabsf(avanco) < p_.baseRumoParedeMm)
        return;
    // Lateral crescendo andando para frente = robô apontando para a direita do eixo.
    float angulo = -atanf((erroLateral_ - lateralRef_) / avanco) * RAD_GRAUS;
    float medido = normalizarGraus(rumoAlvo_ + angulo);
    rumo_ = normalizarGraus(rumo_ + p_.pesoRumoParede * normalizarGraus(medido - rumo_));
    lateralRef_ = erroLateral_;
    avancoRef_ = percorrido_;
}

SaidaMovimento ControleMovimento::andando(const EntradaMovimento &e)
{
    const float sentido = sinal(alvoMm_);
    percorrido_ = ((e.encoderEsquerdo - inicioE_) + (e.encoderDireito - inicioD_)) / 2.0f / p_.pulsosPorMm;
    float restante = fabsf(alvoMm_) - sentido * percorrido_;

    // Parede na frente: a distância medida corrige o erro acumulado dos
    // encoders e garante o robô parado no centro da célula (CT-SE05).
    usouFrente_ = false;
    if (sentido > 0 && p_.frontalCentroMm > 0 && leituraValida(e.distancia_mm[FRENTE]))
    {
        float pelaFrente = (float)e.distancia_mm[FRENTE] - p_.frontalCentroMm;
        if (pelaFrente < restante + JANELA_FRENTE_MM)
        {
            restante = pelaFrente;
            usouFrente_ = true;
        }
    }

    // Parede perto demais na frente: para já, calibrado ou não. Conta como
    // célula andada só se já passou da metade.
    if (sentido > 0 && frenteMuitoPerto(e))
    {
        usouFrente_ = true;
        restante = 0;
    }

    if (restante <= p_.toleranciaDistanciaMm)
    {
        // Parou pela parede antes de andar meia célula: tem parede onde o mapa
        // achava que não havia. Não conta como célula andada.
        if (usouFrente_ && percorrido_ < fabsf(alvoMm_) / 2)
            return terminar(MOV_BLOQUEADO);
        return terminar(MOV_CONCLUIDO);
    }
    if (!vigiar(e))
        return {0, 0, true};

    // Erro lateral (+ = robô à direita do centro) pelas paredes dos lados.
    float medida = 0;
    uint8_t fonte = medirLateral(e, p_, medida);
    // Parede de um lado só: anda afastadoUmaParedeMm mais longe dela que o
    // centro. O canto traseiro do robô passa a ~9-10 cm do eixo no meio de um
    // giro e a parede fica a ~8,4 cm do centro: chegando colado nela, o giro
    // seguinte travava (labirinto novo, 24/11/2026, (1,1) só com a parede Sul).
    if (fonte == 2)
        medida -= p_.afastadoUmaParedeMm; // parede à esquerda: alvo à direita
    else if (fonte == 3)
        medida += p_.afastadoUmaParedeMm; // parede à direita: alvo à esquerda
    if (fonte != fonteAtual_ || fabsf(medida - medidaAnterior_) > p_.saltoLateralMm)
    {
        fonteAtual_ = fonte;
        trocaFonte_ = percorrido_;
        fonteRef_ = 0; // o rumo pela parede também recomeça
    }
    medidaAnterior_ = medida;
    temLateral_ = fonte != 0 && fabsf(percorrido_ - trocaFonte_) >= p_.estabilizarParedeMm &&
                  fabsf(medida) <= p_.lateralMaximoMm;
    if (temLateral_)
        erroLateral_ = medida;
    corrigirRumoPelaParede(e, temLateral_ ? fonte : 0);

    // Odometria do desvio lateral: apontando para a esquerda do eixo, o robô
    // vai para a esquerda. Com parede, a medida dela vale mais.
    lateral_ -= avancoUltimo_ * sinf(normalizarGraus(rumo_ - rumoAlvo_) / RAD_GRAUS);
    if (temLateral_)
        lateral_ = erroLateral_;
    else
        erroLateral_ = lateral_;

    // Robô à direita do centro: aponta um pouco para a esquerda (de ré, o contrário).
    float corrigeLateral = limitar(p_.ganhoLateral * erroLateral_, p_.correcaoLateralMaxima) * sentido;
    float erroRumo = normalizarGraus(rumoAlvo_ + corrigeLateral - rumo_);
    correcao_ = limitar(p_.ganhoRumo * erroRumo, p_.correcaoMaxima);

    float base = p_.ganhoDistancia * restante;
    base = base > p_.potenciaMaxima ? p_.potenciaMaxima : (base < p_.potenciaMinima ? p_.potenciaMinima : base);
    base *= sentido;
    // Correção positiva = girar à esquerda: roda direita mais rápida.
    return {base - correcao_, base + correcao_, false};
}

SaidaMovimento ControleMovimento::girando(const EntradaMovimento &e)
{
    float erro = normalizarGraus(rumoAlvo_ - rumo_);
    if (fabsf(erro) <= p_.toleranciaGiroGraus)
    {
        if (!dentro_)
        {
            dentro_ = true;
            dentroDesde_ = e.agora_ms;
        }
        if (e.agora_ms - dentroDesde_ >= p_.assentarGiroMs)
            return terminar(MOV_CONCLUIDO);
        ultimoPulso_ = e.agora_ms; // parado de propósito, não é bloqueio
        return {0, 0, true};
    }
    dentro_ = false;
    // Girando no lugar a frente não deveria se aproximar da parede: se chegar
    // perto demais, o robô está pivotando (uma roda não anda) e vai bater.
    // Mas no próprio eixo a frente passa perto da parede do lado (só geometria):
    // entre distanciaMinimaFrenteGiroMm e distanciaMinimaFrenteMm, bloqueia só
    // se o robô já saiu do lugar (avanço médio das rodas) mais que pivoMaximoMm.
    float saiu = fabsf((float)(e.encoderEsquerdo - inicioE_ + e.encoderDireito - inicioD_)) / 2 / p_.pulsosPorMm;
    if (frenteMuitoPerto(e) && (saiu > p_.pivoMaximoMm || e.distancia_mm[FRENTE] < p_.distanciaMinimaFrenteGiroMm))
        return terminar(MOV_BLOQUEADO);
    if (!vigiar(e))
        return {0, 0, true};
    float p = fabsf(erro) * p_.ganhoGiro;
    const float minima = erro < 0 ? p_.potenciaGiroMinimaDireita : p_.potenciaGiroMinima;
    p = p < minima ? minima : p;
    p = p > p_.potenciaGiroMaxima ? p_.potenciaGiroMaxima : p;
    correcao_ = sinal(erro) * p;
    // As duas rodas o mesmo tanto: a adiantada perde potência, a atrasada ganha.
    float aE = fabsf((float)(e.encoderEsquerdo - inicioE_)) / p_.pulsosPorMm;
    float aD = fabsf((float)(e.encoderDireito - inicioD_)) / p_.pulsosPorMm;
    float sinc = limitar(p_.ganhoSincronia * (aE - aD), p);
    // Erro positivo = girar à esquerda: esquerda para trás, direita para frente.
    return {-sinal(erro) * (p - sinc), sinal(erro) * (p + sinc), false};
}
