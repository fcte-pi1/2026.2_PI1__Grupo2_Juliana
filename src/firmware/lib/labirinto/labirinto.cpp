#include "labirinto.h"

#include <string.h>

// Um lado tem 4 células e o outro 4, 8 ou 12.
static bool tamanhoDoRoteiro(uint8_t largura, uint8_t altura)
{
    uint8_t menor = largura < altura ? largura : altura;
    uint8_t maior = largura < altura ? altura : largura;
    return menor == 4 && (maior == 4 || maior == 8 || maior == 12);
}

bool Labirinto::iniciar(uint8_t largura, uint8_t altura)
{
    if (!tamanhoDoRoteiro(largura, altura))
        return false;
    largura_ = largura;
    altura_ = altura;
    memset(paredes_, 0, sizeof(paredes_));
    memset(visitada_, 0, sizeof(visitada_));

    for (uint8_t x = 0; x < largura_; x++)
    {
        gravarLado(x, 0, SUL, true);
        gravarLado(x, altura_ - 1, NORTE, true);
    }
    for (uint8_t y = 0; y < altura_; y++)
    {
        gravarLado(0, y, OESTE, true);
        gravarLado(largura_ - 1, y, LESTE, true);
    }

    x_ = 0;
    y_ = 0;
    orientacao_ = NORTE;
    marcarVisitada(0, 0);
    objetivoX_ = largura_ - 1;
    objetivoY_ = altura_ - 1;
    return true;
}

void Labirinto::marcarParede(uint8_t x, uint8_t y, Direcao d) { gravar(x, y, d, true); }

void Labirinto::marcarAberta(uint8_t x, uint8_t y, Direcao d) { gravar(x, y, d, false); }

bool Labirinto::temParede(uint8_t x, uint8_t y, Direcao d) const
{
    if (!dentro(x, y))
        return true;
    return (paredes_[x][y] & d) != 0;
}

bool Labirinto::conhecida(uint8_t x, uint8_t y, Direcao d) const
{
    if (!dentro(x, y))
        return true;
    return (paredes_[x][y] & (d << 4)) != 0;
}

uint8_t Labirinto::paredesDe(uint8_t x, uint8_t y) const
{
    if (!dentro(x, y))
        return 0;
    return paredes_[x][y] & 0x0F;
}

bool Labirinto::podeAvancar() const { return !temParede(x_, y_, orientacao_); }

// Sair do labirinto é fisicamente impossível: se acontecer, a pose já perdeu a
// sincronia com o robô, e quem chama precisa tratar.
bool Labirinto::avancar()
{
    int proximoX = x_ + deslocamentoX(orientacao_);
    int proximoY = y_ + deslocamentoY(orientacao_);
    if (!dentro(proximoX, proximoY))
        return false;
    x_ = proximoX;
    y_ = proximoY;
    marcarVisitada(x_, y_);
    return true;
}

void Labirinto::girar(Direcao nova) { orientacao_ = nova; }

void Labirinto::marcarVisitada(uint8_t x, uint8_t y)
{
    if (dentro(x, y))
        visitada_[x][y] = true;
}

bool Labirinto::visitada(uint8_t x, uint8_t y) const
{
    return dentro(x, y) && visitada_[x][y];
}

bool Labirinto::definirObjetivo(uint8_t x, uint8_t y)
{
    if (!dentro(x, y))
        return false;
    objetivoX_ = x;
    objetivoY_ = y;
    return true;
}

bool Labirinto::ehObjetivo(uint8_t x, uint8_t y) const
{
    return x == objetivoX_ && y == objetivoY_;
}

bool Labirinto::noObjetivo() const { return ehObjetivo(x_, y_); }

bool Labirinto::dentro(int x, int y) const
{
    return x >= 0 && x < largura_ && y >= 0 && y < altura_;
}

// Uma parede separa duas células: grava na célula e, do lado oposto, na vizinha.
// As bordas já nascem fechadas em iniciar() e não podem ser abertas.
void Labirinto::gravar(uint8_t x, uint8_t y, Direcao d, bool parede)
{
    int vizinhoX = x + deslocamentoX(d);
    int vizinhoY = y + deslocamentoY(d);
    if (!dentro(x, y) || !dentro(vizinhoX, vizinhoY))
        return;
    gravarLado(x, y, d, parede);
    gravarLado(vizinhoX, vizinhoY, oposta(d), parede);
}

void Labirinto::gravarLado(uint8_t x, uint8_t y, Direcao d, bool parede)
{
    uint8_t &celula = paredes_[x][y];
    celula |= (uint8_t)(d << 4);
    if (parede)
        celula |= d;
    else
        celula &= (uint8_t)~d;
}
