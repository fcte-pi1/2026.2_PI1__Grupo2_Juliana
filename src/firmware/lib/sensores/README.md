# Filtro de distancia e deteccao de paredes — #211

Parte logica de RF01/HU78 e RF14/HU97, preparatoria aos CT-SE03 e CT-SE04.
Codigo C++ puro, memoria fixa, sem Arduino, acesso a GPIO ou espera bloqueante.

## Uso pela navegacao e pelo driver

Manter tres pares de `FiltroDistancia` e `DetectorParede`, um por sensor
(frontal, esquerdo, direito). Para cada amostra nova, passar a distancia em mm
e a validade do driver a `atualizar`. Timeout, falha I2C/range ou amostra
nao atualizada devem ser informados com `sensorOk=false`. Zero e valores
maiores ou iguais a 8190 tambem sao rejeitados. Uma leitura invalida elimina
o historico: sao necessarias cinco amostras novas para voltar a decidir.

`AQUECENDO` e `INVALIDA` produzem parede `DESCONHECIDA`, nunca passagem livre.
Ao mudar de celula ou girar para observar outra direcao, chamar `reiniciar()`
nos filtros e detectores, evitando atribuir a leitura antiga a outra parede.
A aquisicao precisa terminar na pose usada para converter as direcoes.

O filtro calcula mediana das ultimas cinco amostras e depois EMA com alpha=0,3.
O detector usa limiar 120 mm e histerese de 10 mm: abaixo de 110 ha parede;
acima de 130 ha passagem; na banda conserva a classificacao anterior. Sem
classificacao anterior a banda permanece desconhecida. Esses numeros em
`lib/config/config.h` sao PROVISORIOS e exigem calibracao no robo.

`paredesAbsolutas` retorna duas mascaras N=1/L=2/S=4/O=8: `presentes` e
`conhecidas`. Atualizar o `Labirinto` apenas nas direcoes conhecidas:

```cpp
auto p = paredesAbsolutas(frente, esquerda, direita, lab.orientacao(), tras);
const Direcao direcoes[] = {NORTE, LESTE, SUL, OESTE};
for (Direcao d : direcoes) {
    if (!(p.conhecidas & d)) continue;
    if (p.presentes & d) lab.marcarParede(lab.x(), lab.y(), d);
    else lab.marcarAberta(lab.x(), lab.y(), d);
}
```

`tras` e desconhecida por padrao. Na largada, vem da observacao frontal apos
giro de 180 graus e retorno. Depois de uma passagem confirmada, a aresta
percorrida e conhecida como livre. Apos outros giros, usar a orientacao
absoluta dessa aresta: ela pode nao estar mais atras do robo. Giro e deslocamento
sao responsabilidades da navegacao/drivers (#164/#213/#214).

## Diferenca justificada em relacao ao planejamento

Valores brutos iguais em amostras novas NAO bastam para declarar defeito:
um sensor diante de uma parede imovel pode repetir o valor. A validade deve
vir do driver e da atualizacao da medicao. Deteccao de congelamento requer
timeout/data-ready ou teste com estimulo conhecido na calibracao. A biblioteca
[Pololu VL53L0X](https://github.com/pololu/vl53l0x-arduino#library-reference)
documenta `timeoutOccurred()` e `last_status`; este modulo nao substitui o driver.

## Testes e limites da evidencia

Executar em `src/firmware`: `pio test -e native`.
Os testes usam series SINTETICAS, nao medicoes gravadas do hardware. Verificam
aquecimento, mediana, resposta da EMA, histerese, falhas, recuperacao, mascaras
em quatro orientacoes e integracao com o mapa existente.

Uma mediana de cinco rejeita ate duas amostras espurias na janela. Isso nao
prova rejeicao de qualquer perturbacao de 100 ms nem de uma lanterna por 1 s:
duracao, taxa de amostragem e efeito optico precisam ser medidos no CT-SE04.
Nao usar essa filtragem atrasada como unica protecao imediata contra colisao.

Para o ensaio real, o driver deve registrar no Serial o CSV
`t,bruto_f,filt_f,bruto_e,filt_e,bruto_d,filt_d`, usando campo vazio quando a
filtrada ainda nao for valida, acompanhado de log da validade e das paredes.
Coletar 10 s estaveis, objeto breve e lanterna conforme 4.4.6. O firmware
principal ainda e um esqueleto: leitura fisica e emissao serial dependem da #213.

## Decisoes da revisao do PR #243

8190/8191 continuam como INVALIDA, inclusive com sensorOk=true. Esse booleano
nao distingue ausencia de alvo de uma medicao sem qualidade suficiente.
Converter esses numeros diretamente em LIVRE pode abrir uma parede no mapa
sem evidencia. Confirmar na #213, com o driver e ensaios reais, um estado
explicito de fora de alcance antes de mudar essa politica. Na biblioteca
Pololu, last_status informa a transmissao I2C, nao a qualidade da distancia:
https://github.com/pololu/vl53l0x-arduino#library-reference

O contrato atual exige aquisicao parada na mesma celula e orientacao.
Cinco amostras a 33 ms representam aproximadamente 165 ms de aquecimento,
alem da resposta da EMA. Mesmo a 20 ms por amostra, sao 100 ms. Portanto,
este filtro ainda nao comprova o ciclo completo de 50 ms do RNF01.
A integracao #213/#214 precisa medir a cadencia real e rever a janela ou
separar aquisicao e decisao antes de usar leituras durante o movimento.
Nao reutilizar historico de outra pose para esconder esse atraso. Mantemos
o reinicio apos falha para evitar reaproveitar medidas anteriores a uma
interrupcao cuja duracao esta fora do controle desta biblioteca.

Os valores padrao agora sao validados por static_assert. As verificacoes
em execucao e seus testes permanecem: os construtores publicos aceitam
alpha, limiar e histerese diferentes das constantes de config.h.
