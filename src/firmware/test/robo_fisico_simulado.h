#pragma once

#include <math.h>
#include <stdint.h>

#include "controle_movimento.h"
#include "filtro_distancia.h"
#include "labirinto.h"
#include "robo_fisico.h"

// Robô simulado para testar o controle de movimento e o modo automático sem
// o robô: duas rodas com rampa e atraso, uma roda mais fraca que a outra,
// encoders, BNO085 (que pode falhar), os 3 VL53L0X medindo as paredes do
// labirinto de verdade (com o mesmo filtro de mediana e detector do robô) e
// colisão: se o corpo encostar numa parede, as rodas travam.
//
// Coordenadas em mm: x para Leste, y para Norte; a célula (i, j) vai de
// 180·i a 180·(i+1). Paredes de 12 mm, metade em cada célula.
class RoboFisicoSimulado : public HardwareRobo
{
public:
    // Geometria do robô simulado (centro = entre as rodas)
    static constexpr float MEIA_PAREDE = 6;
    static constexpr float SENSOR_FRENTE = 45; // à frente do centro
    static constexpr float SENSOR_LADO_FRENTE = 20;
    static constexpr float SENSOR_LADO = 35;   // para cada lado
    static constexpr float RAIO_CORPO = 58;
    // Leituras no centro da célula com parede: o que a calibração 'L' mediria.
    static constexpr float FRONTAL_CENTRO = CELULA_MM / 2 - MEIA_PAREDE - SENSOR_FRENTE;   // 39
    static constexpr float LATERAL_CENTRO = CELULA_MM / 2 - MEIA_PAREDE - SENSOR_LADO;     // 49

    static constexpr uint32_t DT_MS = 5;
    static constexpr float VEL_MAX_MM_S = 450;   // roda com potência 1 (5 V)
    static constexpr float ZONA_MORTA = 0.12f;   // abaixo disso o motor não gira
    static constexpr float RAMPA_POR_S = 2.0f;   // igual ao Motores::atualizar
    static constexpr float ATRASO_S = 0.04f;

    explicit RoboFisicoSimulado(const Labirinto &gabarito) : gab_(gabarito) { colocar(0, 0, gabarito.orientacaoInicial()); }

    // Põe o robô no centro da célula, virado para d, com desvio lateral
    // (mm, + = para a direita) e de ângulo (graus, + = para a esquerda).
    void colocar(uint8_t cx, uint8_t cy, Direcao d, float desvioLateral = 0, float desvioGraus = 0)
    {
        phi0_ = anguloMat(gab_.orientacaoInicial());
        float frente = anguloMat(d);
        x_ = (cx + 0.5f) * CELULA_MM + desvioLateral * sinf(frente);
        y_ = (cy + 0.5f) * CELULA_MM - desvioLateral * cosf(frente);
        phi_ = frente + desvioGraus / 57.2957795f;
        for (auto &f : filtros_)
            f.limpar();
        for (int i = 0; i < 6; i++)
            medir();
    }

    // ---- HardwareRobo ----
    EntradaMovimento ler() override
    {
        passo();
        EntradaMovimento e = {};
        e.agora_ms = agora_;
        e.encoderEsquerdo = (int32_t)lroundf(pulsosE_);
        e.encoderDireito = (int32_t)lroundf(pulsosD_);
        e.yawOk = yawOk;
        e.yawGraus = rumoVerdadeiro();
        for (int i = 0; i < 3; i++)
        {
            e.distancia_mm[i] = filtros_[i].valor();
            e.parede[i] = paredes_[i];
        }
        return e;
    }

    void aplicar(const SaidaMovimento &s) override
    {
        if (s.frear)
        {
            cmdE_ = cmdD_ = rampaE_ = rampaD_ = velE_ = velD_ = 0;
            return;
        }
        cmdE_ = s.esquerdo;
        cmdD_ = s.direito;
    }

    bool sensoresOk() override { return true; }
    bool abortado() override { return abortarEm_ != 0 && agora_ >= abortarEm_; }
    void observar(const ControleMovimento &c) override
    {
        if (c.temReferenciaLateral())
        {
            float e = fabsf(erroLateralVerdadeiro());
            if (ciclosObservados_ > 200 && e > maiorErroLateralDepois_)
                maiorErroLateralDepois_ = e;
        }
        ciclosObservados_++;
    }

    // ---- estado verdadeiro, para os testes ----
    float rumoVerdadeiro() const { return normalizarGraus((phi_ - phi0_) * 57.2957795f); }
    int celulaX() const { return (int)floorf(x_ / CELULA_MM); }
    int celulaY() const { return (int)floorf(y_ / CELULA_MM); }
    // Distância do centro da célula atual, ao longo de x e de y (mm).
    float foraDoCentroX() const { return x_ - (celulaX() + 0.5f) * CELULA_MM; }
    float foraDoCentroY() const { return y_ - (celulaY() + 0.5f) * CELULA_MM; }
    float foraDoCentro() const { return sqrtf(foraDoCentroX() * foraDoCentroX() + foraDoCentroY() * foraDoCentroY()); }
    // Desvio para a direita do eixo do corredor, na direção em que o robô aponta.
    float erroLateralVerdadeiro() const
    {
        Direcao d = direcaoDe(rumoVerdadeiro(), gab_.orientacaoInicial());
        float f = anguloMat(d);
        return foraDoCentroX() * sinf(f) - foraDoCentroY() * cosf(f);
    }
    bool colidiu() const { return colisoes_ > 0; }
    uint32_t agora() const { return agora_; }

    // Perturbações
    bool yawOk = true;
    float ganhoRodaEsquerda = 1.0f;
    float ganhoRodaDireita = 0.93f;    // uma roda mais fraca: o controle precisa corrigir
    float larguraEixoReal = 95.0f;
    // Potência mínima para cada roda sair do lugar parada (atrito estático,
    // motor mais duro). Girando, basta ZONA_MORTA.
    float zonaMortaEsquerda = ZONA_MORTA;
    float zonaMortaDireita = ZONA_MORTA;
    uint32_t abortarEm_ = 0;
    float maiorErroLateralDepois_ = 0; // depois do primeiro segundo de movimento

private:
    static float anguloMat(Direcao d)
    {
        switch (d)
        {
        case NORTE: return 1.5707963f;
        case OESTE: return 3.1415927f;
        case SUL: return -1.5707963f;
        default: return 0;
        }
    }

    static float aproximar(float atual, float alvo, float passo)
    {
        if (alvo > atual)
            return atual + passo > alvo ? alvo : atual + passo;
        return atual - passo < alvo ? alvo : atual - passo;
    }

    // Atrito estático: parada, a roda só sai com zonaMorta; girando, continua
    // até ZONA_MORTA (o atrito de quem já está girando é menor).
    float velocidade(float potencia, float ganho, float zonaMorta, float velAtual) const
    {
        float limiar = fabsf(velAtual) > 5 ? (zonaMorta < ZONA_MORTA ? zonaMorta : ZONA_MORTA) : zonaMorta;
        if (fabsf(potencia) < limiar)
            return 0;
        return potencia * VEL_MAX_MM_S * ganho;
    }

    void passo()
    {
        const float dt = DT_MS / 1000.0f;
        agora_ += DT_MS;
        rampaE_ = aproximar(rampaE_, cmdE_, RAMPA_POR_S * dt);
        rampaD_ = aproximar(rampaD_, cmdD_, RAMPA_POR_S * dt);
        velE_ += (velocidade(rampaE_, ganhoRodaEsquerda, zonaMortaEsquerda, velE_) - velE_) * dt / ATRASO_S;
        velD_ += (velocidade(rampaD_, ganhoRodaDireita, zonaMortaDireita, velD_) - velD_) * dt / ATRASO_S;

        float v = (velE_ + velD_) / 2;
        float w = (velD_ - velE_) / larguraEixoReal;
        float nx = x_ + v * cosf(phi_) * dt;
        float ny = y_ + v * sinf(phi_) * dt;
        float nphi = phi_ + w * dt;
        if (encosta(nx, ny))
        {
            // Encostou: o robô não sai do lugar e as rodas travam.
            colisoes_++;
            velE_ = velD_ = 0;
        }
        else
        {
            x_ = nx;
            y_ = ny;
            phi_ = nphi;
            pulsosE_ += velE_ * dt * pulsosPorMm_;
            pulsosD_ += velD_ * dt * pulsosPorMm_;
        }
        if (agora_ - ultimaMedida_ >= 33) // VL53L0X em modo contínuo
        {
            ultimaMedida_ = agora_;
            medir();
        }
    }

    bool dentroParede(float x, float y) const
    {
        const float lim = CELULA_MM - MEIA_PAREDE;
        if (x < MEIA_PAREDE || y < MEIA_PAREDE || x > gab_.largura() * CELULA_MM - MEIA_PAREDE ||
            y > gab_.altura() * CELULA_MM - MEIA_PAREDE)
            return true;
        int cx = (int)(x / CELULA_MM), cy = (int)(y / CELULA_MM);
        float lx = x - cx * CELULA_MM, ly = y - cy * CELULA_MM;
        bool o = lx < MEIA_PAREDE, l = lx > lim, s = ly < MEIA_PAREDE, n = ly > lim;
        if ((o || l) && (s || n))
            return true; // postes dos cantos
        return (o && gab_.temParede(cx, cy, OESTE)) || (l && gab_.temParede(cx, cy, LESTE)) ||
               (s && gab_.temParede(cx, cy, SUL)) || (n && gab_.temParede(cx, cy, NORTE));
    }

    bool encosta(float x, float y) const
    {
        for (int i = 0; i < 16; i++)
        {
            float a = i * 0.3926991f;
            if (dentroParede(x + RAIO_CORPO * cosf(a), y + RAIO_CORPO * sinf(a)))
                return true;
        }
        return false;
    }

    uint16_t raio(float frente, float lado, float direcao)
    {
        float sx = x_ + frente * cosf(phi_) - lado * sinf(phi_);
        float sy = y_ + frente * sinf(phi_) + lado * cosf(phi_);
        float a = phi_ + direcao;
        for (int d = 0; d < (int)DIST_MAX_CONFIAVEL; d++)
            if (dentroParede(sx + d * cosf(a), sy + d * sinf(a)))
            {
                // Ruído de ±3 mm, determinístico.
                semente_ = semente_ * 1103515245u + 12345u;
                int ruido = (int)((semente_ >> 16) % 7) - 3;
                int mm = d + ruido;
                return (uint16_t)(mm < 1 ? 1 : mm);
            }
        return DIST_SEM_ALVO;
    }

    void medir()
    {
        uint16_t mm[3] = {raio(SENSOR_LADO_FRENTE, SENSOR_LADO, 1.5707963f), raio(SENSOR_FRENTE, 0, 0),
                          raio(SENSOR_LADO_FRENTE, -SENSOR_LADO, -1.5707963f)};
        for (int i = 0; i < 3; i++)
        {
            filtros_[i].adicionar(mm[i]);
            paredes_[i] = detectores_[i].atualizar(filtros_[i].valor());
        }
    }

    const Labirinto &gab_;
    float x_ = 0, y_ = 0, phi_ = 0, phi0_ = 0;
    float cmdE_ = 0, cmdD_ = 0, rampaE_ = 0, rampaD_ = 0, velE_ = 0, velD_ = 0;
    float pulsosE_ = 0, pulsosD_ = 0;
    const float pulsosPorMm_ = ParametrosMovimento().pulsosPorMm;
    uint32_t agora_ = 0, ultimaMedida_ = 0;
    uint32_t colisoes_ = 0;
    uint32_t ciclosObservados_ = 0;
    uint32_t semente_ = 12345;
    FiltroDistancia filtros_[3];
    DetectorParede detectores_[3] = {DetectorParede(), DetectorParede(150, 170), DetectorParede()};
    bool paredes_[3] = {false, false, false};
};

// Parâmetros como se a calibração 'L' tivesse sido feita no robô simulado.
inline ParametrosMovimento parametrosCalibrados()
{
    ParametrosMovimento p;
    p.frontalCentroMm = (uint16_t)RoboFisicoSimulado::FRONTAL_CENTRO;
    p.esquerdaCentroMm = (uint16_t)RoboFisicoSimulado::LATERAL_CENTRO;
    p.direitaCentroMm = (uint16_t)RoboFisicoSimulado::LATERAL_CENTRO;
    return p;
}
