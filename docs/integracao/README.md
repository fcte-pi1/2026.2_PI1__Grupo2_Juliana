# Documentação Modular dos Testes de Integração

Para evitar conflitos de *merge* quando várias pessoas documentam testes em paralelo, os ensaios de integração foram divididos em **um arquivo por teste** (`5.1` a `5.22`) dentro desta pasta.

## Regras para todos os PRs

1. **Nunca edite `docs/7.5 - Testes de integração.md` manualmente** em PRs de frente ou de testes individuais. Esse arquivo é consolidado automaticamente.
2. **Edite apenas o(s) arquivo(s) `docs/integracao/5.X-*.md`** correspondente(s) ao(s) seu(s) teste(s):
   - Atualize os campos no bloco YAML do topo (`data_inicio`, `data_conclusao`, `responsavel`, `resultados_obtidos`, `aprovacao_interna`, `observacoes`), mantendo as aspas duplas.
   - Detalhe procedimento, medições, evidências e ações corretivas nas seções do corpo Markdown.
   - Para novos testes não listados abaixo, use o modelo [`_template.md`](./_template.md) após alinhar o próximo ID (`5.23+`) com a equipe.
3. **Salve evidências na subpasta do ID** em [`docs/evidencias/integracao/`](../evidencias/integracao/README.md):
   - Exemplos: `docs/evidencias/integracao/5.5/5.5-medicao-largura.jpeg`, `docs/evidencias/integracao/5.20/5.20-latencia.csv`.
   - Respeite o limite de **5 MB por arquivo** no Git. Vídeos vão para o Google Drive ou YouTube e são referenciados pelo link no `.md`.

## Consolidação automática do `7.5`

Na entrega/revisão final (issue `#291`) ou sempre que quiser conferir a renderização consolidada:

```powershell
# Atualiza docs/7.5 - Testes de integração.md a partir dos arquivos desta pasta
py scripts/consolidar_integracao.py

# Verifica sem alterar arquivos (retorna 0 se estiver sincronizado)
py scripts/consolidar_integracao.py --check
```

## Tabela de alocação de IDs e arquivos (`5.1` a `5.22`)

| **ID** | **Arquivo** | **Frente** | **Issue(s)** | **Responsável** |
|:------:|-------------|------------|:------------:|-----------------|
| 5.1 | [5.1-soldagem-fixacao-chassi.md](./5.1-soldagem-fixacao-chassi.md) | Hardware, Estrutura e Energia (4.1.4) | #261 | @fxred |
| 5.2 | [5.2-centro-gravidade-bateria.md](./5.2-centro-gravidade-bateria.md) | Hardware, Estrutura e Energia (4.1.4) | #262 | @Mach1r0, @cwtshh, @anabiacs |
| 5.3 | [5.3-alinhamento-sensores.md](./5.3-alinhamento-sensores.md) | Hardware, Estrutura e Energia (4.1.4) | #263 | @MLuana725 |
| 5.4 | [5.4-estabilidade-eletrica-autonomia.md](./5.4-estabilidade-eletrica-autonomia.md) | Hardware, Estrutura e Energia (4.1.4) | #272 | @arthurhc3 |
| 5.5 | [5.5-envelope-dimensional-giro.md](./5.5-envelope-dimensional-giro.md) | Hardware, Estrutura e Energia (4.1.4) | #264 | @anabiacs |
| 5.6 | [5.6-inspecao-estrutural-pos-corridas.md](./5.6-inspecao-estrutural-pos-corridas.md) | Hardware, Estrutura e Energia (4.1.4) | #265 | @Mach1r0 |
| 5.7 | [5.7-massa-total-orcamento.md](./5.7-massa-total-orcamento.md) | Hardware, Estrutura e Energia (4.1.4) | #294 | @cwtshh, @anabiacs |
| 5.8 | [5.8-manutencao-acesso-componentes.md](./5.8-manutencao-acesso-componentes.md) | Hardware, Estrutura e Energia (4.1.4) | #295 | @Mach1r0, @cwtshh |
| 5.9 | [5.9-unificacao-drivers-firmware.md](./5.9-unificacao-drivers-firmware.md) | Firmware e Hardware (4.1.5) | #275 | @MatielloAL |
| 5.10 | [5.10-leitura-bateria-ina226.md](./5.10-leitura-bateria-ina226.md) | Firmware e Hardware (4.1.5) | #277, #216 | @Jagaima, @eduardoorsomarso, @MatielloAL, @luccameds |
| 5.11 | [5.11-giroscopio-bno085-firmware.md](./5.11-giroscopio-bno085-firmware.md) | Firmware e Hardware (4.1.5) | #213, #275 | @MatielloAL, @LuisGuilherme67 |
| 5.12 | [5.12-motores-n20-drv8833-bancada.md](./5.12-motores-n20-drv8833-bancada.md) | Firmware e Hardware (4.1.5) | #214, #273 | @MatielloAL, @LuisGuilherme67, @diegolxxz |
| 5.13 | [5.13-calibracao-pid-peso-real.md](./5.13-calibracao-pid-peso-real.md) | Firmware e Hardware (4.1.5) | #273 | @diegolxxz |
| 5.14 | [5.14-sincronizacao-vl53l0x-movimento.md](./5.14-sincronizacao-vl53l0x-movimento.md) | Firmware e Hardware (4.1.5) | #274 | @LuisGuilherme67 |
| 5.15 | [5.15-filtro-ruido-deteccao-paredes.md](./5.15-filtro-ruido-deteccao-paredes.md) | Firmware e Hardware (4.1.5) | #279 | @MatielloAL |
| 5.16 | [5.16-modo-automatico-calibracao-recuperacao.md](./5.16-modo-automatico-calibracao-recuperacao.md) | Firmware e Hardware (4.1.5) | #278 | @MatielloAL |
| 5.17 | [5.17-navegacao-flood-fill-labirinto.md](./5.17-navegacao-flood-fill-labirinto.md) | Firmware e Hardware (4.1.5) | #276 | @MLuana725 |
| 5.18 | [5.18-atualizacao-firmware-ota.md](./5.18-atualizacao-firmware-ota.md) | Firmware e Sistema Web (4.1.6) | #217 | @MatielloAL |
| 5.19 | [5.19-wifi-telemetria-robo-fisico.md](./5.19-wifi-telemetria-robo-fisico.md) | Firmware e Sistema Web (4.1.6) | #280 | @diegolxxz, @MatielloAL |
| 5.20 | [5.20-renderizacao-latencia-dashboard.md](./5.20-renderizacao-latencia-dashboard.md) | Firmware e Sistema Web (4.1.6) | #281 | @MatielloAL |
| 5.21 | [5.21-tolerancia-falha-wifi-reconexao.md](./5.21-tolerancia-falha-wifi-reconexao.md) | Firmware e Sistema Web (4.1.6) | #282 | @MatielloAL |
| 5.22 | [5.22-telemetria-historico-csv-logs.md](./5.22-telemetria-historico-csv-logs.md) | Firmware e Sistema Web (4.1.6) | #283, #290 | @MatielloAL, @HladczukLe, @marcellaanderle, @Geovanaduartee |
