#pragma once

#include <stdint.h>

#include "direcao.h"

const uint8_t MAX_LARG = 16;
const uint8_t MAX_ALT = 16;

// Mapa do labirinto em memória fixa (RNF03).
//
// Convenção de largada (Tema PI1, slide 9: o robô começa num beco sem saída
// em um canto e o objetivo é o canto oposto). A equipe posiciona o robô na
// célula de largada com a borda do labirinto atrás e à esquerda dele. As
// direções valem a partir dessa pose:
//   - Norte é para onde o robô aponta na largada e Leste é a direita dele;
//   - a largada é (0,0); x cresce para Leste e y para Norte;
//   - largura = células à direita do robô, altura = células à frente.
// Se a saída do beco não estiver à frente, está à direita: o robô larga de
// frente para a parede interna e gira para sair.
class Labirinto
{
public:
    // Zera tudo e fecha as bordas. Só aceita os labirintos do TAP (4x4, 8x4 e
    // 12x4, ou 4x8 e 4x12 se girados em relação ao robô); com outro tamanho
    // devolve false e não altera nada.
    bool iniciar(uint8_t largura, uint8_t altura);
    void marcarParede(uint8_t x, uint8_t y, Direcao d); // grava nos dois lados
    void marcarAberta(uint8_t x, uint8_t y, Direcao d); // lado conhecido e sem parede
    bool temParede(uint8_t x, uint8_t y, Direcao d) const;
    bool conhecida(uint8_t x, uint8_t y, Direcao d) const;
    uint8_t paredesDe(uint8_t x, uint8_t y) const; // máscara 0..15 da telemetria
    uint8_t largura() const { return largura_; }
    uint8_t altura() const { return altura_; }

    // Pose do robô. A navegação consulta podeAvancar() antes de acionar o
    // motor; depois que o robô andou de fato, chama avancar() para registrar.
    // avancar() não consulta as paredes: o robô real é quem manda, então uma
    // parede falsa no mapa não pode deixar a pose para trás.
    bool podeAvancar() const; // false se houver parede conhecida na frente
    bool avancar();           // registra uma célula andada; false se sairia do labirinto
    void girar(Direcao nova);
    uint8_t x() const { return x_; }
    uint8_t y() const { return y_; }
    Direcao orientacao() const { return orientacao_; }
    void marcarVisitada(uint8_t x, uint8_t y);
    bool visitada(uint8_t x, uint8_t y) const;

    // Objetivo: a célula do canto diagonalmente oposto à largada (slide 9),
    // definida por iniciar() em (largura-1, altura-1). definirObjetivo() troca a célula.
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
