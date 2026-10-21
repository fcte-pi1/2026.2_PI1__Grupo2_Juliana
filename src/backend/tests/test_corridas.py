"""Testes das corridas, tentativas e consultas do histórico (RF07-S, RF08-S, RF09-S, RF10-S)."""

from contextlib import contextmanager
from datetime import datetime, timedelta
from decimal import Decimal
from zoneinfo import ZoneInfo

import pytest
from fastapi.testclient import TestClient
from sqlalchemy import select
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session

from app.db.models import Corrida, Labirinto, LeituraTelemetria, LogEvento
from app.main import app
from app.services.corridas import FUSO_HORARIO
from app.services.protocol import format_frame
from app.services.telemetry_service import telemetry_service

cliente = TestClient(app)


@pytest.fixture
def sessao(conexao, monkeypatch):
    telemetry_service.reset()
    with Session(bind=conexao, join_transaction_mode="create_savepoint") as sessao:
        @contextmanager
        def escopo():
            yield sessao
        monkeypatch.setattr("app.routers.corridas.session_scope", escopo)
        monkeypatch.setattr("app.routers.telemetry.session_scope", escopo)
        yield sessao
    telemetry_service.reset()


@pytest.fixture
def outro_labirinto(sessao):
    labirinto = Labirinto(nome="labirinto-pytest-8x4", largura_celula=8, altura_celula=4)
    sessao.add(labirinto)
    sessao.commit()
    return labirinto.id


def leitura(id_corrida, tempo_ms, velocidade="0.200", **campos):
    dados = {"pos_x": 0, "pos_y": 0, "orientacao": "N", "paredes": 9,
             "velocidade": Decimal(velocidade), "tensao_bateria": Decimal("8.10"), "valor_flood": 6}
    return LeituraTelemetria(id_corrida=id_corrida, tempo_ms=tempo_ms, **{**dados, **campos})


def criar(id_labirinto):
    resposta = cliente.post("/corridas", json={"id_labirinto": id_labirinto})
    assert resposta.status_code == 201
    return resposta.json()


def encerrar(id_corrida):
    resposta = cliente.post(f"/corridas/{id_corrida}/encerrar")
    assert resposta.status_code == 200
    return resposta.json()


def listar(**params):
    resposta = cliente.get("/corridas", params=params)
    assert resposta.status_code == 200
    return resposta.json()


def total_tentativas(id_labirinto):
    labirintos = {lab["id"]: lab for lab in cliente.get("/labirintos").json()}
    return labirintos[id_labirinto]["total_tentativas"]


def test_criar_corrida_abre_tentativa_em_andamento(sessao, id_labirinto):
    corrida = criar(id_labirinto)
    assert corrida["status"] == "em_andamento"
    assert corrida["num_tentativa"] == 1
    assert corrida["num_corrida_robo"] is None
    assert corrida["labirinto"] == "labirinto-pytest"
    assert (corrida["largura_celula"], corrida["altura_celula"]) == (4, 4)
    assert corrida["leituras"] == []
    assert corrida["total_leituras"] == 0
    assert corrida["duracao_s"] is None


def test_tentativas_sao_numeradas_e_contadas_por_labirinto(sessao, id_labirinto, outro_labirinto):
    # CT-S09: duas no 4x4, uma no 8x4 e a terceira no 4x4
    numeros = []
    for labirinto in (id_labirinto, id_labirinto, outro_labirinto, id_labirinto):
        corrida = criar(labirinto)
        numeros.append(corrida["num_tentativa"])
        encerrar(corrida["id"])
    assert numeros == [1, 2, 1, 3]
    assert total_tentativas(id_labirinto) == 3
    assert total_tentativas(outro_labirinto) == 1
    assert [c["num_tentativa"] for c in listar(id_labirinto=id_labirinto)] == [3, 2, 1]


def test_labirinto_sem_tentativas_tem_total_zero_e_consulta_vazia(sessao, id_labirinto):
    # CT-S11
    assert total_tentativas(id_labirinto) == 0
    assert listar(id_labirinto=id_labirinto) == []


def test_so_uma_corrida_em_andamento_por_vez(sessao, id_labirinto, outro_labirinto):
    criar(id_labirinto)
    resposta = cliente.post("/corridas", json={"id_labirinto": outro_labirinto})
    assert resposta.status_code == 409
    assert total_tentativas(outro_labirinto) == 0


def test_corridas_criadas_ao_mesmo_tempo_respondem_409(sessao, id_labirinto, monkeypatch):
    # A outra corrida passa pela verificação antes do commit: o índice único barra no banco
    def commit_concorrente():
        raise IntegrityError("INSERT", {}, Exception("uq_corrida_uma_em_andamento"))
    monkeypatch.setattr(sessao, "commit", commit_concorrente)
    resposta = cliente.post("/corridas", json={"id_labirinto": id_labirinto})
    assert resposta.status_code == 409
    assert resposta.json()["detail"] == "já existe uma corrida em andamento"


def test_criar_corrida_em_labirinto_inexistente(sessao):
    assert cliente.post("/corridas", json={"id_labirinto": 999999}).status_code == 404


@pytest.mark.parametrize("corpo", [{}, {"id_labirinto": 0}, {"id_labirinto": "x"}])
def test_criar_corrida_rejeita_corpo_invalido(corpo):
    assert cliente.post("/corridas", json=corpo).status_code == 422


def test_encerrar_grava_tentativa_abandonada_com_o_que_foi_recebido(sessao, id_labirinto):
    # CT-S08: a tentativa interrompida fica gravada, com a duração até a última leitura
    corrida = criar(id_labirinto)
    sessao.add_all([leitura(corrida["id"], 0), leitura(corrida["id"], 1400, pos_y=1)])
    sessao.commit()
    encerrada = encerrar(corrida["id"])
    assert encerrada["status"] == "abandonada"
    assert encerrada["tempo_conc_s"] is None
    assert encerrada["duracao_s"] == 1.4
    assert [l["tempo_ms"] for l in encerrada["leituras"]] == [0, 1400]
    log = sessao.scalars(select(LogEvento).where(LogEvento.id_corrida == corrida["id"])).one()
    assert (log.tipo, log.origem) == ("aviso", "backend")
    assert total_tentativas(id_labirinto) == 1


def test_encerrar_corrida_ja_encerrada_ou_inexistente(sessao, id_labirinto):
    corrida = criar(id_labirinto)
    encerrar(corrida["id"])
    assert cliente.post(f"/corridas/{corrida['id']}/encerrar").status_code == 409
    assert cliente.post("/corridas/999999/encerrar").status_code == 404


def test_detalhe_traz_resumo_e_trajeto_em_ordem(sessao, id_corrida):
    sessao.add_all([
        leitura(id_corrida, 400, velocidade="0.300", pos_x=1, orientacao="L"),
        leitura(id_corrida, 0, velocidade="0.100"),
    ])
    corrida = sessao.get(Corrida, id_corrida)
    corrida.status = "concluida"
    corrida.tempo_conc_s = Decimal("12.345")
    corrida.consumo_medio_ma = Decimal("410.50")
    sessao.commit()
    resposta = cliente.get(f"/corridas/{id_corrida}")
    assert resposta.status_code == 200
    detalhe = resposta.json()
    assert detalhe["tempo_conc_s"] == 12.345
    assert detalhe["duracao_s"] == 12.345
    assert detalhe["consumo_medio_ma"] == 410.5
    assert detalhe["velocidade_media"] == 0.2
    assert detalhe["total_leituras"] == 2
    assert [(l["tempo_ms"], l["pos_x"], l["orientacao"]) for l in detalhe["leituras"]] == [(0, 0, "N"), (400, 1, "L")]
    assert detalhe["leituras"][1]["velocidade"] == 0.3


def test_detalhe_de_corrida_inexistente(sessao):
    assert cliente.get("/corridas/999999").status_code == 404


def test_consulta_geral_e_filtros(sessao, id_labirinto, outro_labirinto):
    # CT-S10 e CT-S12: a consulta geral é a soma das consultas por labirinto
    primeira = criar(id_labirinto)
    sessao.add(leitura(primeira["id"], 0))
    sessao.commit()
    encerrar(primeira["id"])
    segunda = criar(outro_labirinto)
    corrida = sessao.get(Corrida, segunda["id"])
    corrida.status = "concluida"
    corrida.tempo_conc_s = Decimal("30.000")
    sessao.commit()

    nossas = {primeira["id"], segunda["id"]}
    geral = [c for c in listar() if c["id"] in nossas]
    assert [c["id"] for c in geral] == [segunda["id"], primeira["id"]]
    assert [c["id"] for c in listar(id_labirinto=id_labirinto)] == [primeira["id"]]
    assert [c["id"] for c in listar(id_labirinto=outro_labirinto)] == [segunda["id"]]
    assert segunda["id"] in [c["id"] for c in listar(status="concluida")]
    assert primeira["id"] not in [c["id"] for c in listar(status="concluida")]
    assert [c["id"] for c in listar(id_labirinto=id_labirinto, status="abandonada")] == [primeira["id"]]
    assert listar(id_labirinto=id_labirinto, num_tentativa=2) == []

    hoje = datetime.now(ZoneInfo(FUSO_HORARIO)).date()
    assert [c["id"] for c in listar(id_labirinto=id_labirinto, data=hoje.isoformat())] == [primeira["id"]]
    assert listar(id_labirinto=id_labirinto, data=(hoje - timedelta(days=1)).isoformat()) == []


def test_consulta_paginada(sessao, id_labirinto):
    ids = []
    for _ in range(3):
        corrida = criar(id_labirinto)
        encerrar(corrida["id"])
        ids.append(corrida["id"])
    ids.reverse()
    assert [c["id"] for c in listar(id_labirinto=id_labirinto, limite=2)] == ids[:2]
    assert [c["id"] for c in listar(id_labirinto=id_labirinto, limite=2, deslocamento=2)] == ids[2:]


@pytest.mark.parametrize("params", [
    {"id_labirinto": 0}, {"status": "cancelada"}, {"num_tentativa": 0}, {"data": "20/10/2026"},
    {"limite": 0}, {"limite": 201}, {"deslocamento": -1},
])
def test_consulta_rejeita_filtros_invalidos(params):
    assert cliente.get("/corridas", params=params).status_code == 422


def test_corrida_criada_pelo_painel_recebe_a_telemetria_do_robo(sessao, id_labirinto):
    # CT-S07: corrida aberta pela rota, percorrida pelo robô e consultada no histórico
    corrida = criar(id_labirinto)
    cab = {"v": 1, "corrida": 7}
    with cliente.websocket_connect("/ws/telemetria") as ws:
        for seq, msg in enumerate([
            {"t": "evt", "tempo_ms": 0, "evento": "inicio", "tensao_bateria": 8.2},
            {"t": "tel", "tempo_ms": 0, "pos_x": 0, "pos_y": 0, "orientacao": "N", "paredes": 11,
             "velocidade": 0.2, "tensao_bateria": 8.2, "valor_flood": 1, "estado": "explorando"},
            {"t": "tel", "tempo_ms": 200, "pos_x": 0, "pos_y": 1, "orientacao": "N", "paredes": 13,
             "velocidade": 0.0, "tensao_bateria": 8.19, "valor_flood": 0, "estado": "parado"},
            {"t": "evt", "tempo_ms": 200, "evento": "fim", "resultado": "concluida",
             "tempo_conc_s": 0.2, "consumo_medio_ma": 400.0},
        ]):
            ws.send_text(format_frame({**cab, **msg, "seq": seq}))
            ws.receive_text()
    detalhe = cliente.get(f"/corridas/{corrida['id']}").json()
    assert detalhe["status"] == "concluida"
    assert detalhe["num_corrida_robo"] == 7
    assert detalhe["tempo_conc_s"] == 0.2
    assert [(l["pos_x"], l["pos_y"]) for l in detalhe["leituras"]] == [(0, 0), (0, 1)]
    assert total_tentativas(id_labirinto) == 1


def test_telemetria_apos_encerrar_pelo_painel_recebe_ack_sem_encher_o_log(sessao, id_labirinto):
    # O robô não sabe que a corrida foi encerrada e continua enviando (e reenviando)
    corrida = criar(id_labirinto)
    cab = {"v": 1, "corrida": 7}
    tel = {"t": "tel", "pos_x": 0, "pos_y": 0, "orientacao": "N", "paredes": 11,
           "velocidade": 0.2, "tensao_bateria": 8.2, "valor_flood": 1, "estado": "explorando"}
    with cliente.websocket_connect("/ws/telemetria") as ws:
        ws.send_text(format_frame({**cab, "seq": 0, "t": "evt", "tempo_ms": 0, "evento": "inicio", "tensao_bateria": 8.2}))
        ws.receive_text()
        encerrar(corrida["id"])
        for seq in range(1, 21):
            ws.send_text(format_frame({**cab, **tel, "seq": seq, "tempo_ms": seq * 200}))
            assert ws.receive_text()
        ws.send_text(format_frame({**cab, "seq": 21, "t": "evt", "tempo_ms": 4200, "evento": "fim",
                                   "resultado": "concluida", "tempo_conc_s": 4.2}))
        assert ws.receive_text()
    logs = sessao.scalars(select(LogEvento).where(LogEvento.id_corrida == corrida["id"])).all()
    assert [(log.tipo, log.mensagem) for log in logs if log.tipo != "info"] == [
        ("aviso", "corrida encerrada manualmente pelo painel"),
    ]
    detalhe = cliente.get(f"/corridas/{corrida['id']}").json()
    assert detalhe["status"] == "abandonada"
    assert detalhe["leituras"] == []
