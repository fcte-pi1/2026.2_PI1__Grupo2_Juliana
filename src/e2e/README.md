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

## Estrutura

| Caminho | Conteúdo |
| :--- | :--- |
| `playwright.config.ts` | Servidores, navegador e um teste por vez (o banco é compartilhado) |
| `global-setup.ts` | Cria e migra o banco de testes antes da suíte |
| `helpers/ambiente.ts` | Caminhos, bancos, portas e Python usado |
| `helpers/teste.ts` | `test` e `expect` com o banco limpo antes de cada caso |
| `helpers/banco.ts` | `limparBanco`, `criarCorrida` e `consultar` (SQL para conferir o que foi gravado) |
| `helpers/simulador.ts` | `rodarSimulador(args)`: roda o `simulador.py` e conta mensagens, acks e falhas |
| `tests/base.spec.ts` | Fumaça da infraestrutura (servidores no ar e simulador gravando no banco) |

`criarCorrida` grava a corrida direto no banco porque ainda não há rota para isso. Quando a 3.8.2 entrar, troca pela rota.

## No CI

O job `e2e` do [ci.yml](../../.github/workflows/ci.yml) roda a suíte em todo Pull Request com um PostgreSQL de serviço. Se um teste falhar, ele é repetido uma vez; se passar na segunda, aparece como **flaky** no relatório, e isso é sinal para corrigir o teste, não para ignorar. O relatório HTML (com trace, print e vídeo das falhas) fica nos artefatos da execução, como `playwright-report`.
