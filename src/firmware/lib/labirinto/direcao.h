#pragma once

#include <stdint.h>

// Direções absolutas do labirinto. Os valores são os bits de paredes do
// Contrato de Telemetria (docs/4.4.7): Norte=1, Leste=2, Sul=4, Oeste=8.
enum Direcao : uint8_t { NORTE = 1, LESTE = 2, SUL = 4, OESTE = 8 };

// Sentido horário: N -> L -> S -> O -> N.
inline Direcao girarDireita(Direcao d) {
  return d == OESTE ? NORTE : (Direcao)(d << 1);
}

// Sentido anti-horário: N -> O -> S -> L -> N.
inline Direcao girarEsquerda(Direcao d) {
  return d == NORTE ? OESTE : (Direcao)(d >> 1);
}

inline Direcao oposta(Direcao d) { return girarDireita(girarDireita(d)); }

// Deslocamento ao andar uma célula: x cresce para Leste, y cresce para Norte.
inline int8_t deslocamentoX(Direcao d) { return d == LESTE ? 1 : (d == OESTE ? -1 : 0); }
inline int8_t deslocamentoY(Direcao d) { return d == NORTE ? 1 : (d == SUL ? -1 : 0); }

inline char letra(Direcao d) {
  switch (d) {
    case NORTE: return 'N';
    case LESTE: return 'L';
    case SUL:   return 'S';
    default:    return 'O';
  }
}
