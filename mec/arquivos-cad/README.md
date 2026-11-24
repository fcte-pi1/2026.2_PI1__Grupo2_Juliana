# Arquivos CAD

Arquivos extraídos de `Estruturas_PI1_2026_2.zip`, recebidos como material do projeto estrutural. O pacote contém uma versão de setembro de 2026; ela não inclui o chassi refatorado para a roda boba nem os espaçadores adicionais de 15 mm e traz o MPU6050 em lugar da BNO085. Os arquivos CAD, STEP e PDFs finais atualizados ainda precisam ser obtidos.

- [`chassi/`](./chassi/): montagem SolidWorks (`assembly_chassi.SLDASM`), exportação `assembly_chassi.STEP`, peças e modelos de componentes usados na montagem. Os modelos de eletrônica nesta pasta são referências geométricas do conjunto CAD.
- [`chassi-atualizado/`](./chassi-atualizado/): novos arquivos da base e do patamar intermediário em SolidWorks e STEP. São versões diferentes das peças de mesmo nome no pacote anterior. A [imagem da montagem atualizada](../modelos-cad/chassi-atualizado.png) mostra o apoio da roda boba.
- [`desenhos/`](./desenhos/): desenhos editáveis SolidWorks e modelo de folha.
- [`labirinto/`](./labirinto/): modelo da base, desenho e PDF do gabarito.

Os arquivos da montagem foram mantidos juntos para preservar suas possíveis referências. A abertura completa da montagem e a resolução de referências externas ainda devem ser verificadas no SolidWorks. Os desenhos em PDF estão em [`../desenhos-tecnicos/pdf/`](../desenhos-tecnicos/pdf/) e as imagens em [`../modelos-cad/`](../modelos-cad/).

As novas peças foram mantidas separadas para não alterar silenciosamente as referências da montagem de setembro. Ainda são necessários a montagem completa e os desenhos finais atualizados, além da confirmação dos espaçadores e da BNO085 nessa montagem.
