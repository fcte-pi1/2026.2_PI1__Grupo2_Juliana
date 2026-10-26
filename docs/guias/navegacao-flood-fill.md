# Navegação do Micromouse: Flood Fill e menor caminho

Explicação do código da issue **#164 (3.7.2)**: como o robô decide para onde ir enquanto explora o labirinto e como ele calcula a rota mais curta para a corrida rápida. No fim você vai saber:

1. o que cada arquivo faz e por que ele existe;
2. como o Flood Fill funciona, com um exemplo feito à mão;
3. o que cada teste confere e qual requisito ou caso de teste ele cobre.

> O Flood Fill usa o mapa da issue **#163** (`lib/labirinto/`). A seção 2 relembra só o necessário dele.
> Para preparar o computador e rodar os testes, veja o [guia de configuração do firmware](configuracao-firmware.md).

---

## Sumário

1. [Visão geral](#1-visão-geral)
2. [Relembrando o mapa (#163)](#2-relembrando-o-mapa-163)
3. [A ideia do Flood Fill](#3-a-ideia-do-flood-fill)
4. [`floodfill.h`: o que o módulo oferece](#4-floodfillh-o-que-o-módulo-oferece)
5. [`floodfill.cpp`: como cada função funciona](#5-floodfillcpp-como-cada-função-funciona)
6. [`carregar`: labirinto desenhado em texto](#6-carregar-labirinto-desenhado-em-texto)
7. [`robo_simulado.h`: o robô de mentira](#7-robo_simuladoh-o-robô-de-mentira)
8. [Os testes](#8-os-testes)
9. [Como rodar](#9-como-rodar)
10. [Perguntas que podem aparecer na revisão](#10-perguntas-que-podem-aparecer-na-revisão)
11. [Largada em qualquer canto](#11-largada-em-qualquer-canto)

---

## 1. Visão geral

| Arquivo | Onde | Para que serve |
|---|---|---|
| `floodfill.h` | `src/firmware/lib/navegacao/` | A "vitrine": lista as funções que outros arquivos podem chamar. É usado nos testes de aceitação. |
| `floodfill.cpp` | `src/firmware/lib/navegacao/` | O código das funções: cálculo das distâncias, escolha da direção, rota mais curta e leitura do desenho. |
| `robo_simulado.h` | `src/firmware/test/` (raiz, compartilhado pelos testes) | Um robô de teste que anda num labirinto conhecido, enxergando só o que os sensores reais enxergariam. |
| `desenhos_labirintos.h` | `src/firmware/test/` (raiz, compartilhado pelos testes) | Os 10 labirintos de teste desenhados em texto, cada um com objetivo e tamanho da rota mínima conferida à mão. |
| `test_floodfill.cpp` | `src/firmware/test/test_floodfill/` | Os testes unitários (testes de uma peça isolada do código). |
| `test_navegacao.cpp` | `src/firmware/test/test_navegacao/` | Os testes de aceitação do CT-SE09, CT-SE11 e CT-SE14, por simulação. |
| `conferencia-manual-labirintos.md` | `docs/guias/` | A rota mínima de cada labirinto de teste, achada e justificada por uma pessoa. |

Como tudo está em `lib/` e não usa `Arduino.h`, o código **roda igual no computador e no ESP32**. Por isso dá para testar antes da placa chegar.

O caminho de uma decisão do robô, do sensor até o motor:

```
sensores ──► #211 (filtro e paredes) ──► Labirinto (#163) ──► FloodFill (#164) ──► direção ──► motores (#214)
                                         marca a parede         recalcula as
                                         nova no mapa           distâncias
```

---

## 2. Relembrando o mapa (#163)

O Flood Fill não guarda paredes. Ele **pergunta ao `Labirinto`**, que guarda para cada célula um número de 8 bits (um `uint8_t`):

```
bit:     7   6   5   4  |  3   2   1   0
         O   S   L   N  |  O   S   L   N
         └─ "conhecida" ┘  └── "parede" ┘
```

- **Bits 0–3:** há parede naquele lado (Norte=1, Leste=2, Sul=4, Oeste=8, como no contrato de telemetria).
- **Bits 4–7:** o robô já **viu** aquele lado (tem certeza se há parede ou não).

Isso cria três situações para cada lado de uma célula:

| Situação | `temParede` | `conhecida` | Exemplo |
|---|---|---|---|
| Parede vista | `true` | `true` | o sensor viu uma parede |
| Aberto visto | `false` | `true` | o sensor viu que não há parede, ou o robô passou por ali |
| Desconhecido | `false` | `false` | o robô ainda não olhou |

O Flood Fill depende da terceira linha: **o que fazer com o desconhecido** é a diferença entre explorar e correr (seção 3).

Outras coisas do `Labirinto` que o Flood Fill usa:

| Função | O que devolve |
|---|---|
| `largura()`, `altura()` | tamanho do labirinto (ex.: 12 e 4) |
| `ehObjetivo(x, y)` | se aquela célula é o objetivo |
| `x()`, `y()`, `orientacao()` | onde o robô está e para onde está virado |

E do `direcao.h`: `girarDireita`, `girarEsquerda`, `oposta`, e `deslocamentoX`/`deslocamentoY`, que dizem quanto `x` e `y` mudam ao andar uma célula naquela direção (Leste: `deslocamentoX = +1`; Sul: `deslocamentoY = -1`).

Da revisão do PR #242 vieram mais três regras que o Flood Fill segue:

| Regra | O que muda para a navegação |
|---|---|
| `iniciar()` só aceita 4x4, 8x4, 12x4, 4x8 e 4x12 | o maior labirinto tem 48 células |
| **Convenção de largada:** o robô começa em (0,0) em qualquer canto, com a borda atrás. Borda à esquerda: ele larga virado para o Norte; borda à direita: virado para o Leste (seção 11) | se a frente da largada estiver fechada, a saída do beco é o lado **oposto à borda** |
| `podeAvancar()` antes de acionar o motor; `avancar()` depois que o robô andou de fato | `avancar()` não consulta paredes. Se devolver `false`, a pose perdeu a sincronia com o robô real: parar e sinalizar o erro |

---

## 3. A ideia do Flood Fill

"Flood fill" quer dizer **preenchimento por inundação**. Imagine jogar água na célula do objetivo: ela se espalha para as células vizinhas, depois para as vizinhas das vizinhas, e assim por diante, sem passar por paredes. Cada célula recebe um número: **quantos passos a água levou para chegar até ela**. Esse número é a distância até o objetivo.

O robô então só precisa de uma regra: **ir sempre para a vizinha com o número menor**. Como os números diminuem de 1 em 1 até o 0 do objetivo, seguir essa regra leva ao objetivo pelo caminho mais curto conhecido.

### Exemplo: labirinto 4x4 sem paredes internas

Objetivo no canto oposto à largada, a célula (3,3). A largada é (0,0), embaixo à esquerda:

```
        x=0 x=1 x=2 x=3
  y=3 │  3   2   1   0  │  ← objetivo
  y=2 │  4   3   2   1  │
  y=1 │  5   4   3   2  │
  y=0 │  6   5   4   3  │
        ↑ largada
```

Sem paredes, a distância é só "quantas células para a direita + quantas para cima", que se chama **distância de Manhattan**. O primeiro teste confere exatamente isso.

### O mesmo labirinto com uma parede nova

O robô descobre uma parede entre (3,2) e (3,3). A água não passa mais por ali e precisa contornar:

```
        x=0 x=1 x=2 x=3
  y=3 │  3   2   1   0  │
                     ═══   ← parede nova
  y=2 │  4   3   2   3  │  ← (3,2) passou de 1 para 3
  y=1 │  5   4   3   4  │
  y=0 │  6   5   4   5  │
```

Isso é o **recálculo a cada parede nova** (RF11): sempre que o mapa muda, os números são refeitos e o robô continua seguindo "o menor vizinho".

### Explorar × correr: o que fazer com o desconhecido

| Modo | Lado desconhecido conta como | Por quê |
|---|---|---|
| `EXPLORACAO` | **aberto** | Otimista: se pode haver um atalho ali, o robô vai até lá conferir. Se houver parede, ele descobre, recalcula e muda de rumo. |
| `RAPIDO` | **fechado** | Cauteloso: na corrida valendo, o robô só passa por onde **tem certeza** de que não há parede. |

---

## 4. `floodfill.h`: o que o módulo oferece

```cpp
enum ModoFlood : uint8_t { EXPLORACAO, RAPIDO };
const uint8_t INFINITO = 255;
```

- `ModoFlood`: os dois modos da tabela acima.
- `INFINITO`: valor que marca "esta célula não chega ao objetivo". É 255, o maior número que cabe em 1 byte (`uint8_t`). Pode ser 255 porque o maior labirinto do roteiro tem 48 células, então nenhuma distância real passa de 47 e nunca se confunde com "infinito".

A classe `FloodFill`:

| Função | O que faz |
|---|---|
| `recalcular(lab, modo)` | Refaz todas as distâncias olhando as paredes atuais do mapa. |
| `distancia(x, y)` | Distância de uma célula. `INFINITO` se não houver caminho ou se a célula estiver fora do mapa. |
| `valorAtual(lab)` | Distância da célula onde o robô está. Vai para a telemetria no campo `valor_flood`. |
| `proximaDirecao(lab, saida)` | Para onde o robô deve ir agora. Grava a resposta em `saida` e devolve `false` se não houver caminho. |
| `rotaMaisCurta(lab, saida, max)` | Lista de direções da largada até o objetivo, só por lados conhecidos. Devolve o número de passos, ou 0. |
| `rotaGarantida(lab)` | Se a rota rápida já é a menor possível, mesmo com lados ainda desconhecidos (seção 5). |

E duas funções soltas: `carregar(ascii, gabarito)`, que monta um `Labirinto` a partir de um desenho em texto, e `imprimirMapa(lab, ff, buf, tamanho)`, que faz o caminho contrário e desenha o mapa com as distâncias (seção 6).

**Por que uma classe e não funções soltas?** As distâncias precisam ficar guardadas em algum lugar entre um `recalcular` e o `proximaDirecao` seguinte. A classe guarda isso em `dist_[16][16]`: 256 células × 1 byte = **256 bytes fixos**, sem `new` nem `vector` (RNF03, memória fixa). A matriz é 16x16, maior que o necessário, para usar os mesmos limites `MAX_LARG` e `MAX_ALT` do `Labirinto`. Ela também guarda o `modo_` do último cálculo, para que `proximaDirecao` use a mesma regra de passagem que gerou os números.

**Por que `proximaDirecao` devolve `bool` e grava a direção num parâmetro?** Porque ela tem duas respostas: "existe direção?" e "qual?". Esse é um jeito comum em C++ de devolver duas coisas sem alocar memória.

---

## 5. `floodfill.cpp`: como cada função funciona

### `recalcular`: a inundação

```cpp
memset(dist_, INFINITO, sizeof(dist_));
```
Começa com **todas as células em INFINITO**. O `memset` enche cada byte da matriz com 255, e como cada distância ocupa exatamente 1 byte, cada célula vira 255.

```cpp
uint8_t filaX[MAX_LARG * MAX_ALT];
uint8_t filaY[MAX_LARG * MAX_ALT];
```
Uma **fila** é uma lista em que o primeiro a entrar é o primeiro a sair, como uma fila de banco. Aqui ela guarda as células que já receberam número e ainda precisam "molhar" as vizinhas. Fica em dois vetores, um para `x` e outro para `y`, com 256 posições.

Não precisa de fila circular (aquela que reaproveita o começo do vetor): **cada célula entra na fila uma vez só**, então 256 posições nunca acabam.

```cpp
if (lab.ehObjetivo(x, y)) { dist_[x][y] = 0; /* entra na fila */ }
```
O objetivo recebe 0 e é o primeiro da fila. O código procura o objetivo célula por célula porque o `Labirinto` não informa as coordenadas dele, só responde "esta é ou não é?".

```cpp
while (inicio < fim) {
    // tira a célula da frente da fila
    for (Direcao d : TODAS) {
        if (!passavel(lab, x, y, d)) continue;   // parede: a água não passa
        if (dist_[vizinhoX][vizinhoY] != INFINITO) continue; // vizinha já tem número
        dist_[vizinhoX][vizinhoY] = dist_[x][y] + 1;         // vizinha = eu + 1
        // vizinha entra no fim da fila
    }
}
```
Esse é o laço da inundação. A técnica se chama **busca em largura** (em inglês, BFS, *breadth-first search*): primeiro todas as células a 1 passo, depois todas a 2, depois a 3... Por isso, quando uma célula recebe um número, ele já é o menor possível, e nunca é preciso corrigir depois.

Passo a passo no 4x4 sem paredes:

| Rodada | Sai da fila | Recebem número |
|---|---|---|
| 1 | (3,3) = 0 | (3,2) = 1, (2,3) = 1 |
| 2 | (3,2) = 1 | (3,1) = 2, (2,2) = 2 |
| 3 | (2,3) = 1 | (1,3) = 2 (a (2,2) já tinha número) |
| 4 | (3,1) = 2 | (3,0) = 3, (2,1) = 3 |
| … | … | … até a largada (0,0) = 6 |

**É rápido?** No maior labirinto (12x4) são 48 células × 4 lados = 192 verificações. Num computador com AMD Ryzen 7 5700X: **0,0027 ms em média no 12x4** (pior caso 0,0062 ms, em 1000 recálculos), milhares de vezes abaixo dos 50 ms do RNF01. O teste `tempo_recalculo_menor_50ms` mostra esses números a cada execução com `-v` (seção 9). No ESP32 vai ser mais lento, e essa medição ainda precisa ser feita na placa.

### `passavel`: dá para passar por este lado?

```cpp
if (lab.temParede(x, y, d)) return false;
return modo_ == EXPLORACAO || lab.conhecida(x, y, d);
```
- Parede vista: nunca passa.
- Sem parede: na exploração, passa; na corrida, só passa se o lado for conhecido.

As bordas são todas paredes conhecidas, então o código nunca tenta sair do mapa e a vizinha sempre existe na matriz `dist_`.

### `escolher`: o vizinho com menor número

```cpp
const Direcao ordem[] = {frente, girarDireita(frente), girarEsquerda(frente), oposta(frente)};
...
if (distanciaVizinho < melhor) { melhor = distanciaVizinho; saida = d; achou = true; }
```
Testa os quatro vizinhos **nesta ordem: reto, direita, esquerda, trás**, e só troca a escolha se achar um número **estritamente menor** (`<`, e não `<=`). Resultado: num empate, fica o primeiro da lista. Isso é proposital, porque **seguir reto evita giros**, e giro gasta tempo e acumula erro nos encoders (sensores que contam as voltas das rodas).

O parâmetro `limite` serve às duas funções que usam `escolher`:
- `proximaDirecao` passa `INFINITO`: aceita qualquer vizinho com caminho;
- `rotaMaisCurta` passa a distância da célula atual: só aceita vizinhos **mais perto** do objetivo.

### `proximaDirecao`: para onde ir agora

```cpp
if (distancia(lab.x(), lab.y()) == INFINITO) return false;
return escolher(lab, lab.x(), lab.y(), lab.orientacao(), INFINITO, saida);
```
Se a célula atual não tem caminho até o objetivo, não há o que escolher. Senão, escolhe o menor vizinho a partir da orientação atual do robô.

**Beco sem saída (RF23, CT-SE09):** não existe código especial para ele. Num beco, frente, direita e esquerda têm parede, e o único lado passável é o de trás. `escolher` devolve `oposta(frente)`, ou seja, o giro de 180°.

**Saída da largada:** também não precisa de código especial. A largada é um beco com a borda atrás e de um dos lados (convenção do `labirinto.h`). Se a frente estiver fechada, o único lado passável é o oposto à borda (a direita, com a borda à esquerda; a esquerda, com a borda à direita), e é isso que `escolher` devolve. O Marcelo sugeriu chamar `girar(girarDireita(...))` quando `podeAvancar()` for `false`; o Flood Fill chega à mesma resposta sozinho, e o teste `largada_com_frente_fechada_sai_pela_direita` prova isso.

### `rotaMaisCurta`: a lista para a corrida rápida

```cpp
recalcular(lab, RAPIDO);
uint8_t passos = dist_[0][0];
if (passos == INFINITO || passos > max) return 0;
```
1. Recalcula **em modo RAPIDO**, só com lados conhecidos.
2. A distância da largada (0,0) já é o tamanho da rota. Se for infinita (nenhum caminho seguro) ou não couber no vetor que quem chamou entregou, devolve 0.
3. Depois "desce a escada": a partir da largada, vai sempre para o vizinho com número menor, gravando cada direção em `saida[]`, até chegar ao 0.

O robô começa virado para `lab.orientacaoInicial()` (Norte ou Leste, pela convenção de largada), e cada passo atualiza a `frente`. A rota vai até a célula do objetivo. Por isso, entre duas rotas do mesmo tamanho, sai a que **tem menos curvas**.

> **Atenção:** `rotaMaisCurta` deixa as distâncias calculadas em modo RAPIDO. Para voltar a explorar, chame `recalcular(lab, EXPLORACAO)` antes do próximo `proximaDirecao`. O robô simulado já faz isso a cada passo.

### `rotaGarantida`: já dá para correr?

A `rotaMaisCurta` só enxerga o que o robô já viu. Se ainda há lados desconhecidos, pode existir um atalho escondido. Para saber se a rota já é a menor de verdade, sem explorar o labirinto inteiro, a função compara os dois modos:

```cpp
recalcular(lab, EXPLORACAO);   // desconhecido = aberto: o melhor caso
uint8_t otimista = dist_[0][0];
recalcular(lab, RAPIDO);       // desconhecido = fechado: o que é certo
return dist_[0][0] != INFINITO && dist_[0][0] == otimista;
```

O tamanho real da menor rota fica sempre **entre** os dois números: nenhum labirinto de verdade é mais aberto que o otimista nem mais fechado que o pessimista. Se os dois são iguais, nenhum lado desconhecido mudaria a resposta, então a rota está garantida (passo 1 do CT-SE14).

O robô simulado usa isso em `explorarAteRotaGarantida()`: chega ao objetivo e, enquanto a rota não estiver garantida, vai e volta entre objetivo e largada. Na volta, a largada vira o objetivo do Flood Fill. Cada viagem que não garante a rota acha pelo menos uma parede nova, então as viagens acabam.

---

## 6. `carregar`: labirinto desenhado em texto

Para testar, é muito mais fácil **desenhar** o labirinto do que chamar `marcarParede` dezenas de vezes:

```
+---+---+---+---+
|           |   |      ← y = 3 (a primeira linha do desenho é o Norte)
+---+---+   +   +
|       |       |      ← y = 2
+   +   +---+   +
|   |           |      ← y = 1
+   +---+---+   +
|               |      ← y = 0 (largada à esquerda)
+---+---+---+---+
```

Regras do desenho:
- cada célula ocupa **4 colunas** (`+---` ou `|   `) e **2 linhas** (uma de "teto" e uma de "miolo");
- por isso um labirinto de largura L tem `4·L + 1` colunas e um de altura A tem `2·A + 1` linhas;
- `|` é parede vertical, `-` é parede horizontal, espaço é aberto;
- `carregar(desenho, lab, LESTE)` lê o mesmo desenho com o robô largando virado para o Leste (borda à direita).

Como a função lê o desenho:

1. **Separa as linhas** pelo `\n`, ignora o `\r` no fim (ele aparece em arquivos salvos no Windows) e ignora linhas em branco **no começo e no fim**. Linha em branco no meio do desenho é erro.
2. **Confere o formato:** número ímpar de linhas, todas do mesmo tamanho, `(tamanho − 1)` divisível por 4, e um tamanho que o `iniciar()` aceite (4x4, 8x4, 12x4, 4x8 ou 4x12). Espaços no fim da linha são sobra do editor e são cortados. Se algo estiver errado, devolve `false`. Assim um desenho torto não vira um labirinto errado sem ninguém perceber.
3. **Lê as paredes.** Para a célula `x` de uma linha:
   - parede Leste: o caractere na coluna `4·x + 4` é `|`?
   - parede Norte: na linha de cima, o caractere na coluna `4·x + 2` (o meio do `---`) é `-`?
4. **Marca tudo como conhecido**, com `marcarParede` ou `marcarAberta`. O gabarito é o labirinto de verdade, não há nada desconhecido nele.

Só é preciso ler Leste e Norte de cada célula. O Oeste de uma célula é o Leste da vizinha, e o `Labirinto` grava os dois lados de uma vez. A borda não é lida: o `iniciar` já a fecha.

### `imprimirMapa`: o caminho de volta, com as distâncias

Desenha o mapa que o robô tem no mesmo formato, com a distância do Flood Fill no meio de cada célula. Serve de evidência no CT-SE11 e para depurar. Num 4x4 que o robô ainda não explorou, fica assim:

```
+---+---+---+ . +
| 3 . 2 . 1 . 0 .
+ . + . + . + . +
| 4 . 3 . 2 . 1 |
+ . + . + . + . +
| 5 . 4 . 3 . 2 |
+ . + . + . + . +
| 6 . 5 . 4 . 3 |
+---+---+---+---+
```

| No desenho | Significa |
|---|---|
| `\|` ou `---` | parede conhecida |
| espaço | lado conhecido e aberto |
| `.` | lado que o robô ainda não viu |
| número | passos até o objetivo |
| `-` no lugar do número | célula sem caminho até o objetivo |

Como as paredes ficam nas mesmas colunas usadas pelo `carregar`, um mapa todo conhecido pode ser impresso e lido de volta. O teste `imprimir_mapa_pode_ser_lido_de_volta` confere isso.

A função escreve num vetor de `char` que quem chama entrega (`buf`), sem alocar memória. Se o vetor for pequeno demais, devolve `false` sem escrever nada. O tamanho necessário é `(4·largura + 2)·(2·altura + 1) + 1` bytes: 163 no 4x4 e 451 no 12x4.

---

## 7. `robo_simulado.h`: o robô de mentira

Para testar a **exploração inteira** sem depender da placa e da pista física, o robô simulado imita o robô real:

| | Robô real | Robô simulado |
|---|---|---|
| Labirinto de verdade | a pista física | o `gabarito_` (um `Labirinto` lido com `carregar`) |
| O que o robô sabe | o que os sensores já viram | o `mapa_`, que começa vazio |
| Sensores | 3 VL53L0X: frente, esquerda e direita | `sentir()` copia do gabarito para o mapa só esses 3 lados |
| Andar | motores + encoders | `mover(d)`: gira, consulta o mapa, confere a parede no gabarito e anda |

Partes principais:

- **Construtor:** prepara um mapa do mesmo tamanho do gabarito, com a mesma orientação inicial. Copia o objetivo (caso o teste tenha trocado o objetivo com `definirObjetivo`) e já faz a primeira leitura dos sensores. O robô larga em (0,0) virado para o Norte ou para o Leste, como manda a convenção do `labirinto.h`.
- **`mover(d)`:** segue o contrato de movimento do `labirinto.h`, na mesma ordem que o robô real vai seguir:
  1. conta o giro e vira;
  2. pergunta ao mapa com `podeAvancar()`; se houver parede conhecida na frente, não aciona o "motor";
  3. confere o labirinto real (o gabarito); se houver parede de verdade, o robô não sai do lugar;
  4. só depois que o robô andou, registra com `avancar()`. Se ele devolver `false`, a pose perdeu a sincronia, e `sincronizado()` passa a ser `false`;
  5. marca como aberto o lado por onde passou, a partir da célula anterior. Se o mapa tinha uma parede falsa ali, ela é corrigida, como o Marcelo pediu;
  6. lê os sensores no centro da célula nova.
- **`explorar()`:** o laço da exploração, igual ao que o robô real vai fazer na #210:
  ```
  enquanto não chegou ao objetivo:
      recalcula as distâncias (modo EXPLORACAO)
      pergunta a próxima direção
      anda
  ```
  Tem um limite de passos (1000 por padrão) para que um erro no algoritmo não deixe o teste rodando para sempre.
- **`explorarAteRotaGarantida()`:** chama o `explorar()` e, se a rota ainda não estiver garantida (seção 5), faz viagens de ida e volta entre o objetivo e a largada até estar. É o que o passo 1 do CT-SE14 pede: explorar até o mapa bastar para a rota mínima. Nos testes, o `BARREIRAS_8X4` precisa de uma volta à largada (16 passos viram 32), e o `ARMADILHA_8X4` já sai garantido na primeira chegada.
- **Contagem de giros:** virar 90° conta 1, virar 180° conta 2, seguir reto conta 0. Isso serve para comparar estratégias ("esta rota tem menos curvas?").

Ele fica em `test/` e não em `lib/` porque **só existe para testes**: não vai para o robô e não entra na cobertura. Fica na **raiz** de `test/`, e não dentro de uma pasta `test_*`, por dois motivos. Os testes unitários e os testes de aceitação usam o mesmo arquivo, com `#include "../robo_simulado.h"`. E o PlatformIO trata toda pasta `test_*` como um conjunto de testes: uma pasta só com o `.h` dá o erro "Nothing to build".

---

## 8. Os testes

Os testes usam o **Unity**, a mesma ferramenta do `test_crc16` e do `test_labirinto`. Cada `TEST_ASSERT_...` é uma afirmação; se for falsa, o teste falha e mostra a linha. Há dois arquivos, com papéis diferentes:

| Arquivo | Tipo | O que confere |
|---|---|---|
| `test_floodfill.cpp` | **unitário** | cada função isolada, com labirintos pequenos feitos sob medida para cada teste |
| `test_navegacao.cpp` | **aceitação** | os casos de teste do roteiro (CT-SE09, CT-SE11 e CT-SE14) de ponta a ponta, com o robô simulado nos labirintos de `desenhos_labirintos.h` |

### Testes unitários: `test_floodfill.cpp`

O labirinto `LAB_4X4` do topo do arquivo é o mesmo da seção 6. A rota mais curta dele (**Leste, Leste, Leste, Norte, Norte, Norte**) foi conferida à mão, olhando o desenho, e não tirada do próprio código.

A função `percorre` anda uma rota no mapa, consultando `podeAvancar()` a cada passo como a navegação faz, e confirma que ela **chega ao objetivo sem atravessar parede**. É uma prova independente de que a rota é válida.

| Teste | O que confere | Requisito / CT |
|---|---|---|
| `sem_paredes_internas_e_distancia_de_manhattan` | 4x4 vazio: cada célula = (3−x) + (3−y) | RF11 |
| `rapido_trata_desconhecida_como_fechada` | no modo RAPIDO, mapa novo: só o objetivo tem distância | RF12 |
| `recalculo_apos_parede_nova` | parede nova faz (3,2) passar de 1 para 3 | RF11 |
| `distancia_fora_do_mapa_e_infinita` | células fora do labirinto são `INFINITO` | — |
| `valor_atual_e_a_distancia_da_celula_do_robo` | `valorAtual` acompanha o robô (6, depois 5) | telemetria `valor_flood` |
| `empate_prefere_seguir_reto` | empate: segue na direção em que está virado | menos giros |
| `empate_prefere_direita_a_esquerda` | frente bloqueada, empate entre os lados: direita | menos giros |
| `beco_sem_saida_volta_por_tras` | num beco, a resposta é o giro de 180° | RF23, CT-SE09 |
| `largada_com_frente_fechada_sai_pela_direita` | largada com parede à frente: a direção escolhida é a direita | convenção de largada |
| `largada_com_borda_a_direita_e_frente_fechada_sai_pela_esquerda` | virado para o Leste com parede à frente: a direção escolhida é a esquerda | convenção de largada |
| `sem_caminho_nao_escolhe_direcao` | sem caminho, `proximaDirecao` devolve `false` | — |
| `carregar_le_as_paredes_do_desenho` | paredes do `LAB_4X4` lidas nos lugares certos | — |
| `carregar_le_labirinto_retangular` | lê um 8x4 com largura e altura certas e objetivo em (7,3) | — |
| `carregar_ignora_linhas_em_branco_nas_pontas` | aceita `\n` extra, espaços e `\r` | — |
| `carregar_com_borda_a_direita` | `carregar(..., LESTE)` larga virado para o Leste; orientação sem borda atrás é recusada | convenção de largada |
| `carregar_recusa_desenho_malformado` | vazio, linhas pares, tamanhos diferentes, célula estreita, linha em branco no meio | — |
| `carregar_recusa_tamanho_fora_do_roteiro` | um 2x1 bem desenhado é recusado | tamanhos do TAP |
| `carregar_recusa_labirinto_maior_que_16` | 17 de largura é recusado | RNF03 |
| `imprimir_mapa_com_lados_desconhecidos` | 4x4 inexplorado: desenho completo com pontos e distâncias de Manhattan | CT-SE11 |
| `imprimir_mapa_mostra_paredes_e_distancias` | `LAB_4X4`: fileiras do topo e da largada com distâncias contadas à mão | CT-SE11 |
| `imprimir_mapa_pode_ser_lido_de_volta` | imprimir e ler com `carregar` devolve as mesmas paredes | — |
| `imprimir_mapa_sem_caminho_e_buffer_pequeno` | célula sem caminho aparece como `-`; vetor pequeno devolve `false` | — |
| `rota_mais_curta_no_labirinto_conhecido` | `LAB_4X4`: 6 passos, na ordem esperada, e a rota é válida | RF12, CT-SE14 |
| `rota_desvia_de_parede_nova` | parede nova na saída muda o 1º passo de Norte para Leste | CT-SE14 (passo 3) |
| `rota_mais_curta_parte_da_orientacao_inicial` | virado para o Leste, o empate faz a rota seguir reto para o Leste | convenção de largada |
| `rota_so_usa_lados_conhecidos` | o caminho pelo Norte empataria, mas é desconhecido, então a rota vai pelo Leste | RF12 |
| `sem_rota_ou_rota_que_nao_cabe_devolve_zero` | sem rota segura, ou vetor pequeno demais: 0 | — |
| `rota_garantida_com_o_mapa_completo` | com todas as paredes conhecidas, a rota está garantida | CT-SE14 (passo 1) |
| `rota_nao_garantida_sem_rota_conhecida` | mapa vazio: sem rota segura, nada garantido | CT-SE14 (passo 1) |
| `rota_nao_garantida_enquanto_um_atalho_e_desconhecido` | rota conhecida de 8 passos com atalho de 6 ainda não visto: não garantida; ao ver o atalho, passa a garantida | CT-SE14 (passo 1) |
| `robo_simulado_explora_ate_o_objetivo` | a exploração chega ao objetivo, sem perder a sincronia, e o mapa das células visitadas é igual ao gabarito | RF11, CT-SE11 |
| `robo_simulado_sai_da_largada_pela_direita` | largada com a frente fechada: chega em 6 passos e 2 giros, o mínimo possível | convenção de largada |
| `robo_simulado_nao_atravessa_parede_e_conta_giros` | `mover` recusa parede e conta 90° = 1 e 180° = 2 | — |
| `tempo_recalculo_menor_50ms` | 1000 recálculos no 4x4, 8x4, 12x4 e 4x12: média e máximo < 50 ms, conferindo que o mapa medido é de verdade. Com `-v`, mostra a média e o máximo de cada tamanho | RNF01 |
| `tamanho_fixo_do_flood_fill` | `sizeof(FloodFill)` ≤ 512 bytes | RNF03 |

**Cobertura** (quanto do código os testes executam): **100% das linhas** de `floodfill.cpp` e de todo o `lib/`.

### Testes de aceitação: `test_navegacao.cpp`

Os labirintos ficam em `test/desenhos_labirintos.h`, na raiz de `test/`, como o `robo_simulado.h`. Eles são **sintéticos**: foram desenhados pela equipe e não substituem os labirintos oficiais da disciplina, quando existirem.

| Desenho | Tamanho | Para que serve |
|---|---|---|
| `MISTO_4X4` | 4x4 | paredes variadas, rota mínima pelo caso fácil |
| `BARREIRAS_8X4`, `BARREIRAS_12X4` | 8x4, 12x4 | barreiras que obrigam a rota a subir e descer: os tamanhos do roteiro com um caminho só |
| `DUAS_ROTAS` | 4x4 | duas rotas de tamanhos diferentes (6 e 10), para ver se a menor é a escolhida |
| `BECO_LARGADA` | 4x4 | largada em beco com a saída à frente, virado para o Norte, ou à esquerda, virado para o Leste (CT-SE09) |
| `SAIDA_DIREITA` | 4x4 | largada em beco com a saída à direita, virado para o Norte, ou à frente, virado para o Leste (CT-SE09) |
| `ARMADILHA_4X4`, `_8X4`, `_12X4` | 4x4, 8x4, 12x4 | o caminho que parece mais curto termina num beco: o robô precisa achar a parede nova, recalcular e revisitar células (CT-SE11) |
| `CORREDOR_RETO` | 4x4 | objetivo em (3,0) e 12 células isoladas: o único desenho em que uma parede nova deixa o robô **sem rota** |

Todo desenho tem a borda inteira fechada. Cada desenho guarda também o campo `movimentosConferidos`: o tamanho da rota mínima que **uma pessoa** achou e justificou em [`conferencia-manual-labirintos.md`](conferencia-manual-labirintos.md). Os testes comparam a resposta do Flood Fill com duas referências independentes: essa ficha e uma busca em largura simples (`menorCaminho`), escrita direto nas paredes do desenho, sem passar pelo `FloodFill`. O roteiro aceita as duas formas ("calculado à mão ou por script de referência").

| Teste | O que confere | CT |
|---|---|---|
| `fixtures_tem_dimensoes_bordas_e_simbolos_validos` | cada desenho tem o tamanho certo, bordas fechadas e só `+ - \|` e espaço | — |
| `objetivos_das_fixtures_sao_alcancaveis` | existe caminho da largada ao objetivo em cada desenho | — |
| `ct_se09_largada_com_saida_a_frente` | sem girar: `BECO_LARGADA` virado para o Norte vai a (0,1); `SAIDA_DIREITA` virado para o Leste vai a (1,0) | CT-SE09 |
| `ct_se09_largada_com_saida_oposta_a_borda` | frente fechada, um giro para o lado oposto à borda: `SAIDA_DIREITA` virado para o Norte vira à direita; `BECO_LARGADA` virado para o Leste vira à esquerda | CT-SE09 |
| `ct_se11_mapa_montado_igual_ao_desenho` | nas duas orientações de largada, depois de explorar, todo lado conhecido é igual ao desenho, toda célula visitada tem os 4 lados conhecidos e nada é "inventado" em célula não visitada | CT-SE11 |
| `ct_se11_armadilhas_obrigam_a_revisitar` | nas armadilhas o robô revisita células e anda mais que o mínimo, sem corromper o mapa. Só virado para o Norte: as armadilhas contam com o robô subindo pela coluna x=0, e virado para o Leste ele segue reto e nem passa por elas | CT-SE11 (passo 1) |
| `ct_se14_rota_igual_a_referencia` | nas duas orientações de largada, depois de explorar até a rota estar garantida, a rota tem o tamanho da busca de referência **e** da ficha manual, e só passa por células ligadas | CT-SE14 (passos 1 e 2) |
| `ct_se14_conferencia_manual_bate_com_o_script` | a ficha manual e a busca de referência concordam nos 10 desenhos; se discordarem, a ficha precisa ser revista | CT-SE14 |
| `ct_se14_parede_nova_bloqueia_a_rota` | uma parede nova fecha a rota no meio: a nova rota desvia e é a menor do mapa atualizado, ou 0 quando não sobra caminho | CT-SE14 (passo 3) |

Na simulação, todo canto de largada vira (0,0) pela convenção do `labirinto.h`; o que muda entre os cantos é o lado da borda, e os testes cobrem os dois (Norte e Leste). A repetição do CT-SE09 em cada canto físico continua sendo do ensaio real.

Os testes de aceitação provam o algoritmo **em simulação**. A evidência física dos CTs (vídeo, log serial, telemetria) continua dependendo do robô montado.

---

## 9. Como rodar

O Flood Fill usa o `lib/labirinto/`, que já está na `develop` (PR #242). Dentro da branch `feature/164-flood-fill`:

```bash
cd src/firmware
source .venv/bin/activate
pio test -e native -f test_floodfill    # só os testes unitários do Flood Fill
pio test -e native -f test_navegacao    # só os testes de aceitação
pio test -e native                      # todos os testes
pio test -e native -v                   # todos, mostrando os tempos do RNF01
gcovr -r . --filter lib/ .pio/build/native
```

O `-f` (de *filter*, filtro) roda só a pasta com aquele nome dentro de `test/`. O `-v` (de *verbose*, detalhado) mostra também as mensagens dos testes, como as linhas `RNF01 12x4: media ... ms, maximo ... ms`.

Para guardar a saída como evidência, use `pio test -e native -v > log.txt`. O log muda a cada execução, então **não vai para o Git**: anexe-o ao PR ou use um print dele no documento de testes.

---

## 10. Perguntas que podem aparecer na revisão

**Por que recalcular tudo a cada passo, e não só o pedaço que mudou?**
Existem versões que atualizam só a região afetada, mas elas são bem mais difíceis de acertar. O cálculo completo leva milésimos de milissegundo e o requisito é 50 ms. Código simples e correto vale mais aqui.

**O robô sempre acha o objetivo na exploração?**
Se existir caminho, sim. Cada vez que ele encontra uma parede, o mapa fica mais parecido com o real e as distâncias são refeitas. No pior caso, ele acaba visitando o labirinto inteiro, que é finito.

**A rota da exploração é a mais curta?**
Não necessariamente. A exploração acha *um* caminho. Por isso existe `rotaMaisCurta`, que usa só o que já foi confirmado. Se o robô explorou pouco, a rota rápida pode ser mais longa que a ideal, mas é sempre **segura**. Para saber se ela já é a ideal, existe a `rotaGarantida`: enquanto ela responder `false`, ainda pode haver um atalho escondido, e vale explorar mais antes da corrida rápida.

**Por que o objetivo é uma célula só, e não o centro 2x2 do Micromouse oficial?**
O Tema do PI1 define o objetivo como o canto diagonalmente oposto à largada. O código já aceita mais de uma célula de objetivo (o `recalcular` põe na fila todas as células em que `ehObjetivo` é verdadeiro), caso isso mude.

**Por que `uint8_t` para as coordenadas?**
Elas vão de 0 a 15, e um `uint8_t` guarda de 0 a 255 em 1 byte. Economiza memória no ESP32 e combina com o contrato de telemetria.

---

## 11. Largada em qualquer canto

Regras, depois das validações:

1. o robô pode largar em **qualquer canto**, com a borda atrás e à esquerda ou atrás e à direita dele;
2. o objetivo é **só** o canto **diagonalmente oposto** à largada, e a corrida **termina quando o robô chega a essa célula** (`noObjetivo()`). A borda do labirinto é toda fechada: não existe abertura de saída;
3. a equipe informa o tamanho do labirinto e **de que lado do robô fica a borda**.

> Em 19/10/2026 chegou a ser combinado que a saída teria uma abertura na borda e que a corrida terminaria ao atravessá-la. Essa decisão foi cancelada, e o código da abertura foi retirado.

### Largada: girar o referencial, não mudar a origem

Num canto, o robô tem a borda atrás e de um dos lados. Em vez de mudar a origem, o mapa muda **para onde o robô aponta**:

| Borda | O robô larga virado para | largura | altura |
|---|---|---|---|
| atrás e à esquerda | Norte | células ao lado | células à frente |
| atrás e à direita | Leste | células à frente | células ao lado |

Nos dois casos a borda de trás e a lateral são o Sul e o Oeste de (0,0). A largada continua em (0,0) e o objetivo em (largura-1, altura-1), então o frontend e a telemetria não mudam. É uma **rotação**, e não um espelho: `girarDireita` e `girarEsquerda` continuam certas.

Quem informa não precisa pensar em Norte e Leste. `iniciarNaLargada(borda, celulasAFrente, celulasAoLado)` recebe o que se vê ao posicionar o robô e faz a conversão da tabela. Esses dados chegam ao robô **pelo site**: a equipe preenche o lado da borda e as células à frente e ao lado, e o backend envia ao robô antes da largada. Para isso, o contrato de telemetria ainda precisa de uma mensagem do servidor para o robô.
