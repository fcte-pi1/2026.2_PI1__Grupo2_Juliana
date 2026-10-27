"""Testes da exportação das corridas e dos logs em CSV (RF18)."""

import csv
import io
from contextlib import contextmanager
from decimal import Decimal

import pytest
from fastapi.testclient import TestClient
from sqlalchemy.orm import Session

from app.db.models import Corrida, Labirinto, LeituraTelemetria, LogEvento
from app.main import app
from app.services.exportacao import COLUNAS, COLUNAS_LOGS


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


def baixar(params, arquivo="corridas.csv"):
    resposta = TestClient(app).get(f"/exportacao/{arquivo}", params=params)
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


def test_corridas_filtram_por_status_tentativa_e_data(sessao_exportacao, id_labirinto):
    c1 = Corrida(id_labirinto=id_labirinto, num_tentativa=1, status="concluida", tempo_conc_s=Decimal("10.0"))
    c2 = Corrida(id_labirinto=id_labirinto, num_tentativa=2, status="abandonada")
    sessao_exportacao.add_all([c1, c2])
    sessao_exportacao.commit()

    _, linhas_concluida = baixar({"status": "concluida"})
    assert all(l["status"] == "concluida" for l in linhas_concluida)
    assert any(l["id_corrida"] == str(c1.id) for l in linhas_concluida)
    assert not any(l["id_corrida"] == str(c2.id) for l in linhas_concluida)

    _, linhas_tentativa2 = baixar({"num_tentativa": 2})
    assert all(l["num_tentativa"] == "2" for l in linhas_tentativa2)


def test_logs_filtram_por_tipo_origem_e_busca(sessao_exportacao, id_corrida):
    sessao_exportacao.add_all([
        LogEvento(id_corrida=id_corrida, tipo="info", origem="backend", mensagem="Corrida iniciada com sucesso"),
        LogEvento(id_corrida=id_corrida, tipo="erro", origem="firmware", mensagem="Falha de sensor frontal"),
    ])
    sessao_exportacao.commit()

    _, linhas_erro = baixar({"tipo": "erro"}, "logs.csv")
    assert all(l["tipo"] == "erro" for l in linhas_erro)

    _, linhas_origem = baixar({"origem": "firmware"}, "logs.csv")
    assert all(l["origem"] == "firmware" for l in linhas_origem)

    _, linhas_busca = baixar({"busca": "sensor"}, "logs.csv")
    assert len(linhas_busca) == 1
    assert "sensor" in linhas_busca[0]["mensagem"].lower()


def test_logs_sem_resultados_saem_so_com_cabecalho(sessao_exportacao):
    resposta, linhas = baixar({"id_corrida": 999999}, "logs.csv")
    assert resposta.headers["content-type"].startswith("text/csv")
    assert 'filename="logs.csv"' in resposta.headers["content-disposition"]
    assert resposta.text.splitlines() == [",".join(COLUNAS_LOGS)]
    assert linhas == []


def test_logs_filtram_por_corrida_e_labirinto_em_ordem(sessao_exportacao, id_corrida, id_labirinto):
    outro = Labirinto(nome="labirinto-pytest-2", largura_celula=8, altura_celula=4)
    sessao_exportacao.add(outro)
    sessao_exportacao.flush()
    outra_corrida = Corrida(id_labirinto=outro.id, num_tentativa=1, status="concluida",
                            tempo_conc_s=Decimal("12.500"))
    sessao_exportacao.add(outra_corrida)
    sessao_exportacao.flush()
    sessao_exportacao.add_all([
        LogEvento(id_corrida=id_corrida, tipo="info", origem="backend", mensagem="evento inicio"),
        LogEvento(id_corrida=None, tipo="erro", origem="backend", mensagem="quadro invalido"),
        LogEvento(id_corrida=outra_corrida.id, tipo="aviso", origem="firmware", mensagem="outra"),
        LogEvento(id_corrida=id_corrida, tipo="aviso", origem="backend", mensagem="desconexao, rede"),
    ])
    sessao_exportacao.commit()
    _, linhas = baixar({"id_corrida": id_corrida}, "logs.csv")
    assert [linha["mensagem"] for linha in linhas] == ["evento inicio", "desconexao, rede"]
    assert linhas[0]["tipo"] == "info"
    assert linhas[0]["ocorrido_em"]
    _, linhas = baixar({"id_labirinto": outro.id}, "logs.csv")
    assert [linha["mensagem"] for linha in linhas] == ["outra"]
    _, linhas = baixar({}, "logs.csv")
    assert any(linha["id_corrida"] == "" for linha in linhas)


@pytest.mark.parametrize("arquivo", ["corridas.csv", "logs.csv"])
@pytest.mark.parametrize("params", [{"id_corrida": 0}, {"id_labirinto": 0}, {"id_corrida": "x"}])
def test_exportacao_rejeita_filtros_invalidos(arquivo, params):
    assert TestClient(app).get(f"/exportacao/{arquivo}", params=params).status_code == 422
