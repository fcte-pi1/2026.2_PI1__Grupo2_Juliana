#include "robo_fisico.h"

#include <math.h>

bool RoboFisico::calibrar()
{
    if (!hw_.sensoresOk())
        return false;
    controle_.reiniciar(hw_.ler(), 0);
    ultimo_ = MOV_OCIOSO;
    recuperacoes_ = 0;
    leiturasTortas_ = 0;
    aprendeuFrente_ = false;
    desvioX_ = desvioY_ = 0;
    return true;
}

void RoboFisico::esperar(uint16_t ms)
{
    uint32_t t0 = hw_.ler().agora_ms;
    for (;;)
    {
        EntradaMovimento e = hw_.ler();
        controle_.atualizar(e); // mantém a odometria em dia
        if (e.agora_ms - t0 >= ms || hw_.abortado())
            return;
    }
}

// Desvio do centro da célula na direção d (mm, + = além do centro).
float RoboFisico::desvioAo(Direcao d) const
{
    return desvioX_ * deslocamentoX(d) + desvioY_ * deslocamentoY(d);
}

// Grava o desvio a partir das componentes ao longo de d e à direita de d.
void RoboFisico::definirDesvio(Direcao d, float longitudinal, float lateral)
{
    Direcao r = girarDireita(d);
    desvioX_ = deslocamentoX(d) * longitudinal + deslocamentoX(r) * lateral;
    desvioY_ = deslocamentoY(d) * longitudinal + deslocamentoY(r) * lateral;
}

// Marca as paredes da célula vistas pela frente, esquerda e direita do robô e,
// com ele parado, corrige o desvio do centro pelas distâncias até elas.
// A frente é a direção física (pelo rumo), não a do mapa: se as duas
// divergissem, as paredes iriam para o lado errado.
void RoboFisico::sentir(Labirinto &mapa)
{
    esperar(ESPERA_SENSORES_MS);
    EntradaMovimento e = hw_.ler();
    Direcao frente = direcao();
    // Giro que não terminou (encostou no meio): os sensores veem as paredes na
    // diagonal. Melhor não marcar nada do que marcar parede onde não tem.
    if (fabsf(normalizarGraus(controle_.rumo() - anguloDe(frente, inicial_))) > ALINHADO_GRAUS)
    {
        leiturasTortas_++;
        return;
    }
    // Muito fora do centro (giro que travou, manobra), os sensores veem as
    // paredes de outro jeito: parede falsa no mapa e "sem caminho" no fim
    // (labirinto, 24/11/2026). Também não marca.
    if (fabsf(desvioX_) > FORA_DO_CENTRO_MM || fabsf(desvioY_) > FORA_DO_CENTRO_MM)
    {
        leiturasTortas_++;
        return;
    }
    const Direcao lados[3] = {girarEsquerda(frente), frente, girarDireita(frente)};
    for (uint8_t i = 0; i < 3; i++)
    {
        if (e.parede[i])
            mapa.marcarParede(mapa.x(), mapa.y(), lados[i]);
        else
            mapa.marcarAberta(mapa.x(), mapa.y(), lados[i]);
    }
    // Sem referência da frente: parado no centro (pelos encoders, sem desvio
    // grande) e com parede na frente, a distância de agora vira a referência.
    ParametrosMovimento &p = controle_.parametros();
    if (p.frontalCentroMm == 0 && e.parede[1] && e.distancia_mm[1] > 0 && e.distancia_mm[1] < 150 &&
        fabsf(desvioAo(frente)) < 10)
    {
        p.frontalCentroMm = e.distancia_mm[1];
        aprendeuFrente_ = true;
    }
    // Medidas fora do plausível (sensor no canto de uma parede) ficam de fora.
    float longitudinal = desvioAo(frente), lateral = desvioAo(girarDireita(frente)), medida = 0;
    if (medirLongitudinal(e, p, medida) && fabsf(medida) <= p.lateralMaximoMm)
        longitudinal = medida;
    if (medirLateral(e, p, medida) && fabsf(medida) <= p.lateralMaximoMm)
        lateral = medida;
    definirDesvio(frente, longitudinal, lateral);
}

bool RoboFisico::executar()
{
    while (true)
    {
        EntradaMovimento e = hw_.ler();
        if (hw_.abortado())
        {
            controle_.parar();
            hw_.aplicar({0, 0, true});
            ultimo_ = MOV_OCIOSO;
            return false;
        }
        SaidaMovimento s = controle_.atualizar(e);
        hw_.aplicar(s);
        hw_.observar(controle_);
        if (controle_.terminou())
        {
            ultimo_ = controle_.estado();
            return ultimo_ == MOV_CONCLUIDO;
        }
    }
}

void RoboFisico::somarDesvio(Direcao d, float mm)
{
    desvioX_ += deslocamentoX(d) * mm;
    desvioY_ += deslocamentoY(d) * mm;
}

// Gira até o rumo. Se bloquear (encostou na parede ou as rodas travaram),
// manobra e tenta de novo, até MAX_TENTATIVAS_GIRO: alterna ré e frente
// curtas (frente só com espaço no sensor da frente), como uma baliza, e, com
// 90° ou mais faltando, começa a tentativa seguinte girando para o outro lado
// (numa meia-volta dá no mesmo). Antes tentava uma vez só e desistia: no
// labirinto (24/11/2026) o robô ficava fora do lugar e lia parede falsa.
bool RoboFisico::girarPara(float rumoGraus)
{
    controle_.girar(rumoGraus);
    if (executar())
        return true;
    for (uint8_t tentativa = 1; tentativa < MAX_TENTATIVAS_GIRO; tentativa++)
    {
        if (ultimo_ != MOV_BLOQUEADO || hw_.abortado())
            return false;
        recuperacoes_++;
        Direcao d = direcao();
        EntradaMovimento e = hw_.ler();
        bool frenteLivre = e.distancia_mm[1] == 0 || e.distancia_mm[1] > FRENTE_LIVRE_MANOBRA_MM;
        float manobra = (tentativa % 2 == 0 && frenteLivre) ? RECUO_MM : -RECUO_MM;
        controle_.andar(manobra, controle_.rumo());
        executar();
        somarDesvio(d, controle_.percorridoMm());
        // Não saiu do lugar: o canto preso está desse lado. Tenta o outro na
        // hora (labirinto novo, 24/11/2026: a ré não andava nada e só a manobra
        // para a frente, na tentativa seguinte, desentalava o giro).
        if (fabsf(controle_.percorridoMm()) < MANOBRA_MINIMA_MM && !hw_.abortado() &&
            (manobra < 0 ? frenteLivre : true))
        {
            controle_.andar(-manobra, controle_.rumo());
            executar();
            somarDesvio(d, controle_.percorridoMm());
        }
        if (hw_.abortado())
            return false;
        float falta = normalizarGraus(rumoGraus - controle_.rumo());
        if (tentativa % 2 == 1 && fabsf(falta) >= 90)
        {
            // Um quarto de volta pelo outro lado; dali o giro curto até o rumo
            // continua nesse sentido.
            controle_.girar(normalizarGraus(controle_.rumo() - (falta > 0 ? 90 : -90)));
            if (!executar() && hw_.abortado())
                return false;
        }
        controle_.girar(rumoGraus);
        if (executar())
            return true;
    }
    return false;
}

// Anda e, se bloquear no caminho (toque na parede, parede que o mapa não
// tinha), volta de ré o que andou: o robô fica no centro da célula de onde
// saiu, que é onde o mapa o registra (RF16-S).
bool RoboFisico::andarMm(float mm, float rumoGraus, float lateralInicialMm, bool corrigirPeloFundo)
{
    controle_.andar(mm, rumoGraus, lateralInicialMm, corrigirPeloFundo);
    bool ok = executar();
    percorridoTotal_ = controle_.percorridoMm();
    if (ok)
        return true;
    EstadoMovimento falha = ultimo_;
    float andou = percorridoTotal_;
    if (hw_.abortado() || fabsf(andou) < 10)
        return false;
    recuperacoes_++;
    controle_.andar(-andou, rumoGraus, controle_.lateralMm());
    executar();
    percorridoTotal_ += controle_.percorridoMm();
    ultimo_ = falha;
    return false;
}

// Vira para d e anda uma célula, compensando o quanto o robô já estava fora
// do centro: sem isso, um erro de milímetros por célula vira centímetros num
// corredor longo e o robô encosta na curva.
bool RoboFisico::mover(Direcao d)
{
    if (hw_.abortado())
        return false;
    float rumo = anguloDe(d, inicial_);
    // Gira sempre que o erro passar da tolerância: o alvo é absoluto, então
    // também corrige o desvio que sobrou do movimento anterior.
    float erroRumo = fabsf(normalizarGraus(rumo - controle_.rumo()));
    bool gira = erroRumo > controle_.parametros().toleranciaGiroGraus;
    // Ajuste de poucos graus (sem BNO085 acontece quase toda célula) não muda
    // o que a frente vê; só um giro de verdade troca a parede da frente.
    bool trocaFrente = erroRumo > TROCA_FRENTE_GRAUS;
    // Antes de girar, volta ao centro da célula: o robô tem ~2 cm de folga de
    // cada lado e, parado além do centro, o canto pega a parede no giro
    // (labirinto, 23/11/2026: parou 15 mm além e travou no meio da meia-volta).
    Direcao atual = direcao();
    float alem = desvioAo(atual);
    if (gira && fabsf(alem) > CENTRALIZAR_ANTES_DO_GIRO_MM)
    {
        andarMm(-alem, anguloDe(atual, inicial_));
        somarDesvio(atual, percorridoTotal_);
    }
    if (gira && !girarPara(rumo))
        return false;
    float longitudinal = desvioAo(d);
    // Sem trocar de direção, o robô está parado desde o sentir(): a parede do
    // fundo pode corrigir a distância. Depois de um giro de verdade a mediana
    // do VL53L0X ainda mostra a direção antiga. (Labirinto, 24/11/2026: um
    // ajuste de 2,5° em (3,1) desligava a correção e o robô passava do ponto.)
    bool ok = andarMm(CELULA_MM - longitudinal, rumo, desvioAo(girarDireita(d)), !trocaFrente);
    // Parou pela parede da frente: está no centro, dentro da tolerância.
    float novoLongitudinal = ok && controle_.paradaPelaFrente()
                                 ? 0
                                 : longitudinal + percorridoTotal_ - (ok ? CELULA_MM : 0);
    definirDesvio(d, novoLongitudinal, controle_.lateralMm());
    // Bloqueou andando (raspou numa coluna): na próxima tentativa o desvio
    // lateral conhecido conta EMPURRAO_LATERAL_MM a mais, e a correção de
    // trajetória vira mais para o lado oposto (labirinto novo, 24/11/2026: em
    // (1,1), 16 mm à esquerda, o robô batia na coluna da esquerda ao subir).
    float lateral = desvioAo(girarDireita(d));
    if (!ok && ultimo_ == MOV_BLOQUEADO && fabsf(lateral) > 3)
        somarDesvio(girarDireita(d), lateral > 0 ? EMPURRAO_LATERAL_MM : -EMPURRAO_LATERAL_MM);
    return ok;
}
