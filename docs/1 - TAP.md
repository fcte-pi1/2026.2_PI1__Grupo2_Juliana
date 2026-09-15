# **Termo de Abertura do Projeto (TAP)**

*Projeto Integrador 1 - Micromouse*

## **Visão Geral do Projeto**

### **Dados do projeto**

  - **Nome do Projeto:** Projeto Integrador 1 - Micromouse 2026.2
  - **Data de Início:** 10/08/2026
  - **Data de Término:** 04/12/2026
  - **Patrocinador:** Universidade de Brasília
  - **Orientador:** Juliana Petrocchi Rodrigues 

  

## **Objetivos**

*O que o grupo pretende obter com a realização do projeto (Regra SMART):*

  

  - **Specific (específico):** Planejar e construir um autômato no estilo Micromouse que possua dimensões máximas de 16,5 cm de largura ou comprimento. O projeto deverá ser capaz de resolver três labirintos de tamanhos 4x4; 8x4 ;12x4 células (com cada célula sendo de 18 cm medindo 5 cm de altura) de configuração sortida. Os labirintos navegáveis pelo Micromouse serão ambientes de paredes brancas e chão preto. O projeto deverá partir de uma posição x do labirinto (dentro de um beco sem saída) e encontrar a saída de forma totalmente autônoma. Além disso, o projeto deverá possuir uma divisão de monitoramento, sendo um software que exiba os dados de telemetria do robô para suas corridas, como tempo de conclusão, consumo médio e trajeto percorrido. 
  - **Measurable (mensurável):** O desempenho do projeto será quantificado por meio de indicadores objetivos definidos pela disciplina: número de tentativas até a resolução de cada labirinto (convertido em nota de 0 a 10) e presença ou não de exibição de telemetria em tempo real no sistema web. A partir desses indicadores, a equipe define como meta interna resolver os três labirintos na primeira tentativa e entregar o sistema web de monitoramento (trajetória, consumo, velocidade, tempo de conclusão) plenamente funcional, garantindo assim a maior pontuação possível dentro do critério de avaliação estabelecido .
  - **Agreed (acordado):** Os objetivos e requisitos do projeto serão estabelecidos e desenvolvidos de forma alinhada entre os integrantes da equipe e a orientadora, Professora Juliana P. Rodrigues, considerando as especificações e restrições definidas para o projeto e as regras aplicáveis às competições acadêmicas de Micromouse. Esse alinhamento será mantido ao longo do semestre por meio das reuniões realizadas durante as aulas práticas, que contemplam momentos de revisão, retrospectiva e planejamento. Essas reuniões permitirão acompanhar a execução das atividades, discutir o andamento do projeto e realizar os ajustes necessários no planejamento.
  - **Realistic (realista):** O projeto é exequível dentro do prazo estabelecido e da capacidade técnica da equipe multidisciplinar, composta por estudantes de Engenharia Automotiva, de Software e de Energia. Do ponto de vista financeiro, a viabilidade é garantida por um orçamento total estimado em R$876,41, que, ao ser rateado entre os 1 membros da equipe, resulta em um investimento de aproximadamente R$ 54,77 por integrante, valor que ainda pode ser reduzido por meio de apoios e patrocínios internos como o aproveitamento de materiais e sobras de equipes de competição, a cessão de componentes por colegas e a utilização da infraestrutura e equipamentos já disponíveis na universidade. Essas estratégias mantêm os custos em um patamar sustentável para o desenvolvimento estudantil.  No que diz respeito ao tempo, a conclusão dentro do prazo estipulado é viabilizada pelo cronograma elaborado em fases sequenciais, pela divisão de tarefas conforme a habilidade dos integrantes e pelo acompanhamento do andamento pelo GitHub, estruturação que facilita a mitigação antecipada de gargalos. 
  - **Time Bound (Limitado no tempo):** O projeto tem início em 10/08/2026 e deve ser concluído até 04/12/2026, passando pelas etapas de projeto conceitual (estruturas, energia, hardware e software), testes por subsistema e testes de integração, culminando na apresentação final com a resolução autônoma dos três labirintos e o sistema web de telemetria integrado e funcional até essa data. 

  

## **Público-Alvo**

O público-alvo deste projeto abrange diversos agentes tanto internos quanto externos, que se beneficiam do processo de desenvolvimento robótico e dos resultados fornecidos pelo experimento. No âmbito interno, pode-se citar os próprios estudantes, que desfrutam do aprendizado extenso que envolve programação, estrutura, eletrônica embarcada e etc. Por outro lado, podemos citar de maneira externa o interesse de diferentes alunos e também de diferentes instituições de ensino, que poderão utilizar os esquemas, códigos e relatórios como fonte de pesquisa para projetos futuros ou atuais. Por fim, a comunidade principal a ser atingida por este projeto tende a ser a comunidade robótica educacional e competitiva, que pode visar o compartilhamento de soluções construtivas, estratégias de navegação em labirinto diferentes e boas práticas.

  

## **Descrição do Problema**

O projeto busca solucionar o problema de navegação autônoma de um robô em ambientes desconhecidos, nos quais não são conhecidos os obstáculos e caminhos antes do início da execução. Sendo assim, se torna necessário desenvolver um sistema que seja capaz de identificar o ambiente ao seu redor, determinar sua própria localização, mapear os caminhos disponíveis e tomar decisões de deslocamento de forma autônoma, sem intervenção humana.

O desafio consiste no desenvolvimento de um micromouse capaz de percorrer diferentes configurações de labirintos, partindo de um ponto inicial e alcançando uma área de destino final no canto oposto. Como a disposição das paredes não será conhecida previamente pelas equipes, o robô deve utilizar sensores, mecanismos de controle, e algoritmos de navegação para detectar obstáculos e definir uma rota adequada durante seu deslocamento. 

Além da movimentação autônoma, existe a necessidade de acompanhar e registrar o desempenho do sistema, para isso, o projeto deve ter uma aplicação web, com informações de telemetria em tempo real, como: trajeto realizado, consumo de bateria, velocidade média, tempo de conclusão e indicação de conclusão do desafio. Após a execução, esses dados também devem ser armazenados em um banco de dados, que permite consultas posteriores relacionadas a cada desafio realizado. 

Dessa forma, o problema abordado pelo projeto envolve a integração de diferentes áreas da engenharia para construir uma solução completa de robótica móvel autônoma, sensoriamento, controle, processamento de dados e monitoramento por software, permitindo assim que o micromouse reconheça e percorra ambientes desconhecidos de maneira eficiente e confiável. 

  

## **Indicadores**

*Listar até 10 indicadores que determinam o mercado consumidor do produto desenvolvido:*

  

1.  Crescimento acelerado do evento: na 4ª edição (2025), o RoboChallenge reuniu 390 robôs e 650 participantes, quase o dobro da edição anterior em apenas um ano. (Fontes: [Diário do Grande ABC, 2025](https://www.folhavitoria.com.br/geral/instituto-maua-de-tecnologia-realiza-3-edicao-do-robochallenge-brasil/); [Diário do Grande ABC, 2024](https://www.folhavitoria.com.br/geral/instituto-maua-de-tecnologia-realiza-3-edicao-do-robochallenge-brasil/))
2.  Alcance direto na FCTE/UnB: 182 estudantes estão matriculados em PI1 no semestre 2026/2 (distribuídos entre as 5 turmas), evidenciando a escala do público que passa por este tipo de desafio a cada semestre. (Fonte: [Consultas de Turmas do SIGAA](https://sigaa.unb.br/sigaa/public/turmas/listar.jsf))
3.  Padrão de tempo por tentativa já é internacional: competições oficiais (regras IEEE MicroMouse) adotam o mesmo limite de 10 minutos por tentativa/labirinto, o que permite usar esse valor como métrica comparável de desempenho. (Fonte:[ IEEE MicroMouse Rules 2020](https://attend.ieee.org/r2sac-2020/wp-content/uploads/sites/175/2020/01/MicroMouse_Rules_2020.pdf)) 
4.  O custo de componentes básicos é baixo no Brasil: placas de microcontrolador com WiFi (classe ESP32/ESP8266) custam entre R$ 30,00 e R$ 55,00 no mercado nacional, tornando a montagem de um protótipo inicial financeiramente acessível para uma equipe estudantil. (Fonte:[ Mercado Livre](https://lista.mercadolivre.com.br/placa-automacao-esp)) 
5.  O cenário do Micromouse e da robótica autônoma no Brasil expandiu-se nacionalmente, deixando de ser exclusividade do eixo Sul-Sudeste. Sendo assim, é válido destacar a distribuição de densidade de equipes ao longo do Brasil. Embora São Paulo (com destaque para IMT e USP) e Minas Gerais (UNIFEI) concentrem a maior densidade e tradição de equipes de elite, polos de excelência no Centro-Oeste (UnB) e no Sul (UTFPR, UDESC) demonstram a robustez tecnológica do país. Além disso, destaca-se a crescente e vitoriosa participação de alunos do ensino básico e médio em competições de alto nível, evidenciando a popularização precoce da área. (Fonte: [noticias.unb](https://noticias.unb.br/ensino/6927-unb-leva-podio-na-competicao-brasileira-de-robotica-2023) ; [events.robocore.net](https://events.robocore.net/))
6.  Para mitigar as barreiras econômicas de entrada, que antecedem as barreiras econômicas de permanência em competições e o custeamento dos robôs, as organizações evidenciam que a política adotada visa claramente a mitigação de custos e o incentivo à antecipação logística, de acordo com uma análise dos boletins de inscrição oficiais para os ciclos de 2025 e 2026 das principais competições O custo de participação não atua como um estrangulador, mas sim como um mecanismo regulador. 

| Nome do Evento Competitivo | Entidade Patrocinadora/Anfitriã | Categoria Analisada | Período e Escalão de Registo | Taxa de Inscrição Oficial (BRL) |
| :--- | :--- | :--- | :--- | :--- |
| RoboChallenge Brasil 2025 | Instituto Mauá de Tecnologia | Micromouse Classic | Nível 1 (Até 28 de Agosto) | R$ 50,00 |
| RoboChallenge Brasil 2025 | Instituto Mauá de Tecnologia | Micromouse Classic | Nível 2 (Até 19 de Setembro) | R$ 80,00 |
| RoboCore Experience 2026 | RoboCore e IMT | Micromouse Classic | 1º Lote (Até 14 de Julho) | R$ 70,00 |
| RoboCore Experience 2026 | RoboCore e IMT | Micromouse Classic | 2º Lote (Até 11 de Agosto) | R$ 100,00 |
| RoboCore Experience 2026 | RoboCore e IMT | Micromouse Classic | 3º Lote (Até 07 de Setembro) | R$ 130,00 |
| RoboCore Experience 2026 | RoboCore e IMT | Micromouse Classic | 4º Lote (Até 30 de Setembro) | R$ 160,00 |

(Fontes: [robocore.net](https://events.robocore.net/rcx-2026/registration_info))

7.  As competições de MicroMouse são eventos em escala internacional que proporcionam oportunidades únicas para seus competidores. Em algumas categorias, os campeões podem concorrer a vagas de hospedagem e inscrição em competições ainda maiores hospedadas em outros países. (Fonte:[CBR RoboCup](https://cbr.robocup.org.br/index.php/2026/05/03/joao-pessoa-recebe-a-competicao-brasileira-de-robotica-petrobras-2026/) ; [RSM Challenge Internacional 2026](https://www.youtube.com/watch?v=j0gGozoHf5o)) 
8.  Em regras da competição CAMM (Canadian/All-American Micromouse), o custo total dos materiais utilizados na construção do Micromouse não pode ultrapassar US$ 500, sendo considerado inclusive o valor de mercado de materiais doados. A regra exige que os competidores mantenham a descrição dos componentes e seus respectivos preços para eventual verificação pelos juízes. Esse valor fornece uma referência internacional para o custo máximo de construção de um Micromouse competitivo (Fonte: [CAMM - Regras](http://micromouseusa.com/wp-content/uploads/2016/04/CAMM2016Rules.pdf))

  

## **Membros da Equipe**

| Nome | Matrícula | Curso | E-mail | Função |
| :--- | :--- | :--- | :--- | :--- |
| Ana Beatriz | 241025130 | Engenharia Automotiva | anabeatriz2013cs@gmail.com  | Time de Estruturas |
| Arthur Vinicius Morais de Lima  | 242015764  | Engenharia de Software  | arthurhc3@gmail.com  | Time de Eletrônica  |
| Cecília Costa Rebelo Cunha  | 232001415  | Engenharia de Software  | cecilia.cunha2004@gmail.com  | Time de Software - Front-end (Gerente Geral)  |
| Daniel Ferreira Nunes  | 211061565  | Engenharia de software  | danielferreiranunes2003@gmail.com  | Time de Estruturas  |
| Davi dos Santos Brito Nobre  | 211062929  | Engenharia de Software  | davinobre.ik@gmail.com  | Time de Energia  |
| Diego Godoi Rodrigues  | 251019806  | Engenharia de Software  | godoir.diego@gmail.com  | Time de Eletrônica  |
| Eduardo Ferreira de Aquino  | 211030710  | Engenharia de Software  | fxr.ed03@gmail.com  | Time de Eletrônica  |
| Eduardo Orsomarso Oliveira  | 232001970  | Engenharia de Energia  | eduardo.orsomarso@gmail.com  | Time de Energia  |
| Geovana Duarte de Carvalho  | 251021241  | Engenharia de Software  | geovanaduarteunb@gmail.com  | Time de Software - Back-end  |
| Gustavo Costa de Jesus  | 211061814  | Engenharia de software  | gucosta1719@gmail.com  | Time de Estruturas (Sub-gerente)  |
| José Eduardo Vieira do Prado  | 221008202  | Engenharia de Software  | jevrprado@gmail.com  | Time de Software - Front-end  |
| Letícia de Cássia Hladczuk Rodrigues  | 221039209  | Engenharia de Software  | leticia.cassia.hr@gmail.com  | Time de Software - Back-end  |
| Lucca Medeiros Silva  | 222031528  | Engenharia de Software  | dev.luccameds@gmail.com  | Time de Software - Back-end |
| Luis Guilherme de Almeida Costa  | 251037401  | Engenharia de Software  | failho42@gmail.com  | Time de Eletrônica (Sub-Gerente)  |
| Marcella Sousa Anderle  | 221035040  | Engenharia de Software  | marcellasanderle@gmail.com  | Time de Software - Back-end |
| Marcelo de Araújo Lopes  | 211062179  | Engenharia de Software  | matielloaraujolopes@gmail.com  | Time de Software - Front-end (Sub-Gerente) |
| Maria Luana Soares Lopes  | 241011448  | Engenharia de Software  | marialuana.sl962@gmail.com  | Time de Eletrônica  |


## **Orçamento estimado**

**Total:** R$ 876,41

| Categoria | Componente | Finalidade | Qtd | Valor | Loja/Site | Links |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Microcontrolador | ESP32 DevKit (30 pinos) | Cérebro do projeto | 3 | 35,35 | Mercado livre | [Link](https://www.mercadolivre.com.br/esp32-devkit-modulo-microcontrolador-wroom32-memoria-placa/up/MLBU3867919063) |
| Sensores | Módulos Laser ToF VL53L0X | Sensor | 3 | 33,16 | Mercado livre | [Link](https://www.mercadolivre.com.br/modulo-sensor-de-distancia-vl53l0x-gy-530-de-alta-precisao/p/MLB46068401) |
| Motores e Tração | Micromotores N20 com encoders | Movimentação. | 3 | 38,7 | Mercado livre | [Link](https://www.mercadolivre.com.br/mini-motor-com-reducao-3v-5v-6v-n20-de-8-a-200-rpm/up/MLBU3644827993) |
| Motores e Tração | Suportes em L | Movimentação. | 0 | 0 | Impressão 3D | Apoio/Patrocínio |
| Rodas e Apoio | Rodas N20 emborrachadas | Suporte para o movimento. | 2 | 24,65 | Mercado Livre | [Link](https://www.mercadolivre.com.br/2x-roda-34mm-para-micro-motor-dc-12v-n20-robo/up/MLBU664879255) |
| Rodas e Apoio | Esfera deslizante | Suporte para o movimento. | 1 | 17,9 | Amazon | [Link](https://www.amazon.com.br/Kit-Rodinhas-Adesivas-Deslizantes-Organizadoras/dp/B0G7LDTXX3) |
| Driver de Motor | Módulo DRV8833 (ou TB6612FNG) | Intermediador entre o cérebro e os motores. | 1 | 26,83 | Mercado Livre | [Link](https://www.mercadolivre.com.br/modulo-de-driver-de-motor-duplo-drv8833-15a-saida-pwm-spee/p/MLB2071552193) |
| Bateria e Carregador | LIPO 2S (7.4V) | Armazenar Energia. | 1 | 145,4 | Mercado Livre | [Link](https://www.mercadolivre.com.br/bateria-lipo-2s-74v-1500-mah-30c-automodelo-wltoys-conquer/up/MLBU3343504044) |
| Regulação e Chave | Módulo Step-Down (MP1584 ou Mini360) + 2x Chave liga/desliga | Regular tensões. | 2 | 19,2 | Mercado Livre | [Link](https://www.mercadolivre.com.br/conversor-step-down-de-tensao-dcdc-com-mp1584en/p/MLB32918616) |
| Conexões e Placa | Placa ilhada (perfboard), barras de pinos fêmea e fios jumpers | Atua como a placa que reúne todos os componentes. | 2 | 14,11 | Mercado Livre | [Link](https://www.mercadolivre.com.br/pcb-placa-fibra-vidro-perfurada-3x7-cm-dupla-face-nfe/up/MLBU1460697141) |
| Passivos e Botões | Resistores (330 Ω e 10 kΩ) e botões tácteis | 2 a 3 botões tácteis e 5 a 8 resistores. | 0 | 11 | Mercado Livre | [Link](https://www.mercadolivre.com.br/kit-10-x-resistor-330-ohm-14w-1-projetos-arduino-raspberry/up/MLBU1754395549) |
| Chassi Mecânico | Acrílico/MDF ou impressão 3D  | Corpo do chassi. | 2 | 19,72 | Mercado Livre | [Link](https://www.mercadolivre.com.br/placa-acrilico-transparente-8x8-cm-8x8-c-3mm/up/MLBU1157027232) |
| Chassi Mecânico | Parafusos e porcas M2/M3 | Fixar as partes ao corpo. | 1 | 38,72 | Amazon | [Link](https://www.amazon.com.br/parafusos-sextavados-inoxid%C3%A1vel-ferragens-eletr%C3%B4nicos/dp/B0G3473KM6) |
| Testes | MDF 72x72cm^2 (base) | Labirinto de testes. | 1 | 64,57 | Mercado Livre | [Link](https://www.bing.com/shop/productdetails?goid=445334455376) |
| Testes | EVA 15mm (paredes) | Labirinto de testes. | 1 | 69 | Mercado Livre | [Link](https://www.mercadolivre.com.br/kit-10-placas-eva-preto-35-cm-x-27cm-x-15-mm-15cm/up/MLBU596320757) |
| Testes | Impressão 3D (suporte das paredes) | Labirinto de testes. | 50 | 0 | Apoio | Apoio/Patrocínio |

## **Duração estimada (horas)**

| Fase | Área | Duração |
| :--- | :--- | :--- |
| Projeto Conceitual | Estruturas | 80 h |
| Projeto Conceitual | Eletrônica | 100 h |
| Projeto Conceitual | Energia | 60 h |
| Projeto Conceitual | Software | 140 h |
| Testes por Subsistema | Estruturas | 50 h |
| Testes por Subsistema | Eletrônica | 60 h |
| Testes por Subsistema | Energia | 40 h |
| Testes por Subsistema | Software | 90 h |
| Testes de Integração | Todos | 190 h |
| **Total:** | **-** | **810 h** |































