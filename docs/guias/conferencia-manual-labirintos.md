# Labirintos sinteticos para revisao manual — #164

Nao sao resultados do Flood Fill.

Coordenadas: origem inferior esquerda; x para Leste, y para Norte.
Todos usam o canto oposto como objetivo, menos CORREDOR_RETO: ele e um teste
auxiliar com objetivo (3,0), nao uma alteracao do destino oficial do projeto.
DUAS_ROTAS serve para comparar alternativas; todas as celulas dele sao
alcancaveis. So o CORREDOR_RETO tem celulas isoladas intencionais (12 das 16).
SAIDA_DIREITA cobre o segundo caso do CT-SE09. As ARMADILHAS escondem um beco
no caminho que parece mais curto, para obrigar o robo a revisitar celulas
(CT-SE11).

## MISTO_4X4

Largada (0,0); objetivo (3,3).

```text
+---+---+---+---+
|               |
+   +   +   +   +
|       |       |
+   +---+   +   +
|           |   |
+   +   +   +   +
|   |           |
+---+---+---+---+
```

- Rota minima conferida por pessoa (lista de celulas): (0,0) -> (0,1) -> (0,2) -> (0,3) -> (1,3) -> (2,3) -> (3,3)
- Quantidade de movimentos: 6
- Justificativa de que nao ha rota menor: distância pelas casas |3 - 0| + |3 - 0| = 6; a rota tem 6 movimentos, então é mínima.
- Responsavel e data da conferencia: Letícia Hladczuk, 18/10/2026 às 16h.

## BARREIRAS_8X4

Largada (0,0); objetivo (7,3).

```text
+---+---+---+---+---+---+---+---+
|               |               |
+   +   +   +   +   +   +   +   +
|       |       |       |       |
+   +   +   +   +   +   +   +   +
|       |       |       |       |
+   +   +   +   +   +   +   +   +
|       |               |       |
+---+---+---+---+---+---+---+---+
```

- Rota minima conferida por pessoa (lista de celulas): (0,0) -> (0,1) -> (0,2) -> (0,3) -> (1,3) -> (2,3) -> (3,3) -> (3,2) -> (3,1) -> (3,0) -> (4,0) -> (5,0) -> (5,1) -> (5,2) -> (5,3) -> (6,3) -> (7,3)
- Quantidade de movimentos: 16.
- Justificativa de que nao ha rota menor: As únicas passagens estão em (1,3) -> (2,3), (3,0) -> (4,0) e (5,3) -> (6,3). Toda rota precisa ir da largada para (1,3) (4 mov), atravessar a coluna (1 mov) e ir de (2,3) até (3,0) (4 mov), atravessar a próxima coluna (1 mov) e ir de (4,0) até (5,3) (4 mov), atravessar a próxima coluna (1 mov) e chegar no objetivo (1 mov). Total: 4 + 1 + 4 + 1 + 4 + 1 + 1 = 16 no mínimo.
- Responsavel e data da conferencia: Letícia Hladczuk, 18/10/2026 às 16h.

## BARREIRAS_12X4

Largada (0,0); objetivo (11,3).

```text
+---+---+---+---+---+---+---+---+---+---+---+---+
|               |               |               |
+   +   +   +   +   +   +   +   +   +   +   +   +
|       |       |       |       |       |       |
+   +   +   +   +   +   +   +   +   +   +   +   +
|       |       |       |       |       |       |
+   +   +   +   +   +   +   +   +   +   +   +   +
|       |               |               |       |
+---+---+---+---+---+---+---+---+---+---+---+---+
```

- Rota minima conferida por pessoa (lista de celulas): (0,0) -> (0,1) -> (0,2) -> (0,3) -> (1,3) -> (2,3) -> (3,3) -> (3,2) -> (3,1) -> (3,0) -> (4,0) -> (5,0) -> (5,1) -> (5,2) -> (5,3) -> (6,3) -> (7,3) -> (7,2) -> (7,1) -> (7,0) -> (8,0) -> (9,0) -> (9,1) -> (9,2) -> (9,3) -> (10,3) -> (11,3)
- Quantidade de movimentos: 26.
- Justificativa de que nao ha rota menor: As únicas passagens estão em (1,3) -> (2,3), (3,0) -> (4,0), (5,3) -> (6,3), (7,0) -> (8,0) e (9,3) -> (10,3). Toda rota precisa ir da largada para (1,3) (4 mov), atravessar a coluna (1 mov), ir de (2,3) até (3,0) (4 mov), atravessar a próxima coluna (1 mov), ir de (4,0) até (5,3) (4 mov), atravessar a próxima coluna (1 mov), ir de (6,3) até (7,0) (4 mov), atravessar a próxima coluna (1 mov), ir de (8,0) até (9,3) (4 mov), atravessar a próxima coluna (1 mov) e chegar no objetivo (1 mov). Total: 4 + 1 + 4 + 1 + 4 + 1 + 4 + 1 + 4 + 1 + 1 = 26 no mínimo.
- Responsavel e data da conferencia: Letícia Hladczuk, 18/10/2026 às 16h.

## CORREDOR_RETO

Largada (0,0); objetivo (3,0).

```text
+---+---+---+---+
|   |   |   |   |
+---+---+---+---+
|   |   |   |   |
+---+---+---+---+
|   |   |   |   |
+---+---+---+---+
|               |
+---+---+---+---+
```

- Rota minima conferida por pessoa (lista de celulas): (0,0) -> (1,0) -> (2,0) -> (3,0).
- Quantidade de movimentos: 3.
- Justificativa de que nao ha rota menor: distância pelas casas |3 - 0| + |0 - 0| = 3; a rota tem 3 movimentos, então é mínima.
- Responsavel e data da conferencia: Letícia Hladczuk, 18/10/2026 às 16h.

## BECO_LARGADA

Largada (0,0); objetivo (3,3).

```text
+---+---+---+---+
|               |
+   +   +   +   +
|               |
+   +   +   +   +
|               |
+   +   +   +   +
|   |           |
+---+---+---+---+
```

- Rota minima conferida por pessoa (lista de celulas): (0,0) -> (0,1) -> (0,2) -> (0,3) -> (1,3) -> (2,3) -> (3,3).
- Quantidade de movimentos: 6.
- Justificativa de que nao ha rota menor: distância pelas casas |3 - 0| + |3 - 0| = 6; a rota tem 6 movimentos, então é mínima.
- Responsavel e data da conferencia: Letícia Hladczuk, 18/10/2026 às 16h.

## DUAS_ROTAS

Largada (0,0); objetivo (3,3).

```text
+---+---+---+---+
|               |
+   +---+---+   +
|   |           |
+   +   +---+---+
|   |           |
+   +---+---+   +
|               |
+---+---+---+---+
```

- Rota minima conferida por pessoa (lista de celulas): (0,0) -> (0,1) -> (0,2) -> (0,3) -> (1,3) -> (2,3) -> (3,3).
- Quantidade de movimentos: 6.
- Justificativa de que nao ha rota menor: distância pelas casas |3 - 0| + |3 - 0| = 6; a rota tem 6 movimentos, então é mínima.
- Responsavel e data da conferencia: Letícia Hladczuk, 18/10/2026 às 16h.

## SAIDA_DIREITA

Largada (0,0); objetivo (3,3).

```text
+---+---+---+---+
|               |
+   +   +   +   +
|               |
+   +   +   +   +
|               |
+---+   +   +   +
|               |
+---+---+---+---+
```

- Rota minima conferida por pessoa (lista de celulas): (0,0) -> (1,0) -> (2,0) -> (3,0) -> (3,1) -> (3,2) -> (3,3).
- Quantidade de movimentos: 6.
- Justificativa de que nao ha rota menor: distância pelas casas |3 - 0| + |3 - 0| = 6; a rota tem 6 movimentos, então é mínima.
- Responsavel e data da conferencia: Letícia Hladczuk, 18/10/2026 às 16h.

## ARMADILHA_4X4

Largada (0,0); objetivo (3,3).

```text
+---+---+---+---+
|   |           |
+   +   +   +   +
|   |           |
+   +   +   +   +
|   |           |
+   +   +   +   +
|               |
+---+---+---+---+
```

- Rota minima conferida por pessoa (lista de celulas): (0,0) -> (1,0) -> (2,0) -> (3,0) -> (3,1) -> (3,2) -> (3,3).
- Quantidade de movimentos: 6.
- Justificativa de que nao ha rota menor: distância pelas casas |3 - 0| + |3 - 0| = 6; a rota tem 6 movimentos, então é mínima.
- Responsavel e data da conferencia: Letícia Hladczuk, 18/10/2026 às 16h.

## ARMADILHA_8X4

Largada (0,0); objetivo (7,3).

```text
+---+---+---+---+---+---+---+---+
|                           |   |
+   +---+---+---+---+---+---+   +
|                               |
+   +   +   +   +   +   +   +   +
|                               |
+   +   +   +   +   +   +   +   +
|                               |
+---+---+---+---+---+---+---+---+
```

- Rota minima conferida por pessoa (lista de celulas): (0,0) -> (1,0) -> (2,0) -> (3,0) -> (4,0) -> (5,0) -> (6,0) -> (7,0) -> (7,1) -> (7,2) -> (7,3).
- Quantidade de movimentos: 10.
- Justificativa de que nao ha rota menor: distância pelas casas |7 - 0| + |3 - 0| = 10; a rota tem 10 movimentos, então é mínima.
- Responsavel e data da conferencia: Letícia Hladczuk, 18/10/2026 às 16h.

## ARMADILHA_12X4

Largada (0,0); objetivo (11,3).

```text
+---+---+---+---+---+---+---+---+---+---+---+---+
|                                           |   |
+   +---+---+---+---+---+---+---+---+---+---+   +
|                                           |   |
+   +   +---+---+---+---+---+---+---+---+---+   +
|                                               |
+   +   +   +   +   +   +   +   +   +   +   +   +
|                                               |
+---+---+---+---+---+---+---+---+---+---+---+---+
```

- Rota minima conferida por pessoa (lista de celulas): (0,0) -> (1,0) -> (2,0) -> (3,0) -> (4,0) -> (5,0) -> (6,0) -> (7,0) -> (8,0) -> (9,0) -> (10,0) -> (11,0) -> (11,1) -> (11,2) -> (11,3).
- Quantidade de movimentos: 14.
- Justificativa de que nao ha rota menor: distância pelas casas |11 - 0| + |3 - 0| = 14; a rota tem 14 movimentos, então é mínima.
- Responsavel e data da conferencia: Letícia Hladczuk, 18/10/2026 às 16h.
