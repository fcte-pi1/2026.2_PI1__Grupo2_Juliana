#pragma once

#include <stdint.h>

#include "direcao.h"

const uint8_t MAX_LARG = 16;
const uint8_t MAX_ALT = 16;

// Mapa do labirinto em memória fixa (RNF03). A célula (0,0) é a largada
// x cresce para Leste e y para Norte.
class Labirinto
{
public:
    void iniciar(uint8_t largura, uint8_t altura);      // zera tudo e fecha as bordas
    void marcarParede(uint8_t x, uint8_t y, Direcao d); // grava nos dois lados
    void marcarAberta(uint8_t x, uint8_t y, Direcao d); // lado conhecido e sem parede
    bool temParede(uint8_t x, uint8_t y, Direcao d) const;
    bool conhecida(uint8_t x, uint8_t y, Direcao d) const;
    uint8_t paredesDe(uint8_t x, uint8_t y) const; // máscara 0..15 da telemetria
    uint8_t largura() const { return largura_; }
    uint8_t altura() const { return altura_; }

    // Pose do robô
    bool avancar(); // anda uma célula na orientação atual; false se houver parede
    void girar(Direcao nova);
    uint8_t x() const { return x_; }
    uint8_t y() const { return y_; }
    Direcao orientacao() const { return orientacao_; }
    void marcarVisitada(uint8_t x, uint8_t y);
    bool visitada(uint8_t x, uint8_t y) const;

    // Objetivo: o canto diagonalmente oposto à largada, definido
    // por iniciar() em (largura-1, altura-1). definirObjetivo() troca a célula.
    bool definirObjetivo(uint8_t x, uint8_t y); // false se fora do labirinto
    bool ehObjetivo(uint8_t x, uint8_t y) const;
    bool noObjetivo() const; // única condição de fim da corrida

private:
    bool dentro(int x, int y) const;
    void gravar(uint8_t x, uint8_t y, Direcao d, bool parede);
    void gravarLado(uint8_t x, uint8_t y, Direcao d, bool parede);

    // Bits 0-3: há parede (N, L, S, O). Bits 4-7: aquele lado já é conhecido.
    uint8_t paredes_[MAX_LARG][MAX_ALT];
    uint8_t largura_ = 0;
    uint8_t altura_ = 0;
    bool visitada_[MAX_LARG][MAX_ALT];
    uint8_t x_ = 0;
    uint8_t y_ = 0;
    Direcao orientacao_ = NORTE;
    uint8_t objetivoX_ = 0;
    uint8_t objetivoY_ = 0;
};
