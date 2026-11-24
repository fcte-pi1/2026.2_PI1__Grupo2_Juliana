#pragma once

#include <stdint.h>

#include "direcao.h"
#include "filtro_distancia.h" // CELULA_MM (18 cm, TAP e RF01-L) e DIST_MAX_CONFIAVEL

// Controle de movimento célula a célula (RF03-S, RF15-S, RF16-S). Lógica pura,
// sem Arduino: o robô chama atualizar() no laço com os encoders, o yaw e as
// distâncias, e aplica a potência devolvida nos motores. Os testes do
// computador fazem o mesmo com um robô simulado.
//
// Ângulos em graus, positivo = giro à esquerda (como o BNO085), 0 = para onde
// o robô apontava na largada.

// Constantes do robô. Os ganhos e as potências são o ponto de partida achado no
// simulador: AJUSTE NO ROBÔ (comandos F, <, > e U do console).
struct ParametrosMovimento
{
    float pulsosPorMm = 3305.0f / (3.14159265f * 34.0f); // encoder (motores.h) / perímetro da roda
    float larguraEixoMm = 95.0f; // MEDIR: centro a centro das rodas. Só vale sem o BNO085.

    // Andar (potência = fração dos 5 V, como Motores::definir). Máximo pedido
    // pela equipe: PWM 50 de 255 com a bateria em 7,9 V = 1,56 V no motor.
    float potenciaMaxima = 0.31f;
    float potenciaMinima = 0.22f;     // abaixo disso o N20 pode não sair do lugar
    float ganhoDistancia = 0.005f;    // potência por mm que falta
    float ganhoRumo = 0.015f;         // potência de giro por grau de erro
    float correcaoMaxima = 0.5f;      // potência de giro máxima andando (0,25 saturava e o robô batia na parede direita)
    float ganhoLateral = 0.4f;        // graus de correção por mm fora do centro
    float correcaoLateralMaxima = 12; // graus
    // Sem o BNO085: o rumo medido pelas paredes (variação do erro lateral ao
    // longo de pelo menos baseRumoParedeMm) corrige o rumo dos encoders.
    float baseRumoParedeMm = 50;
    // Quando uma parede lateral aparece ou some, o sensor passa pelo canto e
    // mede valores intermediários. A medida lateral só vale depois de
    // estabilizarParedeMm andados sem trocar de parede nem saltar mais de
    // saltoLateralMm de uma leitura para a outra, e se der no máximo
    // lateralMaximoMm do eixo.
    float estabilizarParedeMm = 40;
    float saltoLateralMm = 8;
    float lateralMaximoMm = 40;
    float pesoRumoParede = 0.5f; // 0 = desliga, 1 = confia só na parede
    float toleranciaDistanciaMm = 5;

    // Girar no lugar
    float potenciaGiroMaxima = 0.31f;
    float potenciaGiroMinima = 0.40f; // 0,24: freando, a esquerda (para trás) travava abaixo de ~0,35 no labirinto (24/11/2026)
    float ganhoGiro = 0.008f; // potência por grau de erro
    float toleranciaGiroGraus = 2;
    uint16_t assentarGiroMs = 80; // tempo parado dentro da tolerância
    // No giro as duas rodas andam sempre juntas, em sentidos opostos (no
    // próprio eixo). A que se adianta perde potência e a outra ganha
    // ganhoSincronia por mm de diferença. Roda parada: as duas sobem juntas o
    // reforço até trancoGiro, mesmo acima do teto do PWM, só para vencer o atrito
    // parado (chão, 23/11/2026: com PWM até 70 nenhuma roda saía do lugar e,
    // esperando uma pela outra, o robô pivotava numa roda e depois na outra).
    float ganhoSincronia = 0.01f;
    float trancoGiro = 0.6f;

    uint16_t tempoMaximoMs = 4000; // por comando
    uint16_t tempoParadoMs = 800;  // com potência e sem pulsos nas duas rodas: bloqueado (colisão)

    // Roda parada com potência (atrito, motor mais duro que o outro): depois de
    // rodaParadaMs sem pulsos, a outra roda espera e a potência da parada sobe
    // reforcoPorSegundo até ela andar (no máximo potenciaArranqueMaxima). O
    // reforço fica aprendido para os próximos movimentos. Sem isso, uma roda
    // gira e a outra não, e o robô pivota em vez de girar no lugar (bancada,
    // 23/11/2026: bateu na parede no primeiro giro).
    uint16_t rodaParadaMs = 120;
    float reforcoPorSegundo = 1.2f;
    float potenciaArranqueMaxima = 0.65f;
    uint16_t esperaMaximaMs = 400; // andando, a roda que anda espera a travada no máximo isso

    // Parede da frente mais perto que isso, girando ou andando: para na hora,
    // calibrado ou não (no centro da célula a parede fica a ~4-6 cm).
    uint16_t distanciaMinimaFrenteMm = 25;

    // Distâncias com o robô no centro da célula e parede do lado, medidas pela
    // calibração 'L' (#279). 0 = sem calibração: o controle usa só os encoders
    // para parar e só as duas paredes juntas para centralizar.
    uint16_t frontalCentroMm = 0;
    uint16_t esquerdaCentroMm = 0;
    uint16_t direitaCentroMm = 0;
};

// O que o controle lê a cada ciclo. Distâncias e paredes na ordem dos sensores
// (LadoSensor): esquerdo, frontal, direito.
struct EntradaMovimento
{
    uint32_t agora_ms;
    int32_t encoderEsquerdo; // pulsos, positivo = roda para frente
    int32_t encoderDireito;
    bool yawOk;              // BNO085 respondendo
    float yawGraus;
    uint16_t distancia_mm[3];
    bool parede[3];
};

struct SaidaMovimento
{
    float esquerdo; // -1 a 1, para Motores::definir
    float direito;
    bool frear;     // terminou: freio ativo
};

enum EstadoMovimento : uint8_t
{
    MOV_OCIOSO,
    MOV_ANDANDO,
    MOV_GIRANDO,
    MOV_CONCLUIDO,
    MOV_BLOQUEADO,     // as rodas pararam com potência (colisão) ou parede inesperada na frente
    MOV_TEMPO_ESGOTADO
};

const char *nomeMovimento(EstadoMovimento e);

// Ângulo (graus) da direção d do mapa, contado a partir da orientação inicial.
float anguloDe(Direcao d, Direcao orientacaoInicial);
// Direção do mapa mais perto do ângulo.
Direcao direcaoDe(float graus, Direcao orientacaoInicial);
// Ângulo em (-180, 180].
float normalizarGraus(float graus);

// Quanto o robô está fora do eixo do corredor (mm, + = à direita), pelas
// paredes dos lados: com as duas, a diferença; com uma só, a distância de
// referência da calibração. Devolve de onde veio a medida (0 = sem parede
// para medir, 1 = duas paredes, 2 = só a esquerda, 3 = só a direita).
uint8_t medirLateral(const EntradaMovimento &e, const ParametrosMovimento &p, float &lateral);
// Quanto o robô passou do centro da célula na direção em que aponta (mm),
// pela parede da frente. false sem parede na frente ou sem calibração.
bool medirLongitudinal(const EntradaMovimento &e, const ParametrosMovimento &p, float &longitudinal);

class ControleMovimento
{
public:
    explicit ControleMovimento(const ParametrosMovimento &p = ParametrosMovimento()) : p_(p) {}

    ParametrosMovimento &parametros() { return p_; }
    const ParametrosMovimento &parametros() const { return p_; }

    // Maior variação do yaw aceita entre dois ciclos. Mais que isso é o
    // BNO085 reiniciando (o yaw volta com outra referência): o ciclo usa os encoders.
    static constexpr float SALTO_YAW_GRAUS = 20;

    // Zera a odometria: o robô aponta para rumoGraus agora (na largada, 0).
    void reiniciar(const EntradaMovimento &e, float rumoGraus = 0);
    // Anda mm (negativo = de ré) mantendo o rumo, centralizando nas paredes e
    // parando pela parede da frente quando houver. lateralInicialMm é quanto
    // o robô já está fora do eixo (+ = à direita): sem parede dos lados, o
    // controle corrige pela odometria.
    void andar(float mm, float rumoGraus, float lateralInicialMm = 0);
    // Gira no lugar até o rumo.
    void girar(float rumoGraus);
    void parar() { estado_ = MOV_OCIOSO; }

    SaidaMovimento atualizar(const EntradaMovimento &e);

    EstadoMovimento estado() const { return estado_; }
    bool terminou() const { return estado_ >= MOV_CONCLUIDO; }
    bool ocupado() const { return estado_ == MOV_ANDANDO || estado_ == MOV_GIRANDO; }

    // Para o log (CT-SE07): rumo estimado, distância andada no comando atual,
    // erro lateral (mm, + = robô à direita do centro) e potência de correção.
    float rumo() const { return rumo_; }
    float percorridoMm() const { return percorrido_; }
    float erroLateralMm() const { return erroLateral_; }
    // Fora do eixo agora (+ = à direita), pelas paredes ou, sem elas, pela odometria.
    float lateralMm() const { return lateral_; }
    bool temReferenciaLateral() const { return temLateral_; }
    float sinalControle() const { return correcao_; }
    // Potência mínima aprendida de cada roda (0 = ainda não travou).
    float reforcoEsquerdo() const { return reforcoE_; }
    float reforcoDireito() const { return reforcoD_; }
    bool paradaPelaFrente() const { return usouFrente_; }

private:
    void odometria(const EntradaMovimento &e);
    void comecar(const EntradaMovimento &e);
    SaidaMovimento andando(const EntradaMovimento &e);
    void reforcarRodasParadas(const EntradaMovimento &e, SaidaMovimento &s);
    bool frenteMuitoPerto(const EntradaMovimento &e) const;
    void corrigirRumoPelaParede(const EntradaMovimento &e, uint8_t fonte);
    SaidaMovimento girando(const EntradaMovimento &e);
    bool vigiar(const EntradaMovimento &e); // bloqueio e tempo esgotado
    SaidaMovimento terminar(EstadoMovimento fim);

    ParametrosMovimento p_;
    EstadoMovimento estado_ = MOV_OCIOSO;
    bool comecou_ = false;

    // odometria
    bool temLeitura_ = false;
    int32_t ultimoE_ = 0, ultimoD_ = 0;
    float rumo_ = 0;
    // reforço de arranque de cada roda
    uint32_t ultimoPulsoE_ = 0, ultimoPulsoD_ = 0, ultimoCiclo_ = 0;
    float reforcoE_ = 0, reforcoD_ = 0;
    uint32_t esperaDesde_ = 0; // uma roda travada e a outra esperando desde
    bool yawAnteriorOk_ = false;
    float yawAnterior_ = 0;
    float avancoUltimo_ = 0; // mm andados no último ciclo (média das rodas)

    // comando atual
    float alvoMm_ = 0;
    float rumoAlvo_ = 0;
    int32_t inicioE_ = 0, inicioD_ = 0;
    uint32_t inicio_ = 0;
    uint32_t ultimoPulso_ = 0;
    uint32_t dentroDesde_ = 0;
    bool dentro_ = false;

    float percorrido_ = 0;
    float erroLateral_ = 0;
    float lateral_ = 0;
    uint8_t fonteRef_ = 0; // referência do rumo pela parede: fonte, lateral e avanço
    float lateralRef_ = 0, avancoRef_ = 0;
    uint8_t fonteAtual_ = 0; // de onde vem a medida lateral agora e onde ela trocou
    float trocaFonte_ = 0;
    float medidaAnterior_ = 0;
    bool temLateral_ = false;
    float correcao_ = 0;
    bool usouFrente_ = false;
};
