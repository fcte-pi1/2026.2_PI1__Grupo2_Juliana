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
    conexao.execute(
        text("INSERT INTO usuario (matricula, nome, senha_hash) VALUES ('pytest', 'Usuário de Teste', 'x')")
    )
    return conexao.execute(
        text(
            "INSERT INTO corrida (id_labirinto, id_usuario, num_tentativa) "
            "VALUES (:id_labirinto, 'pytest', 1) RETURNING id"
        ),
        {"id_labirinto": id_labirinto},
    ).scalar_one()
