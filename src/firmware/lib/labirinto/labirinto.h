#pragma once

#include <stdint.h>

#include "direcao.h"

const uint8_t MAX_LARG = 16;
const uint8_t MAX_ALT = 16;

// Mapa do labirinto em memória fixa (RNF03). A célula (0,0) é a largada, no
// canto sudoeste; x cresce para Leste e y para Norte (docs/4.4.7).
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

private:
    bool dentro(int x, int y) const;
    void gravar(uint8_t x, uint8_t y, Direcao d, bool parede);
    void gravarLado(uint8_t x, uint8_t y, Direcao d, bool parede);

    // Bits 0-3: há parede (N, L, S, O). Bits 4-7: aquele lado já é conhecido.
    uint8_t paredes_[MAX_LARG][MAX_ALT];
    uint8_t largura_ = 0;
    uint8_t altura_ = 0;
};
