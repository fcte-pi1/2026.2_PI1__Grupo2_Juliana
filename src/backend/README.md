# _Backend_

Esta pasta deverá armazenar arquivos referentes a:

- Código-fonte da API REST: rotas, controladores, modelos e lógica de negócio.
- Arquivos de configuração do servidor: `app.py`, `server.js`, `main.go` etc., dependendo da linguagem/framework utilizado ([Flask](https://flask.palletsprojects.com/), [FastAPI](https://fastapi.tiangolo.com/), [Express](https://expressjs.com/), [Django](https://www.djangoproject.com/) etc.).
- Arquivos de definição de dependências: `requirements.txt` ou `pyproject.toml` (Python), `package.json` (Node.js), `pom.xml` (Java/Maven) etc.
- Scripts de migração e esquemas de banco de dados: arquivos `.sql`, scripts de migração ([Alembic](https://alembic.sqlalchemy.org/), [Sequelize](https://sequelize.org/) etc.) e seeds de dados para desenvolvimento.
- Arquivos de configuração de ambiente: `.env.example` com as variáveis de ambiente necessárias (nunca o `.env` real).
- Arquivos de containerização: `Dockerfile` e `docker-compose.yml`, caso o serviço seja executado em contêiner.

Evite incluir:

- Credenciais e segredos: arquivos `.env`, chaves de API, senhas, tokens de acesso ou qualquer dado sensível **nunca** devem ser versionados.
- Artefatos de build: diretórios como `__pycache__/`, `dist/`, `build/`, `.eggs/` devem ser gerados localmente e ignorados via `.gitignore`.
- Dependências instaladas: pastas como `node_modules/` ou ambientes virtuais Python (`venv/`, `.env/`) não devem ser incluídos no repositório.
- Arquivos temporários/específicos do sistema operacional: arquivos gerados automaticamente pelo sistema ou pelo gerenciador de arquivos (ex.: `*~`, `.DS_Store`, `Thumbs.db`).
> [!WARNING]
> **Não acrescente arquivos referentes ao _frontend_ nesta pasta.** Eles deverão ser armazenados na pasta [frontend](https://github.com/fcte-pi1/template/tree/main/src/frontend) deste repositório.
## Como rodar o banco de dados

O banco é um PostgreSQL 16 rodando em Docker. As tabelas seguem o Dicionário de Dados da [4.4.4 - Persistência de Dados](../../docs/4.4.4%20-%20Persistencia%20de%20Dados.md) e são criadas pelas migrações do Alembic.

### Pré-requisitos

- Docker com o plugin Compose (`docker compose version`)
- Python 3.12 com o módulo `venv` (no Ubuntu: `sudo apt install python3.12-venv`)

### Primeira vez

Todos os comandos são executados dentro de `src/backend`:

```bash
cp .env.example .env              # depois troque a senha no .env
docker compose up -d              # sobe o PostgreSQL; aguarde "healthy" em docker compose ps
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
alembic upgrade head              # cria as tabelas e os labirintos 4x4, 8x4 e 12x4
pytest                            # testa as restrições de integridade
```

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
| `app/db/models.py` | Tabelas `usuario`, `labirinto`, `corrida`, `leitura_telemetria` e `log_evento` |
| `migrations/versions/0001_*` | Criação das tabelas e restrições |
| `migrations/versions/0002_*` | Dados iniciais: labirintos 4x4, 8x4 e 12x4 |
| `tests/` | Testes das restrições de integridade (RNF07-S) |
