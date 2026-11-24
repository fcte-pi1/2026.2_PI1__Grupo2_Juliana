# Firmware

Firmware do Micromouse para o ESP32-DEVKIT-V1 em C++ com o framework Arduino, usando o [PlatformIO](https://platformio.org/).

## Requisitos
- PlatformIO: extensão do VS Code ou `pip install platformio`
- Para rodar os testes no computador: compilador `gcc`/`g++` instalado (no Windows, MinGW)

## Estrutura
- `src/`: código que só roda no robô.
  - `main.cpp`: lê os sensores, envia a telemetria pelo Wi-Fi e roda a corrida automática; comandos pelo monitor serial ou pelo botão BOOT.
  - `sensores.cpp`: 3 VL53L0X (endereços trocados pelo XSHUT), BNO085 e INA226.
  - `telemetria_wifi.cpp`: envio por WebSocket numa tarefa própria (núcleo 0), com o buffer para quedas de conexão.
  - `navegacao_nucleo1.cpp`: tarefa no núcleo 1 que atualiza o mapa com as paredes vistas e recalcula o Flood Fill (vai na telemetria como `valor_flood`).
  - `pinos.h`: pinagem da placa.
  - `bancada/`: testes de bancada de cada sensor, um programa por arquivo (ver abaixo).
- `lib/`: lógica independente do hardware, testada no computador.
  - `labirinto/`: mapa e pose do robô. `navegacao/`: Flood Fill e máquina de estados do modo automático.
  - `movimento/`: controle de movimento célula a célula (andar, girar, centralizar nas paredes) e o `RoboFisico`, que liga o modo automático ao robô.
  - `sensores/`: filtro de ruído (mediana) e detecção de paredes dos VL53L0X.
  - `telemetria/`: pacotes do contrato, CRC16 e buffer de retransmissão.
- `test/`: testes com [Unity](https://github.com/ThrowTheSwitch/Unity), um diretório `test_<nome>/` por módulo. O `test_movimento` usa um robô simulado com física simples (`robo_fisico_simulado.h`: motores, encoders, giroscópio, os 3 sensores medindo as paredes e colisão) para rodar o controle e a corrida inteira sem o robô.
- `include/credenciais.exemplo.h`: modelo do arquivo com o Wi-Fi e o servidor.

> **Atenção:** a ESP32 da placa atual tem defeito na cache do núcleo 1 e trava com `IllegalInstruction`. Por isso o `platformio.ini` roda o Arduino no núcleo 0 (`-DARDUINO_RUNNING_CORE=0`) e a tarefa de telemetria também fica no núcleo 0. Detalhes nos testes 3.11 e 3.18 do [7.3 - Testes de hardware](../../docs/7.3%20-%20Testes%20de%20hardware.md).
>
> O defeito é um único bit: o núcleo 1 roda normalmente funções `IRAM_ATTR` e qualquer código que não caia nos bytes defeituosos. Para saber o que não pode ir para o núcleo 1, compile e rode `python nucleo1_risco.py` (lista as funções afetadas naquela compilação).
>
> A navegação (`navegacao_nucleo1.cpp`: Flood Fill e modo automático) roda no núcleo 1, **na RAM interna**: as funções dela são marcadas com `NUCLEO1` (`lib/labirinto/nucleo1.h`), que no ESP32 as põe na IRAM e no computador não faz nada. Função nova chamada pelo núcleo 1 também precisa do `NUCLEO1`. As raízes ficam em `custom_nucleo1` no `platformio.ini`; depois de compilar, o `nucleo1_verificar.py` lista o que ainda ficou na flash e falha a compilação se algo cair num byte defeituoso.

## Compilar e gravar no robô
```bash
cd src/firmware
pio run -e esp32dev                    # compila
pio run -e esp32dev -t upload          # grava pela USB
pio device monitor                     # abre o monitor serial (115200)
```

### Wi-Fi e servidor
Copie `include/credenciais.exemplo.h` para `include/credenciais.h` e preencha a rede (2,4 GHz), o IP do computador que roda o backend e a senha do OTA. O `credenciais.h` fica fora do git. Sem ele o firmware compila com o Wi-Fi desligado.

O ESP32 e o computador precisam estar na mesma rede. O hotspot do celular não serve se o computador estiver na ancoragem USB: o celular isola as duas redes.

O backend precisa aceitar conexões de fora: `uvicorn app.main:app --host 0.0.0.0 --port 8000`.

### Bateria e USB: nunca os dois juntos
Com a bateria ligada, desligue o USB. Para ver e comandar o robô sem o cabo, use o **console pelo Wi-Fi** (porta 23), que mostra o mesmo que o monitor serial e aceita os mesmos comandos:
```bash
python console_wifi.py <IP do robô>      # o IP aparece no boot: "OTA e console pelo Wi-Fi prontos em ..."
```
Para gravar com a bateria ligada, use o OTA (abaixo).

### Comandos (monitor serial ou console pelo Wi-Fi)
| Tecla | Ação |
| :--- | :--- |
| `i` | calibra e inicia uma corrida (evento `inicio`); abra a corrida no sistema web antes |
| `f` | encerra a corrida como concluída (evento `fim`) |
| `a` | encerra a corrida como abandonada (manual) |
| `c` | calibração: sensores, bateria, yaw e encoders zerados, e um toque em cada motor |
| `l` | log de erros |
| `m` | teste dos motores, **com as rodas no ar**: tabela de PWM × pulsos/s × corrente |
| `e` | mostra e zera os encoders (para medir os pulsos por volta da roda) |
| `p` | para os motores |
| `z` | zera o yaw: a orientação atual vira o Norte |
| `s` | mostra o estado agora |
| `r` | zera as estatísticas de RTT |
| `L` | calibra os limiares de parede (#279): robô parado no centro de uma célula, mede com paredes nos 3 lados e depois sem elas; os limiares ficam salvos na flash |
| `Q` / `G` / `H` | derruba o Wi-Fi por 5 s / 60 s / 12 min, este último até encher o buffer (#282). Inicie a corrida com `i` antes; o console Wi-Fi cai junto, então acompanhe pela serial ou mande `l` depois |
| `A` | **corrida automática** (robô no chão, bateria): explora até o objetivo, volta, repete se preciso e faz a corrida rápida; `p`, `a` ou BOOT param |
| `M` / `B` | tamanho do labirinto (células à frente × ao lado na largada) / onde ficam as bordas do canto: atrás e à esquerda, atrás e à direita, à frente e à direita ou à frente e à esquerda (robô virado para a borda); ficam salvos na flash |
| `F` / `3` | anda 1 / 3 células (CT-SE05) |
| `<` / `>` / `U` | gira 90° à esquerda / à direita / meia-volta (CT-SE06) |
| `O` | liga/desliga o log do controle: uma linha `mov,...` a cada 50 ms com erro lateral e sinal de controle (CT-SE07) |
| `K` | parâmetros do controle, referências da calibração das paredes e labirinto configurado |
| `Y` | calibra os encoders: com uma parede a 30-90 cm na frente, anda ~20 cm e compara os pulsos com a distância medida pelo sensor; mostra se algum encoder ou motor está invertido e salva os pulsos por mm na flash |
| `+` / `-` | PWM máximo do movimento, de 0 a 255 em passos de 5 (padrão 20, salvo na flash): limita andar, girar e o reforço da roda parada |

O botão BOOT, sem corrida: um toque inicia uma corrida com o robô parado (só telemetria, como o `i`); **segurado por 1 s**, a corrida automática (começa 1 s depois de soltar). Com corrida, apertar encerra. A cada segundo o console mostra o Wi-Fi, a conexão com o servidor, as mensagens enviadas e confirmadas, o RTT (`rtt=último/médio/máximo`), as distâncias, as paredes, o yaw, a bateria (tensão, %, corrente, autonomia), os motores e os encoders.

### Motores: limite de 5 V
Pedido da Eletrônica: os motores N20 não podem passar de **5 V**. O driver (`motores.cpp`) limita o PWM a `5 V ÷ tensão da bateria` (59,5 % com a bateria cheia, 8,4 V), recalculado com a leitura do INA226, e sobe a potência em rampa (0 ao máximo em 0,5 s). Sem bateria (só USB) os motores não ligam: os 5 V do USB voltam pelo MP1584 até o DRV8833.

Calibrar com o robô montado: `INVERTE_*` (motor ou encoder espelhado) e `PULSOS_POR_VOLTA` em `motores.h`.

### Corrida automática
O modo automático (`lib/navegacao/modo_automatico.*`) decide a próxima célula pelo Flood Fill; o `RoboFisico` (`lib/movimento/robo_fisico.*`) gira para ela e anda uma célula com o controle de movimento (`lib/movimento/controle_movimento.*`), e lê as paredes parado no centro. Roda no núcleo 0, junto com o laço: durante o movimento, a telemetria (com a pose do mapa e o estado), a bateria, o console e o OTA continuam rodando.

O controle de movimento:
- **anda** pelo encoder (média das rodas), mantendo o rumo e centralizando nas paredes dos lados; com parede na frente, para pela distância até ela (corrige o erro dos encoders);
- **gira** no lugar até o rumo, pelo BNO085 ou, sem ele, pela diferença dos encoders;
- **sem o BNO085** (ainda falha na bateria), o rumo vem dos encoders e é corrigido pelas paredes laterais enquanto anda. Por isso a **largura do eixo** (`larguraEixoMm` em `controle_movimento.h`, centro a centro das rodas) precisa ser medida com até ~2 mm de erro;
- guarda o quanto o robô ficou fora do centro de cada célula e compensa no próximo movimento;
- se as rodas travarem (colisão) ou aparecer parede onde o mapa não tinha, volta de ré ao centro da célula e o modo automático recalcula o caminho (RF16-S).

Primeiro teste no robô, nesta ordem:
1. Medir a largura do eixo e acertar `larguraEixoMm`; conferir `INVERTE_*` em `motores.h` (comando `m`, rodas no ar).
2. `L` no centro de uma célula: além dos limiares, salva a distância de cada lado até a parede, que o controle usa para parar e centralizar.
3. Num corredor: `F`, `3`, `<`, `>`, `U`. Ajustar os ganhos e potências em `ParametrosMovimento` (`controle_movimento.h`; `K` mostra os atuais). Potência mínima baixa demais = robô não sai do lugar e acusa bloqueio; ganho de rumo alto demais = zigue-zague.
4. `M` e `B` com o labirinto da corrida, `K` para conferir, abrir a corrida no sistema web e `A` (ou BOOT segurado).

Os ganhos atuais foram ajustados no simulador (`test/robo_fisico_simulado.h`), que passa nos critérios do roteiro (parada a ±2 cm do centro, giros a ±5°) com uma roda 15% mais fraca, sem BNO085 e com a largura do eixo errada em 2%. No robô, eles são o ponto de partida.

### Bateria
Regras do Contrato de Telemetria (`lib/energia/monitor_bateria.*`): abaixo de 7,0 V por 1 s manda o evento `bateria_critica` (repete só depois de passar de 7,2 V); abaixo de 6,6 V por 1 s para os motores e encerra a corrida como abandonada; INA226 sem resposta faz o mesmo com `falha_sensor_bateria`. O percentual é linear de 6,4 a 8,4 V, como no painel.

### Gravar pelo Wi-Fi (OTA)
```bash
set MICROMOUSE_OTA_SENHA=<a OTA_SENHA do credenciais.h>      # Linux/macOS: export ...
pio run -e esp32dev_ota -t upload --upload-port <IP do robô>
```
O IP aparece no monitor serial (`OTA pronto em ...`). O ESP32 abre uma conexão de volta para o computador na porta TCP 8266: se o envio ficar parado em 0%, libere essa porta no firewall.

## Testes de bancada
Cada teste é um programa separado em `src/bancada/`, com seu ambiente no `platformio.ini`. Rodam só com o USB, sem bateria.

| Ambiente | O que testa |
| :--- | :--- |
| `teste_i2c` | ESP32 rodando e varredura do barramento I2C |
| `teste_vl53` | os 3 VL53L0X: endereços pelo XSHUT e distâncias |
| `teste_bno` | yaw, pitch e roll do BNO085 |
| `teste_encoder` | contagem dos encoders girando a roda à mão (`z` zera) |
| `teste_nucleo1` | o que o núcleo 1 consegue fazer: mapa do defeito da cache e execução de código (E1 a E4) |

```bash
pio run -e teste_vl53 -t upload && pio device monitor
```

## Testar no computador
```bash
pio test -e native
gcovr -r . --filter lib/ .pio/build/native   # cobertura (pip install gcovr)
```
O GitHub Actions roda os testes, a cobertura e a compilação para o ESP32 em todo Pull Request.
