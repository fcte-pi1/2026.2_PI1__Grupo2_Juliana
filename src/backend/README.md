# Backend

Servidor de telemetria do Micromouse em Python com [FastAPI](https://fastapi.tiangolo.com/).

## Requisitos
- Python 3.13

## Instalação
```bash
cd src/backend
python -m venv .venv
# Windows: .venv\Scripts\activate | Linux/macOS: source .venv/bin/activate
pip install -r requirements-dev.txt
```

## Rodar
```bash
uvicorn app.main:app --reload
```
A API sobe em http://localhost:8000 (documentação em http://localhost:8000/docs).

## Testar
```bash
pytest
```
Os testes ficam em `tests/` e a cobertura do pacote `app` é mostrada ao final. O mesmo comando roda no GitHub Actions em todo Pull Request.

## Como rodar o banco de dados

O banco é um PostgreSQL 16 rodando em Docker. As tabelas seguem o Dicionário de Dados da [4.4.4 - Persistência de Dados](../../docs/4.4.4%20-%20Persistencia%20de%20Dados.md) e são criadas pelas migrações do Alembic.

### Pré-requisitos

- Docker com o plugin Compose (`docker compose version`)
- Python 3.13 com o módulo `venv` (no Ubuntu: `sudo apt install python3.13-venv`)

### Primeira vez

Todos os comandos são executados dentro de `src/backend`:

```bash
cp .env.example .env              # depois troque a senha no .env
docker compose up -d              # sobe o PostgreSQL; aguarde "healthy" em docker compose ps
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements-dev.txt
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
