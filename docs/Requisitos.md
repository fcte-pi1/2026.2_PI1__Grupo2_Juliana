# Requisitos

Este documento formaliza as especificações técnicas levantadas para o desenvolvimento do robô Micromouse e de seu sistema web de telemetria, definindo o que o sistema deve fazer e sob quais restrições operará.

Abaixo, os requisitos estão divididos pelas quatro frentes do projeto (Software, Eletrônica, Energia e Estruturas) e classificados em duas categorias:

* **Requisitos Funcionais (RF):** Descrevem as ações e comportamentos que o sistema deve ser capaz de realizar (ex: mapeamento autônomo, detecção de paredes).
* **Requisitos Não-Funcionais (RNF):** Estabelecem as restrições e os critérios de qualidade do projeto de forma objetiva e mensurável (ex: limite dimensional de 16,5 cm, resposta do algoritmo em milissegundos).

Para guiar o cronograma de execução, cada item foi priorizado utilizando a metodologia **MoSCoW** (*Must have*, *Should have*, *Could have*), e possui link direto para a *issue* de rastreabilidade correspondente no painel de gerenciamento do GitHub.

---

## Área de Software

**Sub-gerente:** Marcelo de Araújo Lopes

### Requisitos Funcionais (RF)

| ID | Nome do Requisito | Descrição | Prioridade | Responsáveis | Link Github Projects |
| :---: | :--- | :--- | :---: | :--- | :--- |
| RF01-S | Detecção de Paredes | Detectar paredes nas quatro direções da célula atual a partir da leitura dos sensores | Must have | Cecília e Marcelo | [RF01-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/78) |
| RF02-S | Registro de Célula e Orientação | Manter registro da célula e da orientação atuais ao longo de todo o percurso | Must have | Letícia, Lucca e Marcella | [RF02-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/84) |
| RF03-S | Movimentação Célula a Célula | Movimentar-se célula a célula de forma alinhada, sem colisão com as paredes | Must have | Cecília e Marcelo | [RF03-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/86) |
| RF04-S | Identificação do Objetivo | Identificar autonomamente a chegada ao objetivo e encerrar a corrida | Must have | Cecília e Marcelo | [RF04-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/87) |
| RF05-S | Transmissão de Telemetria | Formatar e enviar os dados de execução do micromouse via protocolo de comunicação para o sistema web | Must have | Geovana e José Eduardo | [RF05-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/88) |
| RF06-S | Exibição de Telemetria em Tempo Real | Exibir no sistema web dados do percurso, bateria, velocidade e status em tempo real | Must have | Cecília e Marcelo | [RF06-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/89) |
| RF07-S | Persistência dos Dados Finais | Armazenar em banco de dados os dados da corrida ao final de cada tentativa/desafio | Must have | Geovana e José Eduardo | [RF07-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/90) |
| RF08-S | Consulta por Labirinto | Permitir consultar os dados armazenados filtrando por um labirinto específico | Must have | Geovana e José Eduardo | [RF08-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/91) |
| RF09-S | Consulta Geral | Permitir consultar os dados de todos os labirintos de uma vez | Should have | Letícia, Lucca e Marcella | [RF09-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/92) |
| RF10-S | Controle de Tentativas | Registrar e contabilizar o número de tentativas por labirinto | Must have | Cecília e Marcelo | [RF10-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/93) |
| RF11-S | Algoritmo de Mapeamento do Labirinto | Construir o mapa do labirinto dinamicamente durante a execução. O sistema deve explorar e mapear as células de forma autônoma utilizando um algoritmo eficiente para armazenar a configuração, sem necessitar de um mapa pré-carregado. | Must have | Geovana e José Eduardo | [RF11-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/94) |
| RF12-S | Cálculo de Menor Caminho | Processar o mapa acumulado para calcular a rota ideal/mais curta até o Final do labirinto | Must have | Geovana e José Eduardo | [RF12-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/95) |
| RF13-S | Modo automático | Realizar percurso otimizado com velocidade utilizando o menor caminho previamente calculado | Should have | Letícia, Lucca e Marcella | [RF13-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/96) |
| RF14-S | Filtro de Ruído nos Sensores | Aplicar tratamento digital aos dados brutos dos sensores para mitigar leituras falsas | Must have | Letícia, Lucca e Marcella | [RF14-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/97) |
| RF15-S | Controle em Malha Fechada (PID) | Controlar os motores utilizando controle proporcional-integral-derivativo para alinhamento | Must have | Geovana e José Eduardo | [RF15-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/98) |
| RF16-S | Recuperação de Erros de Deslocamento | Detectar e corrigir desvios de alinhamento ou pequenas colisões durante o trajeto | Should have | Letícia, Lucca e Marcella | [RF16-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/99) |
| RF17-S | Calibração Inicial Automática | Executar rotina de calibração de sensores de distância e encoders ao inicializar | Should have | Letícia, Lucca e Marcella | [RF17-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/100) |
| RF18-S | Exportação de Relatórios | Permitir exportar relatórios de desempenho e histórico de corridas (ex: PDF ou CSV) | Could have | Letícia, Lucca e Marcella | [RF18-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/101) |
| RF19-S | Indicadores de Status de Bateria e Conexão | Exibir alertas visuais no sistema web sobre nível crítico de bateria ou perda de conexão | Must have | Cecília e Marcelo | [RF19-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/102) |
| RF20-S | Log de Eventos e Erros | Registrar histórico de logs de sistema para depuração de erros de software e hardware | Should have | Geovana e José Eduardo | [RF20-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/103) |
| RF21-S | Autenticação de Usuários no Sistema Web | Restringir acesso às funcionalidades de controle e histórico mediante login | Could have | Cecília e Marcelo | [RF21-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/104) |
| RF22-S | Atualização de Firmware via OTA (Over-The-Air) | Suportar atualização do software do robô sem fio através da rede | Could have | Geovana e José Eduardo | [RF22-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/105) |
| RF23-S | Partida em Beco sem Saída | Iniciar a execução a partir de uma posição interna a um beco sem saída, sem intervenção externa | Must have | Letícia, Lucca e Marcella | [RF23-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/106) |
| RF24-S | Validação de Pacotes de Telemetria | Validar a integridade dos pacotes de telemetria recebidos antes de persistir, descartando e registrando os pacotes corrompidos | Must have | Geovana e José Eduardo | [RF24-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/107) |
| RF25-S | Armazenamento Local de Telemetria | Armazenar na memória local do micromouse os dados de telemetria ainda não confirmados pelo servidor, com capacidade suficiente para uma corrida completa no maior, e transmiti-los assim que a conexão for restabelecida | Must have | Letícia, Lucca e Marcella | [RF25-S](https://github.com/fcte-pi1/2026.2_PI1__Grupo2_Juliana/issues/108) |

### Requisitos Não-Funcionais (RNF)

| ID | Nome do Requisito | Descrição | Prioridade | Responsáveis | Link Github Projects |
| :---: | :--- | :--- | :---: | :--- | :--- |
| RNF01-S | Tempo de Resposta do Algoritmo | O ciclo completo de leitura de sensores, decisão e envio de comando aos motores deve ser concluído em no máximo 50 ms, considerando até 20 ms de latência de leitura (Categoria: Eficiência de desempenho) | Must have | Letícia, Lucca e Marcella | [Colar Link Aqui] |
| RNF02-S | Confiabilidade de Navegação | O robô deve concluir no mínimo 90% das execuções de teste sem colisão, medido sobre pelo menos 10 execuções consecutivas em cada uma das três configurações de labirinto (Categoria: Confiabilidade) | Must have | Geovana e José Eduardo | [Colar Link Aqui] |
| RNF03-S | Uso Eficiente de Memória | O firmware deve utilizar no máximo 80% da RAM e da Flash disponíveis no microcontrolador (Categoria: Eficiência de desempenho) | Must have | Geovana e José Eduardo | [Colar Link Aqui] |
| RNF04-S | Disponibilidade do Sistema Web | O sistema web deve permanecer operante durante 100% da janela de execução de cada corrida, e se recuperar de uma queda em até 30 segundos (Categoria: Confiabilidade) | Must have | Letícia, Lucca e Marcella | [Colar Link Aqui] |
| RNF05-S | Usabilidade da Interface | Um avaliador sem treinamento prévio na interface deve identificar o status da corrida em até 5 segundos (Categoria: Usabilidade) | Should have | Cecília e Marcelo | [Colar Link Aqui] |
| RNF06-S | Latência de Telemetria | A latência entre a captura do dado no micromouse e sua exibição na interface deve ser de no máximo 500 ms, medida por comparação entre o timestamp de origem e o de renderização (Categoria: Eficiência de desempenho) | Must have | Geovana e José Eduardo | [Colar Link Aqui] |
| RNF07-S | Integridade dos Dados Armazenados | 100% das corridas concluídas devem estar íntegras na consulta posterior, com verificação por checksum na recepção dos pacotes (Categoria: Confiabilidade) | Must have | Letícia, Lucca e Marcella | [Colar Link Aqui] |
| RNF08-S | Manutenibilidade | O código deve ser modular, documentado e versionado no repositório do grupo seguindo a estrutura src/firmware, src/backend e src/frontend (Categoria: Manutenibilidade) | Must have | Geovana e José Eduardo | [Colar Link Aqui] |
| RNF09-S | Portabilidade do Código | Estrutura de software independente de plataforma específica para facilitar reuso (Categoria: Portabilidade) | Should have | Geovana e José Eduardo | [Colar Link Aqui] |
| RNF10-S | Testabilidade | Arquitetura que permite execução de testes unitários automatizados nos algoritmos principais (Categoria: Manutenibilidade) | Should have | Letícia, Lucca e Marcella | [Colar Link Aqui] |
| RNF11-S | Design Responsivo | A interface web deve ser legível e funcional (Categoria: Usabilidade) | Should have | Cecília e Marcelo | [Colar Link Aqui] |
| RNF12-S | Tolerância a Falhas na Comunicação | O sistema deve detectar a queda da conexão, reconectar automaticamente e retransmitir os pacotes armazenados localmente conforme RF26-S, sem perda de dados da corrida | Must have | Geovana e José Eduardo | [Colar Link Aqui] |
| RNF13-S | Concorrência e Escala de Conexões | O servidor web deve suportar até 5 clientes conectados simultaneamente sem degradação da atualização em tempo real (Categoria: Eficiência de desempenho) | Could have | Letícia, Lucca e Marcella | [Colar Link Aqui] |
| RNF14-S | Segurança na Comunicação | Proteção básica dos pacotes de dados transmitidos entre o robô e o servidor web (Categoria: Segurança) | Could have | Letícia, Lucca e Marcella | [Colar Link Aqui] |
| RNF15-S | Independência de Conexão na Navegação | A perda de conexão com o servidor não pode interromper, pausar ou alterar a navegação do robô (Categoria: Confiabilidade) | Must have | Letícia, Lucca e Marcella | [Colar Link Aqui] |

---

## Área de Eletrônica

**Sub-gerente:** Luis Guilherme de Almeida Costa

### Requisitos Funcionais (RF)

| ID | Nome do Requisito | Descrição | Prioridade | Responsáveis | Link Github Projects |
| :---: | :--- | :--- | :---: | :--- | :--- |
| RF01 | Sensoriamento de Distância das Paredes | O subsistema de sensoriamento deve medir a distância das paredes frontais e laterais (esquerda e direita) utilizando os sensores ou conversão analógico-digital de até 3,3 V. | Must have | Luis Guilherme, Eduardo Ferreira | [Colar Link Aqui] |
| RF02 | Regulação de Tensão | O regulador Buck deve converter a faixa de tensão da bateria (7,4V a 8,4V) para 5V regulados na entrada VIN do microcontrolador e barramentos. | Must have | Maria Luana, Diego Godoi | [Colar Link Aqui] |
| RF03 | Comunicação de Telemetria Sem Fio | O subsistema de RF integrado (Wi-Fi/Bluetooth do ESP32) deve transmitir dados telemétricos para uma estação de controle remota. | Must have | Arthur Vinicius, Luis Guilherme | [Colar Link Aqui] |
| RF04 | Controle Independente de Motores | O circuito de potência (ponte H) deve acionar individualmente cada um dos dois motores de tração nos sentidos horário e anti-horário com variação contínua de tensão média por sinais PWM de 3,3 V provenientes do microcontrolador. | Must have | Diego Godoi, Eduardo Ferreira, Maria Luana | [Colar Link Aqui] |
| RF05 | Controle do Giroscópio com corretude | O módulo de giroscópio precisa possibilitar curvas acuradas de 90° e 180° com baixa margem de erro. | Must have | Arthur Vinicius, Diego Godoi | [Colar Link Aqui] |
| RF06 | Velocidade Instantânea e Encoders | O robô precisa ser capaz de calcular a velocidade instantânea dele por meio dos encoders existentes. | Should have | Maria Luana, Luis Guilherme | [Colar Link Aqui] |
| RF07 | Seccionamento Elétrico Geral | O circuito deve conter um interruptor eletromecânico manual (chave gangorra/deslizante) no barramento positivo primário para desenergização total imediata da placa. | Must have | Arthur Vinicius, Eduardo Ferreira | [Colar Link Aqui] |

### Requisitos Não-Funcionais (RNF)

| ID | Nome do Requisito | Descrição | Prioridade | Responsáveis | Link Github Projects |
| :---: | :--- | :--- | :---: | :--- | :--- |
| RNF01 | Compatibilidade de Nível Lógico | Todos os sinais que chegam às portas GPIO e conversores ADC do microcontrolador devem operar estritamente dentro da faixa segura de 0V a 3,3V, sob risco de danos ao silício do ESP32. | Must have | Arthur Vinicius, Luis Guilherme | [Colar Link Aqui] |
| RNF02 | Tempo de Resposta do Sensoriamento | A latência total de aquisição e transmissão das leituras de distância dos sensores para os registradores da CPU não deve ultrapassar 20 ms por canal. | Should have | Maria Luana, Eduardo Ferreira | [Colar Link Aqui] |
| RNF03 | Estabilidade Elétrica | O circuito deve possuir capacitores de desacoplamento e plano de terra unificado suficientes para atenuar ruídos eletromagnéticos de chaveamento dos motores e evitar quedas bruscas de tensão que provoquem reinicializações (brownout reset) na CPU. | Should have | Diego Godoy, Arthur Vinicius | [Colar Link Aqui] |
| RNF04 | Restrição Dimensional | A disposição física dos componentes, módulos, conectores e da fiação/trilhas não pode fazer o robô exceder os limites mecânicos regulamentares de 16,5 x 16,5 cm, devendo caber com folga nas células do labirinto 18 x 18 cm. | Must have | Diego Godoy, Arthur Vinicius, Maria Luana | [Colar Link Aqui] |
| RNF05 | Ausência de Conflitos em Barramentos | Na microcontroladora ESP32, não devem haver conflitos de pinos ou barramentos. | Must have | Maria Luana, Luis Guilherme | [Colar Link Aqui] |
| RNF06 | Posicionamento para Precisão de Leituras | O módulo do giroscópio precisa ser posicionado ao centro do eixo das rodas do robô para evitar registros com pouca precisão. | Must have | Maria Luana, Diego Godoy | [Colar Link Aqui] |

---

## Área de Energia

**Sub-gerente:** Eduardo Orsomarso Oliveira

### Requisitos Funcionais (RF)

| ID | Nome do Requisito | Descrição | Prioridade | Responsáveis | Link Github Projects |
| :---: | :--- | :--- | :---: | :--- | :--- |
| RF-01-EN | Alimentação Principal | Fornecer energia ao robô a partir de bateria, com tensão nominal de 7,4 V e máxima de 8,4 V, dentro dos limites aceitos pelo subsistema de Eletrônica | Must have | Davi Nobre | [Colar Link Aqui] |
| RF-02-EN | Monitoramento de tensão da bateria | Medir continuamente a tensão da bateria em interação com o microcontrolador para proteção contra a descargas perigosas ao equipamento | Must have | Eduardo Oliveira | [Colar Link Aqui] |
| RF-03-EN | Monitorar corrente | Medir o gasto total do micromouse para que o time de software possa estimar o consumo da bateria | Should have | Davi Nobre | [Colar Link Aqui] |
| RF-04-EN | Proteção da bateria | Implementar proteções contra sobretensão, subtensão, sobrecorrente, curto-circuito, super-aquecimento e inversão de polaridade caso os padrões de qualidade do fabricante se mostrem insuficientes | Must have | Eduardo Oliveira | [Colar Link Aqui] |
| RF-05-EN | Controle de alimentação do sistema | Distribuir a energia para os subsistemas e permitir ligar/desligar motores e eletrônica por chave geral e/ou chaveamento controlado | Must have | Davi Nobre | [Colar Link Aqui] |
| RF-06-EN | Indicação de carga/estado | Fornecer alguma indicação visual (ex: led) para que a equipe saiba se está carregando, bateria cheia, carga média ou descarregando | Must have | Eduardo Oliveira | [Colar Link Aqui] |

### Requisitos Não-Funcionais (RNF)

| ID | Nome do Requisito | Descrição | Prioridade | Responsáveis | Link Github Projects |
| :---: | :--- | :--- | :---: | :--- | :--- |
| RNF-01-EN | Autonomia mínima | O sistema deve garantir autonomia mínima de 30 minutos em operação convencional. | Must have | Davi Nobre | [Colar Link Aqui] |
| RNF-02-EN | Segurança térmica | Bateria, reguladores e chaveamento não devem passar dos 55° graus em operação. Se ultrapassar, o sistema deve emitir um aviso e encerrar as operações imediatamente | Must have | Eduardo Oliveira | [Colar Link Aqui] |
| RNF-03-EN | Medições | A medição de tensão da bateria deve ter erro máximo de ±1% ou ±50 mV; a medição de corrente, erro máximo de ±5% | Should have | Davi Nobre | [Colar Link Aqui] |
| RNF-04-EN | Medições de peso e composição da bateria | A bateria do robô deve ter um teto máximo de 80g, bem como a composição de Lítio-polímero | Must have | Eduardo Oliveira | [Colar Link Aqui] |

---

## Área de Estruturas

**Sub-gerente:** Gustavo Costa de Jesus

### Requisitos Funcionais (RF)

#### Chassi

| ID | Nome do Requisito | Descrição | Prioridade | Responsáveis | Link Github Projects |
| :---: | :--- | :--- | :---: | :--- | :--- |
| RF01-C | Alojamento e Proteção de Componentes Eletrônicos | O chassi deve possuir volume útil, área de base e geometria interna dimensionadas para acomodar, fixar e proteger fisicamente todos os componentes eletrônicos do sistema. | Must have | Ana Beatriz Carvalho | [Colar Link Aqui] |
| RF02-C | Interface de Fixação de Atuação e Rolagem | O chassi deve integrar pontos de fixação mecânica precisos (furações e encaixes) dedicados à montagem dos motores, das rodas motrizes e das esferas deslizantes. | Must have | Daniel Ferreira | [Colar Link Aqui] |
| RF03-C | Acessibilidade e Modularidade Estrutural | O chassi deve ter interfaces mecânicas padronizadas (como guias de alinhamento ou encaixes por pressão) para permitir a integração firme, rápida e modular entre as diferentes camadas, decks e componentes do robô nos locais corretos. | Should have | Ana Beatriz | [Colar Link Aqui] |
| RF04-C | Posicionamento e Visibilidade dos Sensores | O chassi deve possuir janelas, recortes ou aberturas para que a estrutura física não cause oclusão, reflexões parasitas ou obstrução do campo de visão dos sensores. | Must have | Gustavo | [Colar Link Aqui] |

#### Labirinto

| ID | Nome do Requisito | Descrição | Prioridade | Responsáveis | Link Github Projects |
| :---: | :--- | :--- | :---: | :--- | :--- |
| RF01-L | Padronização das Células | O labirinto deve ser composto por células modulares quadradas de 18 x 18 cm, delimitadas por paredes com espessura e altura padronizadas. | Must have | Ana Beatriz | [Colar Link Aqui] |
| RF02-L | Encaixe Modular das Paredes | As paredes devem possuir um sistema de encaixe rápido que permita alterar a configuração do labirinto sem ferramentas complexas. | Must have | Gustavo | [Colar Link Aqui] |
| RF03-L | Reconfiguração do Percurso | O labirinto deve permitir a criação de diferentes percursos por meio da adição, remoção ou reposicionamento das paredes. | Must have | Gustavo | [Colar Link Aqui] |
| RF04-L | Delimitação de Partida e Chegada | O labirinto deve permitir a identificação das células destinadas à partida e ao objetivo final do robô. | Must have | Daniel | [Colar Link Aqui] |
| RF05-L | Fixação das Paredes | O sistema de encaixe deve manter as paredes firmemente posicionadas durante a movimentação e possíveis colisões do robô. | Must have | Gustavo | [Colar Link Aqui] |

### Requisitos Não-Funcionais (RNF)

#### Chassi

| ID | Nome do Requisito | Descrição | Prioridade | Responsáveis | Link Github Projects |
| :---: | :--- | :--- | :---: | :--- | :--- |
| RNF01-C | Manobrabilidade | O chassi com todos os componentes montados (eletrônica, rodas e sensores) deve respeitar o envelope máximo de projeto estabelecido pelas normas de 16,5 x 16,5 cm. | Should have | Ana Beatriz | [Colar Link Aqui] |
| RNF02-C | Limite de massa e Eficiência Dinâmica | O chassi deve apresentar uma massa total descarregada inferior a 40g, enquanto o robô totalmente montado deve respeitar o limite estimado de 150g para otimizar a relação potência-peso fornecida pelos motores. | Must have | Gustavo | [Colar Link Aqui] |
| RNF03-C | Resistência Mecânica | O material do chassi deve ser leve e oferecer rigidez mecânica satisfatória para a fixação dos componentes sem adicionar peso indevido à estrutura, como o Acrílico ou material equivalente. | Must have | Gustavo | [Colar Link Aqui] |
| RNF04-C | Gerenciamento de Cabos | O chassi deve possuir canaletas, furos-guia ou pontos de passagem para fiação, evitando que cabos interfiram nas rodas, sensores ou movimentação do robô. | Could have | Daniel | [Colar Link Aqui] |
| RNF05-C | Tolerância de Fabricação | Os furos e encaixes de fixação do chassi devem manter tolerância dimensional suficiente para garantir o encaixe correto entre motores, rodas e placas eletrônicas. | Must have | Daniel | [Colar Link Aqui] |
| RNF06-C | Facilidade de Manutenção Mecânica | O chassi deve permitir a desmontagem e troca de peças danificadas ou componentes eletrônicos sem necessidade de desmontagem completa da estrutura. | Could have | Gustavo | [Colar Link Aqui] |

#### Labirinto

| ID | Nome do Requisito | Descrição | Prioridade | Responsáveis | Link Github Projects |
| :---: | :--- | :--- | :---: | :--- | :--- |
| RNF-L01 | Material das paredes | Deve ser feito de material rígido (MDF, PVC ou acrílico) que não deforme com o manuseio repetido. | Must have | Ana Beatriz | [Colar Link Aqui] |
| RNF-L02 | Peso máximo por peça | Cada célula/parede deve ter peso que permita montagem manual sem esforço excessivo. | Should have | Ana | [Colar Link Aqui] |
| RNF-L03 | Acabamento das superfícies | As paredes devem ter acabamento fosco/não reflexivo, para não interferir em sensores infravermelhos ou ópticos do robô. | Must have | Daniel | [Colar Link Aqui] |
| RNF-L04 | Resistência a colisões | O labirinto deve suportar múltiplas colisões do robô sem quebrar ou se deslocar permanentemente. | Must have | Ana | [Colar Link Aqui] |
| RNF-L05 | Tolerância dimensional | As células devem manter tolerância de ±1-2 mm nas dimensões, para não comprometer a navegação do robô. | Must have | Ana | [Colar Link Aqui] |
| RNF-L06 | Padrão de Piso | O piso do labirinto deve ter cor e coeficiente de atrito padronizados, garantindo tração consistente das rodas e leitura confiável de sensores ópticos/de reflectância. | Should have | Gustavo | [Colar Link Aqui] |
| RNF-L07 | Controle de Iluminação Ambiente | O labirinto deve ser operado sob condições de iluminação controlada/padronizada, minimizando interferência em sensores infravermelhos ou ópticos. | Should have | Daniel | [Colar Link Aqui] |
| RNF-L08 | Marcação de Células | As células do labirinto devem possuir identificação (numeração ou coordenadas) que facilite depuração, calibração e análise de logs de execução. | Could have | Daniel | [Colar Link Aqui] |