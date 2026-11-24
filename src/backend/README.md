# Backend

A API do Micromouse usa Python 3.13, FastAPI e Uvicorn. As rotas em `app/routers/` recebem requisições HTTP e conexões WebSocket. Os serviços em `app/services/` validam os quadros de telemetria, associam as leituras às corridas e preparam consultas e exportações. O acesso ao PostgreSQL 16 passa pelo SQLAlchemy com o driver psycopg. O Alembic mantém o esquema do banco.

## Configuração e execução local

Na raiz do repositório, prepare o ambiente de desenvolvimento com Python 3.13 e Docker Compose.

```bash
cd src/backend
python3.13 -m venv .venv
source .venv/bin/activate
pip install -r requirements-dev.txt
cp .env.example .env
```

No Windows, ative o ambiente pelo PowerShell com `.venv\Scripts\Activate.ps1`. Edite o arquivo local `.env` antes de iniciar o banco.

| Variável | Uso |
| --- | --- |
| `POSTGRES_USER` | Usuário criado pelo contêiner na primeira inicialização |
| `POSTGRES_PASSWORD` | Senha desse usuário. Substitua a senha de exemplo e evite `%`, que o Alembic interpreta |
| `POSTGRES_DB` | Nome do banco criado pelo contêiner |
| `DATABASE_URL` | URL usada pela API, pelas migrações e pelos testes. Use o mesmo usuário, senha e banco no formato `postgresql+psycopg://usuario:senha@localhost:5432/banco` |

Mantenha o `.env` fora do Git. No mesmo terminal, suba o banco, aplique as migrações e inicie a API.

```bash
docker compose up -d --wait
alembic upgrade head
uvicorn app.main:app --reload
```

O Python executa a aplicação diretamente. Confira a resposta `{"status":"ok"}` em [GET /health](http://localhost:8000/health) e consulte as rotas HTTP em [Swagger UI](http://localhost:8000/docs).

## Testes

Com o ambiente virtual ativo em `src/backend`, use um banco de desenvolvimento ou de testes com as migrações aplicadas e sem corrida em andamento. Os testes de restrição exercitam o índice que permite apenas uma corrida aberta por vez.

```bash
pytest
```

O pytest executa a suíte em `tests/` e mostra a cobertura de `app/`. Para testar também o fluxo ao vivo com o painel, use o [simulador](simulador.py).

## Consulta de logs

`GET /logs` retorna uma lista JSON, do evento mais recente ao mais antigo
(ordem por ID). Os filtros opcionais podem ser combinados:

| Parâmetro | Valores |
| --- | --- |
| `id_corrida` | ID do banco, inteiro positivo; não é o contador enviado pelo robô |
| `tipo` | `info`, `aviso`, `erro` |
| `origem` | `firmware`, `backend`, `frontend` |
| `limite` | 1 a 200, padrão 100 |
| `deslocamento` | inteiro não negativo, padrão 0 |

Exemplo: `/logs?id_corrida=1&tipo=aviso&limite=20`.
Cada item contém `id`, `id_corrida` (pode ser nulo), `tipo`, `origem`,
`mensagem` e `ocorrido_em`. Sem resultados, retorna `[]`; parâmetros inválidos
retornam HTTP 422. O contrato também aparece em `/docs`.

Ao encerrar a conexão de telemetria, o backend registra `desconexao` como
aviso, sem encerrar a corrida. Usa a última corrida confirmada por um pacote
válido e persistido naquela conexão. Sem associação confirmada (por exemplo,
uma conexão de teste ou do navegador), nada é registrado. Fechar após um evento
de fim já confirmado também não gera falso aviso. A detecção ocorre quando o WebSocket informa fechamento/erro; este
código não promete descobrir instantaneamente uma perda silenciosa de rede.

Descartes por quadro inválido ou por `corrida desconhecida` usam o contexto
já confirmado do socket, nunca o campo `corrida` do quadro descartado. Bateria crítica e reconexão mantêm
o processamento já existente. Nenhum limite de bateria foi alterado aqui.

## Corridas, tentativas e consultas

Antes de o robô largar, o painel abre a corrida; o evento `inicio` da
telemetria é associado a ela (Contrato de Telemetria, seção 5).

| Rota | Uso |
| --- | --- |
| `GET /labirintos` | Labirintos com `total_tentativas` de cada um (0 se não tiver) |
| `POST /corridas` | Corpo `{"id_labirinto": 1}`. Abre a próxima tentativa do labirinto como `em_andamento` (201). Labirinto inexistente: 404; já existe corrida em andamento: 409 |
| `POST /corridas/{id}/encerrar` | Encerra a corrida em andamento como `abandonada` e registra um aviso em `log_evento`. Inexistente: 404; já encerrada: 409 |
| `GET /corridas` | Histórico, da mais recente à mais antiga. Sem filtros é a consulta geral |
| `GET /corridas/{id}` | Resumo da corrida e as leituras em ordem de `tempo_ms` (trajeto) |

Filtros opcionais de `GET /corridas`, combináveis: `id_labirinto`,
`status` (`em_andamento`, `concluida`, `abandonada`), `num_tentativa` e
`data` (`AAAA-MM-DD`, dia de início no horário de Brasília). A paginação usa
`limite` (1 a 200, padrão 100) e `deslocamento` (padrão 0), como em `/logs`.
Sem resultados, retorna `[]`; parâmetros inválidos retornam HTTP 422.

Cada corrida traz `id`, `id_labirinto`, `labirinto`, `largura_celula`,
`altura_celula`, `num_tentativa`, `num_corrida_robo`, `status`,
`iniciado_em`, `tempo_conc_s`, `duracao_s`, `consumo_medio_ma`,
`velocidade_media` e `total_leituras`. `duracao_s` é o `tempo_conc_s` da
corrida concluída ou, se ela não foi concluída, o `tempo_ms` da última
leitura. `iniciado_em` vem em UTC (ex.: `2026-10-21T02:30:00Z` é 23h30 do
dia 20 em Brasília); as telas devem converter para o horário de Brasília ao
exibir.

Depois de `POST /corridas/{id}/encerrar` o robô não fica sabendo e continua
enviando. Os pacotes daquela corrida que chegam pelo mesmo socket recebem ACK
e são ignorados, sem gravar leitura nem erro no log. Em outra conexão, continuam
como `corrida desconhecida` (Contrato de Telemetria, seção 5). Os testes ficam em `tests/test_corridas.py`.

## Exportação CSV

`GET /exportacao/corridas.csv` baixa o arquivo `corridas.csv` (UTF-8, separado
por vírgula). Os filtros opcionais podem ser combinados:

| Parâmetro | Valores |
| --- | --- |
| `id_corrida` | ID do banco, inteiro positivo |
| `id_labirinto` | ID do banco, inteiro positivo |

O CSV de corridas também aceita `status`, `num_tentativa` e `data`. Exemplo: `/exportacao/corridas.csv?id_labirinto=1`.

Cada linha é uma leitura de telemetria, com os dados da corrida repetidos à
esquerda: `id_corrida`, `labirinto`, `num_tentativa`, `num_corrida_robo`,
`status`, `iniciado_em`, `tempo_conc_s`, `consumo_medio_ma`, `tempo_ms`,
`recebido_em`, `pos_x`, `pos_y`, `orientacao`, `paredes`, `velocidade`,
`tensao_bateria`, `valor_flood`. As linhas seguem a ordem da corrida e, dentro
dela, de `tempo_ms`. Corrida sem leituras sai em uma linha só, com as colunas
de leitura vazias. Valor nulo vira célula vazia. Sem resultados, o arquivo tem
só o cabeçalho; parâmetros inválidos retornam HTTP 422.

`GET /exportacao/logs.csv` baixa o arquivo `logs.csv` com os eventos de
`log_evento` (início/fim, desconexões, descartes, bateria crítica), com filtros por
`id_corrida`, `id_labirinto`, `tipo`, `origem`, `busca` e `data`. As colunas são `id`, `id_corrida`, `tipo`, `origem`,
`mensagem` e `ocorrido_em`, do evento mais antigo ao mais recente. Sem filtro,
inclui também os logs sem corrida (`id_corrida` vazio); com `id_labirinto`, só
entram os logs ligados a uma corrida daquele labirinto. Os testes ficam em
`tests/test_exportacao.py`.

## Como rodar o banco de dados

O banco é um PostgreSQL 16 rodando em Docker. As tabelas seguem o Dicionário de Dados da [4.4.4 - Persistência de Dados](../../docs/4.4.4%20-%20Persistencia%20de%20Dados.md) e são criadas pelas migrações do Alembic.

### No dia a dia

| Comando | Para quê |
| :--- | :--- |
| `source .venv/bin/activate` | Ativar o ambiente Python (a cada terminal novo) |
| `docker compose up -d` / `docker compose stop` | Ligar / desligar o banco, mantendo os dados |
| `alembic upgrade head` | Aplicar migrações novas depois de um `git pull` |
| `alembic current` | Ver em qual migração o banco está |
| `docker compose exec db psql -U micromouse -d micromouse` | Abrir o terminal SQL do banco (`\dt` lista as tabelas, `\q` sai) |
| `docker compose down -v` | ⚠️ Apagar o banco inteiro para recomeçar do zero |

### Alterar o modelo de dados

1. Edite as classes em `app/db/models.py` (e o Dicionário de Dados da 4.4.4).
2. Gere a migração: `alembic revision --autogenerate -m "descrição da mudança"`.
3. Revise o arquivo criado em `migrations/versions/` antes de aplicar.
4. Aplique com `alembic upgrade head` e rode `pytest`.

### Estrutura

| Caminho | Conteúdo |
| :--- | :--- |
| `docker-compose.yml` | Contêiner do PostgreSQL |
| `.env.example` | Modelo das variáveis de ambiente (o `.env` real não é versionado) |
| `app/db/models.py` | Tabelas `labirinto`, `corrida`, `leitura_telemetria` e `log_evento` |
| `migrations/versions/0001_*` | Criação das tabelas e restrições |
| `migrations/versions/0002_*` | Dados iniciais: labirintos 4x4, 8x4 e 12x4 |
| `migrations/versions/0003_*` | Coluna `num_corrida_robo` em `corrida` (associação com o Contrato de Telemetria, 4.4.7) |
| `migrations/versions/0004_*` | Índice que permite só uma corrida `em_andamento` por vez |
| `migrations/versions/0005_*` | Remove a tabela `usuario` e a coluna `corrida.id_usuario` (autenticação fora do escopo) |
| `tests/` | Testes das restrições de integridade (RNF07-S) |
