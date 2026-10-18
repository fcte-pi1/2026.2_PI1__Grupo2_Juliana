#pragma once

#include <stdint.h>

// Fixtures sinteticas para revisao da dupla; nao sao os labirintos oficiais.
// (0,0) fica embaixo a esquerda. Objetivo declarado em cada fixture.
struct LabirintoTeste {
    const char* nome;
    uint8_t largura, altura, objetivoX, objetivoY;
    const char* ascii;
};

static const char MISTO_4X4_ASCII[] = R"MAZE(+---+---+---+---+
|               |
+   +   +   +   +
|       |       |
+   +---+   +   +
|           |   |
+   +   +   +   +
|   |           |
+---+---+---+---+
)MAZE";

static const char BARREIRAS_8X4_ASCII[] = R"MAZE(+---+---+---+---+---+---+---+---+
|               |               |
+   +   +   +   +   +   +   +   +
|       |       |       |       |
+   +   +   +   +   +   +   +   +
|       |       |       |       |
+   +   +   +   +   +   +   +   +
|       |               |       |
+---+---+---+---+---+---+---+---+
)MAZE";

static const char BARREIRAS_12X4_ASCII[] = R"MAZE(+---+---+---+---+---+---+---+---+---+---+---+---+
|               |               |               |
+   +   +   +   +   +   +   +   +   +   +   +   +
|       |       |       |       |       |       |
+   +   +   +   +   +   +   +   +   +   +   +   +
|       |       |       |       |       |       |
+   +   +   +   +   +   +   +   +   +   +   +   +
|       |               |               |       |
+---+---+---+---+---+---+---+---+---+---+---+---+
)MAZE";

static const char CORREDOR_RETO_ASCII[] = R"MAZE(+---+---+---+---+
|   |   |   |   |
+---+---+---+---+
|   |   |   |   |
+---+---+---+---+
|   |   |   |   |
+---+---+---+---+
|               |
+---+---+---+---+
)MAZE";

static const char BECO_LARGADA_ASCII[] = R"MAZE(+---+---+---+---+
|               |
+   +   +   +   +
|               |
+   +   +   +   +
|               |
+   +   +   +   +
|   |           |
+---+---+---+---+
)MAZE";

static const char DUAS_ROTAS_ASCII[] = R"MAZE(+---+---+---+---+
|               |
+   +---+---+   +
|   |           |
+   +   +---+---+
|   |           |
+   +---+---+   +
|               |
+---+---+---+---+
)MAZE";

static const LabirintoTeste LABIRINTOS_TESTE[] = {
    {"MISTO_4X4", 4, 4, 3, 3, MISTO_4X4_ASCII},
    {"BARREIRAS_8X4", 8, 4, 7, 3, BARREIRAS_8X4_ASCII},
    {"BARREIRAS_12X4", 12, 4, 11, 3, BARREIRAS_12X4_ASCII},
    {"CORREDOR_RETO", 4, 4, 3, 0, CORREDOR_RETO_ASCII},
    {"BECO_LARGADA", 4, 4, 3, 3, BECO_LARGADA_ASCII},
    {"DUAS_ROTAS", 4, 4, 3, 3, DUAS_ROTAS_ASCII},
};
static const unsigned QUANTIDADE_LABIRINTOS_TESTE = sizeof(LABIRINTOS_TESTE) / sizeof(LABIRINTOS_TESTE[0]);
