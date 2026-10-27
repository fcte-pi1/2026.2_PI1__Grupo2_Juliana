#pragma once

#include <stdint.h>

#include "direcao.h"

const uint8_t MAX_LARG = 16;
const uint8_t MAX_ALT = 16;

// De que lado do robô fica a borda do labirinto na largada. A equipe informa.
enum LadoBorda : uint8_t
{
    BORDA_ESQUERDA,
    BORDA_DIREITA
};

// Mapa do labirinto em memória fixa (RNF03).
//
// Convenção de largada (Tema PI1, slide 9): o robô começa num beco em
// qualquer canto, com a borda atrás dele, e o objetivo é o canto
// diagonalmente oposto. A largada é sempre (0,0), no canto Sudoeste do mapa;
// x cresce para Leste e y para Norte. O que muda com o canto é para onde o
// robô aponta:
//   - borda atrás e à esquerda: larga virado para o Norte;
//   - borda atrás e à direita: larga virado para o Leste.
// Assim a borda de trás e a lateral são sempre o Sul e o Oeste de (0,0), e o
// objetivo fica em (largura-1, altura-1), como no frontend e na telemetria.
// Se a frente estiver fechada, a saída do beco fica do lado oposto à borda.
// Todas as bordas são fechadas.
class Labirinto
{
public:
    // Zera tudo e fecha as bordas. Só aceita os labirintos do TAP (4x4, 8x4 e
    // 12x4, ou 4x8 e 4x12 se girados em relação ao robô) e orientação inicial
    // NORTE ou LESTE; senão devolve false e não altera nada.
    bool iniciar(uint8_t largura, uint8_t altura, Direcao orientacaoInicial = NORTE);
    // O mesmo, a partir do que a equipe informa olhando o robô na largada:
    // o lado da borda e quantas células há à frente e ao lado dele.
    bool iniciarNaLargada(LadoBorda borda, uint8_t celulasAFrente, uint8_t celulasAoLado);

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
    Direcao orientacaoInicial() const { return orientacaoInicial_; }
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
    Direcao orientacaoInicial_ = NORTE;
    uint8_t objetivoX_ = 0;
    uint8_t objetivoY_ = 0;
};
