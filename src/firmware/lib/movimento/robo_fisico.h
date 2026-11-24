#pragma once

#include <stdint.h>

#include "controle_movimento.h"
#include "labirinto.h"
#include "modo_automatico.h"

// O que o RoboFisico precisa do hardware. No ESP32, o main.cpp implementa com
// os sensores e os motores (e aproveita ler() para manter a telemetria, a
// bateria e o console em dia durante o movimento); nos testes, um robô
// simulado com física simples.
class HardwareRobo
{
public:
    virtual ~HardwareRobo() {}
    // Um ciclo: atualiza sensores e motores e devolve a leitura. Quem chama
    // fica em laço nisso enquanto o robô se move.
    virtual EntradaMovimento ler() = 0;
    virtual void aplicar(const SaidaMovimento &s) = 0;
    // Os 3 VL53L0X respondendo (o BNO085 é opcional: sem ele, rumo pelos encoders).
    virtual bool sensoresOk() = 0;
    // A corrida foi interrompida de fora (botão, console, bateria): parar já.
    virtual bool abortado() = 0;
    // Chamado a cada ciclo de movimento, para log (CT-SE07). Opcional.
    virtual void observar(const ControleMovimento &) {}
};

// O robô de verdade para o modo automático: vira com o giroscópio (ou os
// encoders), anda uma célula com o controle de movimento e lê as paredes com
// os VL53L0X parados no centro da célula.
class RoboFisico : public InterfaceRobo
{
public:
    // Depois de parar, espera a mediana dos VL53L0X (5 leituras de ~33 ms)
    // trocar as leituras do caminho pelas do centro da célula.
    static const uint16_t ESPERA_SENSORES_MS = 200;
    // Giro bloqueado (encostou ou pivotou): manobra isso (ré ou frente) e
    // tenta girar de novo, até MAX_TENTATIVAS_GIRO vezes no total.
    static constexpr float RECUO_MM = 30;
    static const uint8_t MAX_TENTATIVAS_GIRO = 4;
    // Manobra para a frente só com a frente mais longe que isso.
    static const uint16_t FRENTE_LIVRE_MANOBRA_MM = 70;
    // Giro de 90° perto da parede da varredura: avanço no meio (ver girarPara).
    static constexpr float AVANCO_MEIO_GIRO_MM = 15;
    // Giro que parou (bloqueado ou tempo esgotado) faltando até isso: vale.
    static constexpr float QUASE_NO_RUMO_GRAUS = 10;
    static const uint16_t PERTO_DA_VARREDURA_MM = 5; // parede pelo menos isso mais perto que o centro
    // Manobra que andou menos que isso: preso desse lado, tenta o outro.
    static constexpr float MANOBRA_MINIMA_MM = 5;
    // Andar bloqueado: a próxima tentativa exagera o desvio lateral nisso.
    static constexpr float EMPURRAO_LATERAL_MM = 15;
    // Confirmação do giro pelos sensores (confirmarGiro): só paredes até esse
    // alcance contam; erro médio até a tolerância é "virou certo"; senão
    // procura até BUSCA_MAX_GRAUS para cada lado, em passos de BUSCA_PASSO_GRAUS.
    static const uint16_t ALCANCE_CONFIRMACAO_MM = 400;
    static constexpr float TOLERANCIA_CONFIRMACAO_MM = 30;
    static constexpr float ERRO_MAXIMO_CONFIRMACAO_MM = 150;
    static constexpr float BUSCA_MAX_GRAUS = 40;
    static constexpr float BUSCA_PASSO_GRAUS = 10;
    // Mais longe que isso do centro (pelos encoders), sentir() não marca nada.
    static constexpr float FORA_DO_CENTRO_MM = 40;
    // Parede lida a mais que isso além da referência do centro do sensor: não marca.
    static const uint16_t PAREDE_AMBIGUA_MM = 40;
    // Mais longe que isso do centro (ao longo da direção atual), volta antes de girar.
    static constexpr float CENTRALIZAR_ANTES_DO_GIRO_MM = 8;
    // No sentir, mais longe que isso do centro pela parede da frente: acerta antes de ler os lados.
    static constexpr float CENTRALIZAR_NO_SENTIR_MM = 18;
    // Mais torto que isso em relação ao corredor, os sensores leem as paredes
    // na diagonal: sentir() não marca nada (o mapa ficaria errado).
    static constexpr float ALINHADO_GRAUS = 15;
    // Giro maior que isso troca a parede que a frente vê (desliga a correção pelo fundo).
    static constexpr float TROCA_FRENTE_GRAUS = 45;

    RoboFisico(HardwareRobo &hw, ControleMovimento &controle, Direcao orientacaoInicial = NORTE)
        : hw_(hw), controle_(controle), inicial_(orientacaoInicial) {}

    void definirOrientacaoInicial(Direcao d) { inicial_ = d; }

    bool calibrar() override;
    void sentir(Labirinto &mapa) override;
    bool mover(Direcao d) override;

    // Movimentos soltos, para a bancada (CT-SE05 a CT-SE07) e para o mover().
    bool girarPara(float rumoGraus);
    bool andarMm(float mm, float rumoGraus, float lateralInicialMm = 0, bool corrigirPeloFundo = false);

    // Direção física do robô agora, pelo rumo estimado.
    Direcao direcao() const { return direcaoDe(controle_.rumo(), inicial_); }
    EstadoMovimento ultimoResultado() const { return ultimo_; }
    uint16_t recuperacoes() const { return recuperacoes_; } // voltas ao centro depois de bloqueio (RF16-S)
    uint16_t rumosCorrigidos() const { return rumosCorrigidos_; } // giros corrigidos pelos sensores
    uint16_t leiturasTortas() const { return leiturasTortas_; } // sentir() sem marcar, robô torto ou fora do centro
    // Sem calibração da frente, a primeira parada no centro de uma célula com
    // parede na frente (pelos encoders) ensina a distância de referência.
    bool aprendeuFrente() const { return aprendeuFrente_; }
    // Quanto o robô está fora do centro da célula, nos eixos do mapa (mm,
    // x para Leste, y para Norte). O próximo mover() compensa.
    float desvioX() const { return desvioX_; }
    float desvioY() const { return desvioY_; }

private:
    bool executar();
    void confirmarGiro(const uint16_t antes[3], Direcao frenteAntes, Direcao frenteAgora, float rumo);
    void esperar(uint16_t ms);

    HardwareRobo &hw_;
    ControleMovimento &controle_;
    Direcao inicial_;
    void definirDesvio(Direcao d, float longitudinal, float lateral);
    float desvioAo(Direcao d) const;
    void somarDesvio(Direcao d, float mm);
    void somarDesvioPeloRumo(float mm);
    bool quaseNoRumo(float rumoGraus);

    EstadoMovimento ultimo_ = MOV_OCIOSO;
    uint16_t recuperacoes_ = 0;
    uint16_t leiturasTortas_ = 0;
    uint16_t rumosCorrigidos_ = 0;
    bool aprendeuFrente_ = false;
    float percorridoTotal_ = 0; // ida e volta do último andarMm
    float desvioX_ = 0, desvioY_ = 0;
};
