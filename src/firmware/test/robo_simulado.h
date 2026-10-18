#pragma once

#include <stdint.h>

#include "direcao.h"
#include "floodfill.h"
#include "labirinto.h"

// Robô de teste: conhece o labirinto de verdade (gabarito), mas só enxerga o
// que os 3 sensores veriam — frente, esquerda e direita da célula onde está —
// e vai anotando no próprio mapa. Conta passos e giros de 90°.
// Larga em (0,0) virado para o Norte.
class RoboSimulado
{
public:
    explicit RoboSimulado(const Labirinto &gabarito) : gabarito_(gabarito)
    {
        mapa_.iniciar(gabarito.largura(), gabarito.altura());
        for (uint8_t x = 0; x < gabarito.largura(); x++)
            for (uint8_t y = 0; y < gabarito.altura(); y++)
                if (gabarito.ehObjetivo(x, y))
                {
                    objetivoX_ = x;
                    objetivoY_ = y;
                }
        mapa_.definirObjetivo(objetivoX_, objetivoY_);
        sentir();
    }

    // Vira para d e anda uma célula, seguindo o contrato do labirinto.h:
    // consulta podeAvancar() antes do motor e registra com avancar() depois.
    // false se o mapa ou o labirinto real tiver parede, ou se a pose perder a
    // sincronia (nesse caso, sincronizado() passa a ser false).
    bool mover(Direcao d)
    {
        contarGiro(d);
        mapa_.girar(d);
        if (!mapa_.podeAvancar())
            return false;
        if (gabarito_.temParede(mapa_.x(), mapa_.y(), d))
            return false;

        uint8_t anteriorX = mapa_.x();
        uint8_t anteriorY = mapa_.y();
        if (!mapa_.avancar())
        {
            sincronizado_ = false;
            return false;
        }
        mapa_.marcarAberta(anteriorX, anteriorY, d);
        passos_++;
        sentir();
        return true;
    }

    // Explora com o Flood Fill até o objetivo, recalculando a cada célula.
    bool explorar(uint16_t limitePassos = 1000)
    {
        while (!mapa_.noObjetivo())
        {
            if (passos_ >= limitePassos)
                return false;
            floodFill_.recalcular(mapa_, EXPLORACAO);
            Direcao d;
            if (!floodFill_.proximaDirecao(mapa_, d) || !mover(d))
                return false;
        }
        return true;
    }

    // Explora até a rota rápida ser comprovadamente a menor (CT-SE14, passo 1).
    // Depois da primeira chegada, vai e volta entre objetivo e largada. Cada
    // viagem que não comprova a rota acha ao menos uma parede nova, então o
    // laço termina. Para a volta, a largada vira o objetivo do Flood Fill.
    bool explorarAteRotaGarantida(uint16_t limitePassos = 1000)
    {
        if (!explorar(limitePassos))
            return false;
        bool naLargada = false; // onde o robô está: largada ou objetivo
        while (!floodFill_.rotaGarantida(mapa_))
        {
            uint8_t destinoX = naLargada ? objetivoX_ : 0;
            uint8_t destinoY = naLargada ? objetivoY_ : 0;
            mapa_.definirObjetivo(destinoX, destinoY);
            bool chegou = explorar(limitePassos);
            mapa_.definirObjetivo(objetivoX_, objetivoY_);
            if (!chegou)
                return false;
            naLargada = !naLargada;
        }
        return true;
    }

    const Labirinto &mapa() const { return mapa_; }
    FloodFill &floodFill() { return floodFill_; }
    uint16_t passos() const { return passos_; }
    uint16_t giros() const { return giros_; }
    bool sincronizado() const { return sincronizado_; }

private:
    void sentir()
    {
        Direcao frente = mapa_.orientacao();
        revelar(frente);
        revelar(girarDireita(frente));
        revelar(girarEsquerda(frente));
    }

    void revelar(Direcao d)
    {
        uint8_t x = mapa_.x();
        uint8_t y = mapa_.y();
        if (gabarito_.temParede(x, y, d))
            mapa_.marcarParede(x, y, d);
        else
            mapa_.marcarAberta(x, y, d);
    }

    void contarGiro(Direcao d)
    {
        Direcao frente = mapa_.orientacao();
        if (d == oposta(frente))
            giros_ += 2;
        else if (d != frente)
            giros_ += 1;
    }

    Labirinto gabarito_;
    Labirinto mapa_;
    FloodFill floodFill_;
    uint8_t objetivoX_ = 0;
    uint8_t objetivoY_ = 0;
    uint16_t passos_ = 0;
    uint16_t giros_ = 0;
    bool sincronizado_ = true;
};
