# Mecânica e Estruturas

Esta pasta reúne os artefatos do projeto mecânico e estrutural do Micromouse, incluindo desenhos técnicos, imagens do modelo CAD, registros dos componentes e evidências dos testes de fabricação e montagem.

## Organização da pasta

| Diretório | Conteúdo |
|---|---|
| [`componentes/`](./componentes/) | Imagens de referência das rodas e das alternativas de apoio do chassi. |
| [`desenhos-tecnicos/`](./desenhos-tecnicos/) | Desenhos dos três patamares do chassi, base e abraçadeira do suporte do motor, além da EBOM. |
| [`modelos-cad/`](./modelos-cad/) | Imagens renderizadas do modelo CAD, incluindo vistas completa, explodida, da base e da montagem atualizada. |
| [`testes/`](./testes/) | Fotografias usadas como evidências dos testes estruturais de inventário, medição, montagem, encaixe de componentes, pesagem, sensores e cabeamento. |
| [`documentos/`](./documentos/) | Índice dos documentos de projeto, testes estruturais e integração mantidos em `docs/`. |
| [`arquivos-cad/`](./arquivos-cad/) | Controle dos arquivos CAD editáveis ainda pendentes de inclusão. |
| [`impressao-3d/`](./impressao-3d/) | Controle dos modelos de impressão 3D ainda pendentes de inclusão. |

## Inventário atual

- 3 imagens de componentes e alternativas de apoio;
- 5 desenhos técnicos de peças estruturais;
- 1 EBOM do Micromouse;
- 5 imagens do modelo CAD;
- 30 fotografias de testes e da montagem física.

Os arquivos desta pasta foram organizados a partir dos artefatos estruturais já existentes no repositório. As cópias originais foram mantidas em `docs/figs/estruturas/` para preservar os links utilizados na documentação atual.

## Artefatos pendentes

As imagens comprovam a existência dos modelos, mas os seguintes arquivos-fonte ainda não estavam versionados no repositório no momento desta organização:

- arquivos editáveis do SolidWorks (`.SLDPRT` e `.SLDASM`);
- arquivos neutros para intercâmbio CAD (`.STEP` ou `.STP`);
- arquivos de impressão 3D (`.STL` ou `.3MF`);
- arquivos CAD e de impressão 3D referentes ao labirinto de testes.

Quando forem obtidos, os modelos CAD deverão ser adicionados em `arquivos-cad/` e os modelos de manufatura aditiva em `impressao-3d/`.

## Documentação relacionada

- [Projeto conceitual de estruturas](../docs/4.1%20-%20Projeto%20conceitual%20de%20estruturas.md)
- [Testes de estrutura](../docs/7.1%20-%20Testes%20de%20estrutura.md)
- [Testes de integração](../docs/7.5%20-%20Testes%20de%20integração.md)

## Regras de armazenamento

- Não adicionar esquemáticos eletrônicos, projetos KiCad ou outros arquivos de hardware eletrônico nesta pasta; esses artefatos pertencem a [`hw/`](../hw/).
- Não adicionar código-fonte nesta pasta; o firmware pertence a [`src/firmware/`](../src/firmware/).
- Usar nomes descritivos e manter a extensão original dos arquivos de engenharia.
- Registrar no documento de testes correspondente a origem e a finalidade das novas evidências.

Os estudos técnicos e textos acadêmicos devem ser produzidos e revisados pela equipe, conforme as orientações da disciplina.
