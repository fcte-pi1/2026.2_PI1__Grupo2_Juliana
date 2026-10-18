"""Testes da exportação das corridas em CSV (RF18)."""

import csv
import io
from contextlib import contextmanager
from decimal import Decimal

import pytest
from fastapi.testclient import TestClient
from sqlalchemy.orm import Session

from app.db.models import Corrida, Labirinto, LeituraTelemetria
from app.main import app
from app.services.exportacao import COLUNAS


@pytest.fixture
def sessao_exportacao(conexao, monkeypatch):
    with Session(bind=conexao, join_transaction_mode="create_savepoint") as sessao:
        @contextmanager
        def escopo():
            yield sessao
        monkeypatch.setattr("app.routers.exportacao.session_scope", escopo)
        yield sessao


def leitura(id_corrida, tempo_ms, **campos):
    dados = {"pos_x": 0, "pos_y": 0, "orientacao": "N", "paredes": 9,
             "velocidade": Decimal("0.250"), "tensao_bateria": Decimal("8.10"), "valor_flood": 6}
    return LeituraTelemetria(id_corrida=id_corrida, tempo_ms=tempo_ms, **{**dados, **campos})


def baixar(params):
    resposta = TestClient(app).get("/exportacao/corridas.csv", params=params)
    assert resposta.status_code == 200
    return resposta, list(csv.DictReader(io.StringIO(resposta.text)))


def test_cabecalho_e_download_sem_corridas(sessao_exportacao):
    resposta, linhas = baixar({"id_corrida": 999999})
    assert resposta.headers["content-type"].startswith("text/csv")
    assert "attachment" in resposta.headers["content-disposition"]
    assert resposta.text.splitlines() == [",".join(COLUNAS)]
    assert linhas == []


def test_corrida_com_leituras_sai_uma_linha_por_leitura_em_ordem(sessao_exportacao, id_corrida):
    sessao_exportacao.add_all([
        leitura(id_corrida, 500, pos_y=1, valor_flood=5),
        leitura(id_corrida, 0),
    ])
    sessao_exportacao.commit()
    _, linhas = baixar({"id_corrida": id_corrida})
    assert [linha["tempo_ms"] for linha in linhas] == ["0", "500"]
    assert all(linha["id_corrida"] == str(id_corrida) for linha in linhas)
    assert linhas[0]["labirinto"] == "labirinto-pytest"
    assert linhas[0]["status"] == "em_andamento"
    assert linhas[0]["tempo_conc_s"] == ""
    assert linhas[1]["pos_y"] == "1"
    assert linhas[1]["orientacao"] == "N"
    assert linhas[1]["velocidade"] == "0.250"
    assert linhas[1]["tensao_bateria"] == "8.10"
    assert linhas[1]["valor_flood"] == "5"


def test_corrida_sem_leituras_sai_com_colunas_de_leitura_vazias(sessao_exportacao, id_corrida):
    _, linhas = baixar({"id_corrida": id_corrida})
    assert len(linhas) == 1
    assert linhas[0]["id_corrida"] == str(id_corrida)
    assert linhas[0]["num_tentativa"] == "1"
    assert all(linhas[0][coluna] == "" for coluna in COLUNAS[8:])


def test_filtro_por_labirinto_nao_mistura_corridas(sessao_exportacao, id_corrida, id_labirinto):
    outro = Labirinto(nome="labirinto-pytest-2", largura_celula=8, altura_celula=4)
    sessao_exportacao.add(outro)
    sessao_exportacao.flush()
    sessao_exportacao.add(Corrida(id_labirinto=outro.id, num_tentativa=1, status="concluida",
                                  tempo_conc_s=Decimal("12.500")))
    sessao_exportacao.commit()
    _, linhas = baixar({"id_labirinto": id_labirinto})
    assert [linha["id_corrida"] for linha in linhas] == [str(id_corrida)]
    _, linhas = baixar({"id_labirinto": outro.id})
    assert linhas[0]["status"] == "concluida"
    assert linhas[0]["tempo_conc_s"] == "12.500"


@pytest.mark.parametrize("params", [{"id_corrida": 0}, {"id_labirinto": 0}, {"id_corrida": "x"}])
def test_exportacao_rejeita_filtros_invalidos(params):
    assert TestClient(app).get("/exportacao/corridas.csv", params=params).status_code == 422
