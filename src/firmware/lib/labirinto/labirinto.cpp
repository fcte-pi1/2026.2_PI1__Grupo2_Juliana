#include "labirinto.h"

#include <string.h>

void Labirinto::iniciar(uint8_t largura, uint8_t altura)
{
    largura_ = (largura == 0 || largura > MAX_LARG) ? MAX_LARG : largura;
    altura_ = (altura == 0 || altura > MAX_ALT) ? MAX_ALT : altura;
    memset(paredes_, 0, sizeof(paredes_));

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

bool Labirinto::dentro(int x, int y) const
{
    return x >= 0 && x < largura_ && y >= 0 && y < altura_;
}

// Uma parede separa duas células: grava na célula e, do lado oposto, na vizinha.
// As bordas já nascem fechadas em iniciar() e não podem ser abertas.
void Labirinto::gravar(uint8_t x, uint8_t y, Direcao d, bool parede)
{
    int vx = x + dx(d);
    int vy = y + dy(d);
    if (!dentro(x, y) || !dentro(vx, vy))
        return;
    gravarLado(x, y, d, parede);
    gravarLado(vx, vy, oposta(d), parede);
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
