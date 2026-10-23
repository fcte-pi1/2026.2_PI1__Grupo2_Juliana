# Testes E2E (CT-S)

Automação com [Playwright](https://playwright.dev/) dos casos de teste do sistema web do [Roteiro de Testes Funcionais (4.4.6)](../../docs/4.4.6%20-%20Roteiro%20de%20testes%20funcionais.md) (atividade 3.9.4.1). Cada teste sobe o fluxo inteiro: banco, backend, site e o [simulador de telemetria](../backend/simulador.py) no lugar do robô.

## Pré-requisitos

- Banco configurado como em [src/backend/README.md](../backend/README.md) (`docker compose up -d`) e o `.venv` do backend com as dependências
- Dependências do frontend instaladas (`npm install` em `src/frontend`)
- Node 24

## Rodar

```bash
cd src/e2e
npm install
npx playwright install chromium   # só na primeira vez
npx playwright test               # roda tudo
npx playwright test --ui          # modo interativo, bom para escrever testes
npx playwright show-report        # relatório da última execução
```

O Playwright sobe sozinho o backend e o site e roda os testes num navegador escondido.

### Banco e portas próprios

Os testes **apagam corridas, leituras e logs** antes de cada caso. Para não mexer nos seus dados, eles usam um banco separado no mesmo PostgreSQL: `micromouse_e2e` (o nome do banco do `DATABASE_URL` + `_e2e`). O [global-setup.ts](global-setup.ts) cria esse banco se não existir e aplica as migrações antes da suíte. Seu banco `micromouse` não é tocado.

Pelo mesmo motivo, backend e site dos testes sobem em portas próprias: **18000** e **15173**. Assim um `uvicorn` ou `npm run dev` que você deixou aberto (8000 e 5173) nunca é reaproveitado por engano.

| Variável | Para quê | Padrão |
| :--- | :--- | :--- |
| `DATABASE_URL` | Banco principal (só usado para criar o de testes) | Lida de `src/backend/.env` |
| `E2E_DATABASE_URL` | Banco dos testes | `DATABASE_URL` + `_e2e` |
| `E2E_BACKEND_PORTA` / `E2E_FRONTEND_PORTA` | Trocar as portas se estiverem ocupadas | `18000` / `15173` |
| `PYTHON` | Python usado para backend, Alembic e simulador | `.venv` do backend, ou `python` do PATH |

## Regras para escrever testes

Teste automatizado só ajuda se o grupo confiar nele. Um teste que às vezes falha sem motivo faz todo mundo ignorar o CI. Estas regras evitam isso:

1. **Importe `test` e `expect` de `helpers/teste.ts`**, nunca de `@playwright/test`. É ele que limpa o banco antes de cada caso.
2. **Nunca espere tempo fixo** (`page.waitForTimeout(2000)`, `sleep`). Espere a coisa acontecer: `await expect(page.getByTestId('card-conexao')).toHaveText(/Conectado/)` tenta de novo sozinho até dar certo ou estourar o limite. Para o banco, use `await expect.poll(() => consultar(...)).toEqual(...)`.
3. **Simulador na velocidade real** (sem `--intervalo`). Acelerado, ele manda mais mensagens do que o backend consegue tratar: os acks atrasam, os reenvios chegam depois do `fim` e são descartados como "corrida desconhecida". O teste passa no seu PC e falha no CI, ou o contrário.
4. **Use `--semente`** no simulador para o labirinto ser sempre o mesmo e o resultado ser repetível.
5. **Cada teste prepara o próprio cenário** (`criarCorrida`, simulador). Não conte com dados deixados por outro teste: a ordem pode mudar e o banco é limpo entre eles.
6. **Ache elementos por `data-testid`** ou pelo texto que o usuário vê, nunca por classe CSS. Se precisar de um `data-testid` novo, combine com quem cuida da tela.
7. **Prefira o labirinto 4x4.** Na velocidade real, uma corrida 4x4 leva ~11 s; 8x4 e 12x4 só no caso que pede.
8. **Rodou verde uma vez não basta.** Antes do PR, rode o teste novo algumas vezes seguidas: `npx playwright test -g "nome do teste" --repeat-each 5`.

## Casos automatizados

| Caso | Arquivo | Situação |
| :--- | :--- | :--- |
| CT-S01 Recepção de telemetria | `tests/ct-s01.spec.ts` | ✅ |
| CT-S02 Pacote válido | `tests/ct-s02.spec.ts` | ✅ |
| CT-S03 Pacote corrompido | `tests/ct-s03.spec.ts` | ✅ backend, painel e tela Logs e Eventos |
| CT-S04 Tempo real | `tests/ct-s04.spec.ts` | ✅ |
| CT-S05 Bateria crítica | `tests/ct-s05.spec.ts` | ✅ |
| CT-S06 Perda de conexão | `tests/ct-s06.spec.ts` | ✅ |
| CT-S07 Persistência dos dados finais | `tests/ct-s07.spec.ts` | ✅ |
| CT-S08 Tentativa não concluída | `tests/ct-s08.spec.ts` | ✅ robô retirado e comando de abortar |
| CT-S09 Tentativas por labirinto | `tests/ct-s09.spec.ts` | ✅ |
| CT-S10 Consulta por labirinto | `tests/ct-s10.spec.ts` | ✅ |
| CT-S11 Labirinto sem registros | `tests/ct-s11.spec.ts` | ✅ |
| CT-S12 Consulta geral | `tests/ct-s12.spec.ts` | ✅ |
| CT-S13 Exportação CSV/PDF | `tests/ct-s13.spec.ts` | ✅ ("Exportar PDF" chama a impressão do navegador: o teste conta a chamada e gera o PDF com `page.pdf`) |
| CT-S14 Log de eventos e erros | `tests/ct-s14.spec.ts` | ✅ |
| CT-S17 Codificação das paredes | `tests/ct-s17.spec.ts` | ✅ confere o lado desenhado pela posição da linha, não só pelo nome |

Caso que ainda dependa de tela ou rota que não existe entra como `test.fixme`: aparece como *skipped* no relatório.

## Evidências para o documento 7.4

Cada caso anexa ao relatório do Playwright as evidências pedidas no roteiro (prints, CSV das consultas ao banco, log do simulador, latências). Para gravá-las também em `docs/evidencias/<caso>/`, no padrão `<caso>_<AAAA-MM-DD>_exec<n>_<nome>.<ext>`:

```bash
E2E_EVIDENCIAS=1 npx playwright test          # Linux/macOS/Git Bash
$env:E2E_EVIDENCIAS=1; npx playwright test     # PowerShell
```

Sem a variável nada é escrito em `docs/`, para o CI e as execuções do dia a dia não sujarem o repositório.

## Estrutura

| Caminho | Conteúdo |
| :--- | :--- |
| `playwright.config.ts` | Servidores, navegador e um teste por vez (o banco é compartilhado) |
| `global-setup.ts` | Cria e migra o banco de testes antes da suíte |
| `helpers/ambiente.ts` | Caminhos, bancos, portas e Python usado |
| `helpers/teste.ts` | `test` e `expect` com o banco limpo antes de cada caso |
| `helpers/banco.ts` | `limparBanco` e `consultar` (SQL para conferir o que foi gravado) |
| `helpers/api.ts` | `criarCorrida` e `encerrarCorrida` pelas rotas do backend (as mesmas do painel) |
| `helpers/simulador.ts` | `rodarSimulador(args)`: roda o `simulador.py` (corrida inteira, realista) e conta mensagens, acks e falhas |
| `helpers/robo.ts` | `RoboFalso`: manda pacotes exatos pelo protocolo (com CRC) e espera o ack, e reconecta mantendo o `seq`; `correrComRoboFalso` e `massaDoCtS09`; `ouvirPainel` escuta o que o backend retransmite ao site |
| `helpers/painel.ts` | `abrirPainel` (abre o site com o labirinto escolhido e espera conectar), `irPara(aba)`, `linhasDoHistorico`, `celulaDoMapa` |
| `helpers/evidencia.ts` | `salvarEvidencia`, `capturarTela` e `csv` |
| `tests/base.spec.ts` | Fumaça da infraestrutura (servidores no ar e simulador gravando no banco) |
| `tests/ct-sNN.spec.ts` | Um arquivo por caso do roteiro; o título começa com o código (`CT-S01 - ...`) |

**Simulador ou robô falso?** Use o `rodarSimulador` quando o caso é uma corrida inteira (taxa, perdas). Use o `RoboFalso` quando o caso precisa de pacotes exatos: valores conhecidos para comparar com a tela, pacotes inválidos específicos, pausas controladas.

**Ack chega antes da tela.** O backend responde o ack ao robô e só depois retransmite ao painel. Depois de `robo.tel(...)`, espere o valor aparecer (`expect(...).toHaveText`) ou a mensagem chegar ao `ouvirPainel` (`expect.poll`) antes de conferir.

**Massa de dados das consultas.** `correrComRoboFalso` faz uma tentativa inteira em ~1 s (rota de criar corrida, `inicio`, leituras, `fim`) e `massaDoCtS09` monta a massa do CT-S09 (4x4, 4x4, 8x4, 4x4), reaproveitada no CT-S10, CT-S12 e CT-S13.

## No CI

O job `e2e` do [ci.yml](../../.github/workflows/ci.yml) roda a suíte em todo Pull Request com um PostgreSQL de serviço. Se um teste falhar, ele é repetido uma vez; se passar na segunda, aparece como **flaky** no relatório, e isso é sinal para corrigir o teste, não para ignorar. O relatório HTML (com trace, print e vídeo das falhas) fica nos artefatos da execução, como `playwright-report`.
