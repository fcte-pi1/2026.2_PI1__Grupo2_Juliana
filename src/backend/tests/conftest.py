import os

import pytest
from dotenv import load_dotenv
from sqlalchemy import create_engine, text
from sqlalchemy.exc import OperationalError

load_dotenv()


@pytest.fixture(scope="session")
def engine():
    eng = create_engine(os.environ["DATABASE_URL"])
    try:
        with eng.connect():
            pass
    except OperationalError:
        pytest.fail(
            "Não foi possível conectar ao banco. Em src/backend, rode "
            "'docker compose up -d' e 'alembic upgrade head' antes dos testes."
        )
    yield eng
    eng.dispose()


@pytest.fixture
def conexao(engine):
    """Conexão dentro de uma transação desfeita no fim do teste: nenhum teste deixa dados no banco."""
    with engine.connect() as conn:
        trans = conn.begin()
        # Começa sem corridas, leituras e logs de outras execuções (ex.: uma corrida em_andamento
        # deixada pelos testes E2E). O DELETE também é desfeito no rollback.
        conn.execute(text("DELETE FROM leitura_telemetria; DELETE FROM log_evento; DELETE FROM corrida"))
        yield conn
        trans.rollback()


@pytest.fixture
def id_labirinto(conexao):
    return conexao.execute(
        text(
            "INSERT INTO labirinto (nome, largura_celula, altura_celula) "
            "VALUES ('labirinto-pytest', 4, 4) RETURNING id"
        )
    ).scalar_one()


@pytest.fixture
def id_corrida(conexao, id_labirinto):
    return conexao.execute(
        text("INSERT INTO corrida (id_labirinto, num_tentativa) VALUES (:id_labirinto, 1) RETURNING id"),
        {"id_labirinto": id_labirinto},
    ).scalar_one()
