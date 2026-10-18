# Preparacao dos testes de aceitacao — #164

Parte da Geovana. O algoritmo, carregador ASCII e robo simulado continuam
com a Leticia. `labirintos.h` contem seis desenhos sinteticos: 4x4, 8x4,
12x4, corredor, beco inicial e duas rotas. `test_navegacao.cpp` por enquanto
verifica APENAS formato, bordas e existencia de caminho nos desenhos.
Passar nesses testes NAO significa que o Flood Fill foi implementado/aprovado.

## Para integrar com a Leticia

1. Receber as assinaturas reais de `carregar`, `recalcular`,
   `proximaDirecao`, `rotaMaisCurta` e do robo simulado. Nao implementar
   uma segunda versao do algoritmo apenas para fazer os testes passarem.
2. Preencher `CONFERENCIA_MANUAL.md` com a rota e comprimento conferidos
   por pessoa, como exige o planejamento da dupla. Nao copiar a resposta
   do algoritmo testado nem apresentar uma resposta de IA como conferencia humana.
3. Usar `Labirinto::iniciar` real: os tamanhos oficiais sao 4x4, 8x4 e
   12x4 (e suas rotacoes). O 16x16 mencionado no plano antigo nao e aceito
   pela API integrada na #163.
4. Adicionar os testes abaixo no mesmo diretorio. Nao marcar CT-SE09,
   CT-SE11 ou CT-SE14 como aprovado antes da execucao do algoritmo integrado.

## Assercoes de aceitacao a implementar apos receber a API

| Caso | Entrada e verificacoes |
| --- | --- |
| CT-SE09 | BECO_LARGADA: iniciar voltado para Leste, Sul e Oeste. A unica saida fisica e Norte. Para Sul, exigir giro de 180 graus; nas outras duas, 90 graus. Nenhuma tentativa de atravessar parede. |
| CT-SE11 | Explorar MISTO_4X4, BARREIRAS_8X4 e BARREIRAS_12X4 com tres sensores simulados. Revelar so o que seria observado e a aresta de chegada; terminar no canto oposto, com limite de passos para detectar loop. Comparar as arestas conhecidas com o desenho, inclusive espelhamento. |
| CT-SE14 | Carregar mapa conhecido DUAS_ROTAS, conferir cada passo contra as paredes e comparar comprimento com referencia MANUAL. Fazer o mesmo nos tamanhos oficiais; nao presumir que a primeira rota encontrada e minima. |
| CT-SE14, parede nova | Na fixture 4x4, revelar uma parede que invalida a proxima escolha. Recalcular e exigir alternativa legal ou sinal explicito de falta de rota. |
| RNF01 | Medir recalculos no 12x4 com relogio monotonicamente crescente; relatar maquina, media e maximo. A medicao no PC nao substitui o limite de 50 ms no ESP32. |

A exploracao que termina na primeira chegada nao necessariamente conhece
todo o labirinto. Comparar apenas as arestas observadas; se o ensaio exigir
mapa integral, precisa executar uma fase que efetivamente visite/revele
essas arestas. Nao aprovar um mapa completo so porque o objetivo foi atingido.

Salvar log de simulacao separado da evidencia fisica. Os desenhos deverao
ser substituidos/complementados pelos labirintos oficiais quando disponiveis.
