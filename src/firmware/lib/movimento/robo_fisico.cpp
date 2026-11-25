#include "robo_fisico.h"

#include <math.h>

bool RoboFisico::calibrar()
{
    if (!hw_.sensoresOk())
        return false;
    controle_.reiniciar(hw_.ler(), 0);
    ultimo_ = MOV_OCIOSO;
    recuperacoes_ = 0;
    rumosCorrigidos_ = 0;
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
    ParametrosMovimento &p = controle_.parametros();
    // Parou longe do centro pela parede da frente (o freio passa ~20 mm por
    // causa do atraso da mediana): acerta a posição antes de ler os lados. Com
    // o robô 22 mm além, o sensor da esquerda ficou na altura da coluna do
    // canto e marcou uma parede que não existe (labirinto novo, 24/11/2026, (3,1)).
    float medidaFrente = 0;
    if (medirLongitudinal(e, p, medidaFrente) && fabsf(medidaFrente) > CENTRALIZAR_NO_SENTIR_MM &&
        fabsf(medidaFrente) <= p.lateralMaximoMm && !hw_.abortado())
    {
        andarMm(-medidaFrente, anguloDe(frente, inicial_));
        esperar(ESPERA_SENSORES_MS);
        e = hw_.ler();
    }
    const Direcao lados[3] = {girarEsquerda(frente), frente, girarDireita(frente)};
    const uint16_t ref[3] = {p.esquerdaCentroMm, p.frontalCentroMm, p.direitaCentroMm};
    for (uint8_t i = 0; i < 3; i++)
    {
        // Parede longe da referência do centro (entre a da célula e a da
        // seguinte): o robô está deslocado ou torto, a leitura não decide.
        // Fica desconhecido (na exploração conta como aberto); se for parede,
        // o andar descobre. Labirinto novo, 24/11/2026: em (1,3), depois de
        // um giro que travou, a frente leu 115 mm e virou parede falsa.
        if (e.parede[i] && ref[i] && e.distancia_mm[i] > ref[i] + PAREDE_AMBIGUA_MM)
        {
            leiturasTortas_++;
            continue;
        }
        // Lado já visto aberto (ou atravessado) não vira parede: a leitura de
        // "parede" agora é o robô torto ou vendo uma coluna (pedido da equipe,
        // 24/11/2026: o painel mostrava parede no caminho por onde ele passou).
        // O contrário vale: parede marcada errada vira passagem.
        if (e.parede[i] && mapa.conhecida(mapa.x(), mapa.y(), lados[i]) && !mapa.temParede(mapa.x(), mapa.y(), lados[i]))
            continue;
        if (e.parede[i])
            mapa.marcarParede(mapa.x(), mapa.y(), lados[i]);
        else
            mapa.marcarAberta(mapa.x(), mapa.y(), lados[i]);
    }
    // Sem referência da frente: parado no centro (pelos encoders, sem desvio
    // grande) e com parede na frente, a distância de agora vira a referência.
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
    // Sem parede na frente da célula: a da célula seguinte (n células adiante,
    // até ALCANCE_CONFIRMACAO_MM) também dá a posição ao longo do corredor.
    // Labirinto novo, 24/11/2026: em (1,1) a frente lia 215 em vez de 231, o
    // robô estava 16 mm além do centro, e virado para o Norte batia na coluna.
    else if (!e.parede[1] && p.frontalCentroMm && e.distancia_mm[1] > 0 && e.distancia_mm[1] <= ALCANCE_CONFIRMACAO_MM)
    {
        float n = roundf(((float)e.distancia_mm[1] - p.frontalCentroMm) / CELULA_MM);
        float alem = p.frontalCentroMm + n * CELULA_MM - e.distancia_mm[1];
        if (n >= 1 && fabsf(alem) <= p.janelaFundoMm)
            longitudinal = alem;
    }
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

// Andou mm no rumo de agora, que pode ser diagonal (manobra no meio de um
// giro). Somar na direção cardeal mais próxima errava centímetros depois de
// algumas manobras: o robô achava que estava no centro, a coluna ao lado virou
// parede (labirinto novo, 24/11/2026, (1,1)).
void RoboFisico::somarDesvioPeloRumo(float mm)
{
    float a = normalizarGraus(controle_.rumo() - anguloDe(LESTE, inicial_)) / 57.2957795f;
    desvioX_ += mm * cosf(a);
    desvioY_ += mm * sinf(a);
}

// Gira até o rumo. Se bloquear (encostou na parede ou as rodas travaram),
// manobra e tenta de novo, até MAX_TENTATIVAS_GIRO: anda curto para a frente
// (frente livre) ou de ré (frente perto), como uma baliza, e, com
// 90° ou mais faltando, começa a tentativa seguinte girando para o outro lado
// (numa meia-volta dá no mesmo). Antes tentava uma vez só e desistia: no
// labirinto (24/11/2026) o robô ficava fora do lugar e lia parede falsa.
bool RoboFisico::girarPara(float rumoGraus)
{
    // Giro de ~90° com parede do lado para onde a traseira varre (girando à
    // direita ela vai para a esquerda, e vice-versa): no centro o canto de trás
    // pega nela (labirinto novo, 24/11/2026: em (1,1), centrado, o giro de Oeste
    // para Norte parava sempre em 153° com as duas rodas a 0,6). Gira num ponto
    // afastarNaVarreduraMm mais longe da parede, no mesmo lugar ao longo do
    // corredor: recua, gira ANGULO_DESVIO_GRAUS, avança na diagonal, gira o resto.
    float falta = normalizarGraus(rumoGraus - controle_.rumo());
    if (controle_.parametros().afastarNaVarreduraMm > 0 && fabsf(falta) > 60 && fabsf(falta) < 120)
    {
        EntradaMovimento e = hw_.ler();
        ParametrosMovimento &p = controle_.parametros();
        uint8_t lado = falta < 0 ? 0 : 2;
        uint16_t ref = lado == 0 ? p.esquerdaCentroMm : p.direitaCentroMm;
        uint16_t dist = e.distancia_mm[lado];
        if (ref && dist > 0 && dist < ref + PAREDE_NA_VARREDURA_MM)
        {
            const float a = ANGULO_DESVIO_GRAUS / 57.2957795f;
            const float diagonal = p.afastarNaVarreduraMm / sinf(a);
            float rumo0 = controle_.rumo();
            controle_.andar(-diagonal * cosf(a), rumo0);
            executar();
            somarDesvioPeloRumo(controle_.percorridoMm());
            controle_.girar(normalizarGraus(rumo0 + (falta < 0 ? -ANGULO_DESVIO_GRAUS : ANGULO_DESVIO_GRAUS)));
            executar();
            if (!hw_.abortado())
            {
                controle_.andar(diagonal, controle_.rumo());
                executar();
                somarDesvioPeloRumo(controle_.percorridoMm());
            }
        }
    }
    controle_.girar(rumoGraus);
    if (executar() || quaseNoRumo(rumoGraus))
        return true;
    const float faltaInicial = fabsf(normalizarGraus(rumoGraus - controle_.rumo()));
    for (uint8_t tentativa = 1; tentativa < MAX_TENTATIVAS_GIRO; tentativa++)
    {
        // Insiste enquanto o giro avança; sem avanço nenhum em 3 tentativas
        // (roda morta, robô preso), as manobras só pivotam e ele bate.
        if (tentativa > 3 && faltaInicial - fabsf(normalizarGraus(rumoGraus - controle_.rumo())) < PROGRESSO_MINIMO_GIRO_GRAUS)
            return false;
        // Tempo esgotado também: o giro se arrastando no fim, faltando 2,7°, era
        // desistência com o robô praticamente alinhado (bancada J, 24/11/2026).
        if ((ultimo_ != MOV_BLOQUEADO && ultimo_ != MOV_TEMPO_ESGOTADO) || hw_.abortado())
            return false;
        recuperacoes_++;
        EntradaMovimento e = hw_.ler();
        bool frenteLivre = e.distancia_mm[1] == 0 || e.distancia_mm[1] > FRENTE_LIVRE_MANOBRA_MM;
        // Frente livre: quem bateu foi a traseira (girando, ela vai para o lado
        // oposto ao novo rumo), então anda para a frente; frente perto, ré.
        // Labirinto novo, 24/11/2026: em (1,1), girando de Oeste para Norte com a
        // parede Sul a 5 cm, a ré levava o robô para cima dela a cada tentativa.
        float manobra = frenteLivre ? RECUO_MM : -RECUO_MM;
        controle_.andar(manobra, controle_.rumo());
        executar();
        somarDesvioPeloRumo(controle_.percorridoMm());
        // Não saiu do lugar: o canto preso está desse lado. Tenta o outro na
        // hora (labirinto novo, 24/11/2026: a ré não andava nada e só a manobra
        // para a frente, na tentativa seguinte, desentalava o giro).
        if (fabsf(controle_.percorridoMm()) < MANOBRA_MINIMA_MM && !hw_.abortado())
        {
            controle_.andar(-manobra, controle_.rumo());
            executar();
            somarDesvioPeloRumo(controle_.percorridoMm());
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
        if (executar() || quaseNoRumo(rumoGraus))
            return true;
    }
    return false;
}

// Faltando até QUASE_NO_RUMO_GRAUS, o giro vale: andando, o controle de rumo
// corrige esse resto.
bool RoboFisico::quaseNoRumo(float rumoGraus)
{
    if (hw_.abortado() || fabsf(normalizarGraus(rumoGraus - controle_.rumo())) > QUASE_NO_RUMO_GRAUS)
        return false;
    ultimo_ = MOV_CONCLUIDO;
    return true;
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
    // Lados no instante da batida (antes da ré): dizem de que lado está a
    // pilastra em que encostou (mover() desvia dela na tentativa seguinte).
    EntradaMovimento eb = hw_.ler();
    ladosNaBatida_[0] = eb.distancia_mm[0];
    ladosNaBatida_[1] = eb.distancia_mm[2];
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
    // Distâncias antes do giro (parado desde o sentir: leituras assentadas):
    // depois dele, outros sensores olham para essas mesmas paredes e confirmam
    // (ou corrigem) o rumo.
    uint16_t antes[3];
    EntradaMovimento e0 = hw_.ler();
    for (uint8_t i = 0; i < 3; i++)
        antes[i] = e0.distancia_mm[i];
    if (gira && fabsf(alem) > CENTRALIZAR_ANTES_DO_GIRO_MM)
    {
        andarMm(-alem, anguloDe(atual, inicial_));
        somarDesvio(atual, percorridoTotal_);
        // Com parede na frente, confere pela leitura nova e completa: a ré
        // curta para antes do pedido (em (2,3), 20 mm além do centro, voltou
        // só 7 e o robô saiu do giro batendo na coluna; labirinto novo, 24/11/2026).
        const uint16_t refFrente = controle_.parametros().frontalCentroMm;
        for (uint8_t k = 0; k < 2 && refFrente && !hw_.abortado(); k++)
        {
            esperar(ESPERA_SENSORES_MS);
            EntradaMovimento e = hw_.ler();
            float medida = 0;
            if (!medirLongitudinal(e, controle_.parametros(), medida) || fabsf(medida) > controle_.parametros().lateralMaximoMm)
                break;
            definirDesvio(atual, medida, desvioAo(girarDireita(atual)));
            if (fabsf(medida) <= CENTRALIZAR_ANTES_DO_GIRO_MM)
                break;
            andarMm(-medida, anguloDe(atual, inicial_));
            somarDesvio(atual, percorridoTotal_);
        }
        EntradaMovimento e = hw_.ler();
        for (uint8_t i = 0; i < 3; i++)
            antes[i] = e.distancia_mm[i];
    }
    uint16_t recuperacoesAntes = recuperacoes_;
    if (gira && !girarPara(rumo))
        return false;
    // Só confere giro que precisou de manobra (entalou ou patinou): é quando a
    // conta dos encoders erra. Num giro limpo não espera os sensores.
    if (trocaFrente && recuperacoes_ > recuperacoesAntes)
        confirmarGiro(antes, atual, d, rumo);
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
    // O lado vem dos sensores no instante da batida: o mais perto que o normal
    // (referência do centro) é o da pilastra; sem leitura que decida, o desvio
    // conhecido. Antes era só o desvio pelos encoders, que errava o lado e o
    // robô batia na mesma pilastra várias vezes (labirinto novo, 24/11/2026:
    // 43 s indo e voltando em (1,1)).
    float lateral = desvioAo(girarDireita(d));
    if (!ok && ultimo_ == MOV_BLOQUEADO)
    {
        ParametrosMovimento &p = controle_.parametros();
        float pertoE = ladosNaBatida_[0] > 0 && p.esquerdaCentroMm ? (float)p.esquerdaCentroMm - ladosNaBatida_[0] : 0;
        float pertoD = ladosNaBatida_[1] > 0 && p.direitaCentroMm ? (float)p.direitaCentroMm - ladosNaBatida_[1] : 0;
        if (pertoE > pertoD + 5)
            somarDesvio(girarEsquerda(d), EMPURRAO_LATERAL_MM); // encostado à esquerda: desvia para a direita
        else if (pertoD > pertoE + 5)
            somarDesvio(girarDireita(d), EMPURRAO_LATERAL_MM);
        else if (fabsf(lateral) > 3)
            somarDesvio(girarDireita(d), lateral > 0 ? EMPURRAO_LATERAL_MM : -EMPURRAO_LATERAL_MM);
    }
    return ok;
}

// Sem BNO085 o rumo vem dos encoders: se uma roda patina ou o robô entala no
// giro, a conta diz "virado para o Norte" e ele está torto (labirinto novo,
// 24/11/2026: em (1,1) a telemetria dizia reto e o robô olhava para a célula
// do lado). Antes do giro os sensores mediram as paredes em 3 direções;
// depois, os sensores que olham para essas direções devem ler o mesmo,
// descontada a posição de cada um no robô (referência do centro de cada
// sensor): esperado = antes - ref(sensor de antes) + ref(sensor de agora).
// Não bateu: procura em passos de BUSCA_PASSO_GRAUS até BUSCA_MAX_GRAUS para
// cada lado o ângulo em que as leituras batem melhor, para nele e assume que
// ele é o rumo pedido.
void RoboFisico::confirmarGiro(const uint16_t antes[3], Direcao frenteAntes, Direcao frenteAgora, float rumo)
{
    ParametrosMovimento &p = controle_.parametros();
    const uint16_t ref[3] = {p.esquerdaCentroMm, p.frontalCentroMm, p.direitaCentroMm};
    if (!ref[0] || !ref[1] || !ref[2])
        return; // sem calibração das paredes não há com o que comparar
    const Direcao olhavaPara[3] = {girarEsquerda(frenteAntes), frenteAntes, girarDireita(frenteAntes)};
    const Direcao olhaPara[3] = {girarEsquerda(frenteAgora), frenteAgora, girarDireita(frenteAgora)};
    int32_t esperado[3] = {-1, -1, -1};
    uint8_t n = 0;
    for (uint8_t j = 0; j < 3; j++)
        for (uint8_t i = 0; i < 3; i++)
            if (olhavaPara[i] == olhaPara[j] && antes[i] > 0 && antes[i] <= ALCANCE_CONFIRMACAO_MM)
            {
                esperado[j] = (int32_t)antes[i] - ref[i] + ref[j];
                n++;
            }
    if (n == 0)
        return;
    // Erro médio (mm) entre o esperado e o lido agora.
    auto erro = [&]() {
        esperar(ESPERA_SENSORES_MS);
        EntradaMovimento e = hw_.ler();
        float soma = 0;
        for (uint8_t j = 0; j < 3; j++)
            if (esperado[j] >= 0)
                soma += fminf(fabsf((float)e.distancia_mm[j] - esperado[j]), ERRO_MAXIMO_CONFIRMACAO_MM);
        return soma / n;
    };
    float melhor = erro();
    if (melhor <= TOLERANCIA_CONFIRMACAO_MM || hw_.abortado())
        return;
    float melhorDesvio = 0;
    for (float desvio = -BUSCA_MAX_GRAUS; desvio <= BUSCA_MAX_GRAUS && !hw_.abortado(); desvio += BUSCA_PASSO_GRAUS)
    {
        if (desvio == 0)
            continue;
        controle_.girar(normalizarGraus(rumo + desvio));
        executar();
        float e = erro();
        if (e < melhor)
        {
            melhor = e;
            melhorDesvio = desvio;
        }
    }
    if (hw_.abortado())
        return;
    controle_.girar(normalizarGraus(rumo + melhorDesvio));
    executar();
    // Onde as leituras batem é o rumo pedido: a conta dos encoders estava
    // errada em melhorDesvio graus.
    controle_.reiniciar(hw_.ler(), rumo);
    rumosCorrigidos_++;
}
