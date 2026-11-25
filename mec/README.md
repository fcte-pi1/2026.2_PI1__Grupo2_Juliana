# Mecânica e Estruturas

Esta pasta reúne os artefatos do projeto mecânico e estrutural do Micromouse, incluindo desenhos técnicos, imagens do modelo CAD, registros dos componentes e evidências dos testes de fabricação e montagem.

## Organização da pasta

| Diretório | Conteúdo |
|---|---|
| [`componentes/`](./componentes/) | Imagens de referência das rodas e das alternativas de apoio do chassi. |
| [`desenhos-tecnicos/`](./desenhos-tecnicos/) | Desenhos em PNG e [PDF](./desenhos-tecnicos/pdf/), incluindo peças, montagem e EBOM. |
| [`modelos-cad/`](./modelos-cad/) | Imagens renderizadas do modelo CAD, incluindo vistas completa, explodida, da base e da montagem atualizada. |
| [`testes/`](./testes/) | Fotografias usadas como evidências dos testes estruturais de inventário, medição, montagem, encaixe de componentes, pesagem, sensores e cabeamento. |
| [`documentos/`](./documentos/) | Índice dos documentos de projeto, testes estruturais e integração mantidos em `docs/`. |
| [`arquivos-cad/`](./arquivos-cad/) | Montagem e peças em SolidWorks e STEP, incluindo as novas peças do chassi, desenhos editáveis e arquivos da base do labirinto. |
| [`impressao-3d/`](./impressao-3d/) | Modelos STL de referência, cuja correspondência com as peças fabricadas deve ser verificada. |

## Inventário atual

- 3 imagens de componentes e alternativas de apoio;
- 5 desenhos técnicos em PNG, 1 EBOM em PNG e 10 PDFs de desenhos e montagem;
- 9 imagens do modelo CAD;
- 37 fotografias de testes, montagem e pesagem;
- 44 arquivos de montagem, peças, desenhos e gabarito em `arquivos-cad/`;
- 2 modelos STL comerciais de referência em `impressao-3d/referencias/`.

Os arquivos desta pasta foram organizados a partir dos artefatos estruturais do repositório e do pacote `Estruturas_PI1_2026_2.zip`. As imagens originais em `docs/figs/estruturas/` foram mantidas para preservar os links da documentação.

## Artefatos pendentes

O pacote CAD de setembro de 2026 não representa a versão final: faltam na montagem o chassi refatorado para a roda boba, os espaçadores adicionais de 15 mm e a IMU BNO085 (o pacote contém modelos MPU6050). As novas peças da base e do patamar intermediário estão em [`arquivos-cad/chassi-atualizado/`](./arquivos-cad/chassi-atualizado/); faltam a montagem completa e os PDFs atualizados. Verificar as referências no SolidWorks. Os STLs incluídos são referências comerciais e ainda precisam ser comparados às peças fabricadas. Permanecem pendentes os STLs das peças fabricadas, os DXFs de corte dos patamares e o STL dos pilares do labirinto e, se aplicável, os modelos CAD/STL dos suportes dos sensores. O modelo editável do poste do labirinto está em [arquivos-cad/labirinto/](./arquivos-cad/labirinto/).

Também faltam os registros complementares de massa por componente, centro de gravidade e envelope final, a partir das medições e dados conferidos pela equipe.

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
