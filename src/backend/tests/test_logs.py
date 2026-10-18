"""Testes de persistência e consulta dos logs (RF20)."""

from contextlib import contextmanager

import pytest
from fastapi.testclient import TestClient
from sqlalchemy import select
from sqlalchemy.orm import Session

from app.db.models import Corrida, LogEvento
from app.main import app
from app.services.protocol import format_frame
from app.services.telemetry_service import telemetry_service


@pytest.fixture
def sessao_logs(conexao, monkeypatch):
    telemetry_service.reset()
    with Session(bind=conexao, join_transaction_mode="create_savepoint") as sessao:
        @contextmanager
        def escopo():
            yield sessao
        monkeypatch.setattr("app.routers.telemetry.session_scope", escopo)
        monkeypatch.setattr("app.routers.logs.session_scope", escopo)
        yield sessao
    telemetry_service.reset()


def inicio(numero=1):
    return {"v": 1, "t": "evt", "corrida": numero, "seq": 0,
            "tempo_ms": 0, "evento": "inicio", "tensao_bateria": 8.2}


def test_desconexao_fica_na_corrida_confirmada_e_nao_a_encerra(sessao_logs, id_corrida):
    with TestClient(app).websocket_connect("/ws/telemetria") as ws:
        ws.send_text(format_frame(inicio()))
        ws.receive_text()
    logs = sessao_logs.scalars(select(LogEvento).where(LogEvento.tipo == "aviso")).all()
    assert len(logs) == 1
    assert logs[0].id_corrida == id_corrida
    assert logs[0].origem == "backend"
    assert "desconexao" in logs[0].mensagem
    assert sessao_logs.get(Corrida, id_corrida).status == "em_andamento"


def test_socket_sem_quadro_confirmado_nao_assume_corrida_ativa(sessao_logs, id_corrida):
    with TestClient(app).websocket_connect("/ws/telemetria"):
        pass
    log = sessao_logs.scalar(select(LogEvento))
    assert log.id_corrida is None
    assert "desconexao" in log.mensagem


def test_fechamento_depois_do_fim_nao_gera_falsa_perda(sessao_logs, id_corrida):
    with TestClient(app).websocket_connect("/ws/telemetria") as ws:
        ws.send_text(format_frame(inicio()))
        ws.receive_text()
        ws.send_text(format_frame({"v": 1, "t": "evt", "corrida": 1, "seq": 1,
            "tempo_ms": 1000, "evento": "fim", "resultado": "concluida",
            "tempo_conc_s": 1.0, "consumo_medio_ma": 100.0}))
        ws.receive_text()
    assert not any("desconexao" in log.mensagem for log in sessao_logs.scalars(select(LogEvento)))


def test_descarte_preserva_contexto_e_bateria_critica_nao_duplica(sessao_logs, id_corrida):
    with TestClient(app).websocket_connect("/ws/telemetria") as ws:
        ws.send_text(format_frame(inicio()))
        ws.receive_text()
        # Campo corrida adulterado não pode reassociar o socket.
        corrompido = format_frame(inicio(999))
        crc_errado = "0000" if corrompido[-4:] != "0000" else "FFFF"
        ws.send_text(corrompido[:-4] + crc_errado)
        evento = {"v": 1, "t": "evt", "corrida": 1, "seq": 1,
                  "tempo_ms": 1000, "evento": "bateria_critica", "tensao_bateria": 6.9}
        for _ in range(2):
            ws.send_text(format_frame(evento))
            assert '"seq":1' in ws.receive_text()
    logs = sessao_logs.scalars(select(LogEvento)).all()
    descartes = [log for log in logs if log.tipo == "erro"]
    bateria = [log for log in logs if "evento bateria_critica" in log.mensagem]
    assert len(descartes) == 1 and descartes[0].id_corrida == id_corrida
    assert len(bateria) == 1 and bateria[0].id_corrida == id_corrida
    assert bateria[0].tipo == "aviso"


def test_contextos_de_duas_conexoes_nao_se_misturam(sessao_logs, id_corrida):
    client = TestClient(app)
    with client.websocket_connect("/ws/telemetria") as identificado:
        identificado.send_text(format_frame(inicio()))
        identificado.receive_text()
        with client.websocket_connect("/ws/telemetria"):
            pass
    avisos = sessao_logs.scalars(select(LogEvento).where(LogEvento.tipo == "aviso").order_by(LogEvento.id)).all()
    assert [log.id_corrida for log in avisos] == [None, id_corrida]


def test_consulta_combina_filtros_e_pagina_sem_duplicar(sessao_logs, id_corrida):
    dados = [(id_corrida, "erro", "backend"), (None, "erro", "backend"),
             (id_corrida, "aviso", "firmware"), (id_corrida, "erro", "backend")]
    for i, (corrida, tipo, origem) in enumerate(dados):
        sessao_logs.add(LogEvento(id_corrida=corrida, tipo=tipo, origem=origem, mensagem=f"evento {i}"))
    sessao_logs.commit()
    client = TestClient(app)
    params = {"id_corrida": id_corrida, "tipo": "erro", "origem": "backend", "limite": 1}
    primeira = client.get("/logs", params=params)
    segunda = client.get("/logs", params={**params, "deslocamento": 1})
    assert primeira.status_code == segunda.status_code == 200
    assert primeira.json()[0]["mensagem"] == "evento 3"
    assert segunda.json()[0]["mensagem"] == "evento 0"
    assert primeira.json()[0]["ocorrido_em"]
    assert client.get("/logs", params={**params, "deslocamento": 2}).json() == []
    assert len(client.get("/logs").json()) == 4
    assert client.get("/logs", params={"id_corrida": 999999}).json() == []


@pytest.mark.parametrize("params", [
    {"tipo": "qualquer"}, {"origem": "qualquer"}, {"id_corrida": 0},
    {"limite": 0}, {"limite": 201}, {"deslocamento": -1},
])
def test_consulta_rejeita_filtros_invalidos(params):
    assert TestClient(app).get("/logs", params=params).status_code == 422
