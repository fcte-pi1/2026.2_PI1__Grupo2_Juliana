"""Testes de integração para os endpoints WebSocket de telemetria e painel."""

from contextlib import contextmanager
from decimal import Decimal
import pytest
from fastapi.testclient import TestClient
from sqlalchemy import func, select
from sqlalchemy.orm import Session
from starlette.websockets import WebSocketDisconnect

from app.core.websocket_manager import manager
from app.db.models import Corrida, LeituraTelemetria, LogEvento
from app.main import app
from app.services.protocol import format_frame
from app.services.telemetry_service import telemetry_service


@pytest.fixture
def db_session(conexao, monkeypatch):
    """Sessão SQLAlchemy vinculada à mesma transação do fixture conexao."""
    telemetry_service.reset()
    sess = Session(bind=conexao, join_transaction_mode="create_savepoint")

    @contextmanager
    def fake_session_scope():
        yield sess

    monkeypatch.setattr("app.routers.telemetry.session_scope", fake_session_scope)
    yield sess
    sess.close()
    telemetry_service.reset()


def test_ws_telemetria_ciclo_completo_com_ack(db_session, id_corrida):
    """Testa envio de evento de início, telemetria periódica e evento de fim com recebimento de ACKs."""
    client = TestClient(app)

    with client.websocket_connect("/ws/telemetria") as ws:
        # 1. Evento de início
        inicio = {
            "v": 1, "t": "evt", "corrida": 1, "seq": 0, "tempo_ms": 0,
            "evento": "inicio", "tensao_bateria": 8.25
        }
        ws.send_text(format_frame(inicio))
        ack_inicio = ws.receive_text()
        assert ack_inicio.startswith('{"t":"ack","corrida":1,"seq":0}|')

        # 2. Pacote de telemetria
        tel = {
            "v": 1, "t": "tel", "corrida": 1, "seq": 1, "tempo_ms": 200,
            "pos_x": 0, "pos_y": 0, "orientacao": "N", "paredes": 11,
            "velocidade": 0.250, "tensao_bateria": 8.20, "valor_flood": 0,
            "estado": "explorando"
        }
        ws.send_text(format_frame(tel))
        ack_tel = ws.receive_text()
        assert ack_tel.startswith('{"t":"ack","corrida":1,"seq":1}|')

        # 3. Evento de fim
        fim = {
            "v": 1, "t": "evt", "corrida": 1, "seq": 2, "tempo_ms": 10500,
            "evento": "fim", "resultado": "concluida", "tempo_conc_s": 10.500,
            "consumo_medio_ma": 350.2
        }
        ws.send_text(format_frame(fim))
        ack_fim = ws.receive_text()
        assert ack_fim.startswith('{"t":"ack","corrida":1,"seq":2}|')

    # Valida persistência no banco
    leitura = db_session.scalar(select(LeituraTelemetria).where(LeituraTelemetria.id_corrida == id_corrida))
    assert leitura is not None
    assert leitura.tempo_ms == 200
    assert leitura.paredes == 11
    assert leitura.velocidade == Decimal("0.250")

    # Corrida deve estar concluída com tempo gravado
    corrida = db_session.get(Corrida, id_corrida)
    assert corrida.status == "concluida"
    assert corrida.tempo_conc_s == Decimal("10.500")


def test_ws_telemetria_pacote_corrompido_descartado_e_registrado_em_log(db_session, id_corrida):
    """Testa que pacotes com checksum corrompido não recebem ACK e são registrados em log_evento."""
    client = TestClient(app)

    with client.websocket_connect("/ws/telemetria") as ws:
        # Inicia a corrida
        inicio = {
            "v": 1, "t": "evt", "corrida": 1, "seq": 0, "tempo_ms": 0,
            "evento": "inicio", "tensao_bateria": 8.25
        }
        ws.send_text(format_frame(inicio))
        ws.receive_text()

        # Envia pacote de telemetria com CRC adulterado
        tel = {
            "v": 1, "t": "tel", "corrida": 1, "seq": 1, "tempo_ms": 200,
            "pos_x": 0, "pos_y": 0, "orientacao": "N", "paredes": 11,
            "velocidade": 0.250, "tensao_bateria": 8.20, "valor_flood": 0,
            "estado": "explorando"
        }
        quadro_corrompido = format_frame(tel)[:-4] + "0000"
        ws.send_text(quadro_corrompido)

        # Envia um próximo pacote válido para comprovar que a conexão permanece viva
        tel_valido = {**tel, "seq": 2, "tempo_ms": 400}
        ws.send_text(format_frame(tel_valido))
        ack = ws.receive_text()
        # O ACK recebido deve ser o do seq 2 (o corrompido não gerou resposta)
        assert ack.startswith('{"t":"ack","corrida":1,"seq":2}|')

    # Verifica se o erro foi registrado no log
    log = db_session.scalar(select(LogEvento).where(LogEvento.tipo == "erro").order_by(LogEvento.id.desc()))
    assert log is not None
    assert "checksum invalido" in log.mensagem
    assert log.origem == "backend"


def test_ws_telemetria_corrida_desconhecida_e_rejeitada(db_session):
    """Mensagens enviadas sem nenhuma corrida em andamento devem ser descartadas como corrida desconhecida."""
    client = TestClient(app)

    with client.websocket_connect("/ws/telemetria") as ws:
        inicio = {
            "v": 1, "t": "evt", "corrida": 999, "seq": 0, "tempo_ms": 0,
            "evento": "inicio", "tensao_bateria": 8.25
        }
        ws.send_text(format_frame(inicio))

    log = db_session.scalar(select(LogEvento).where(LogEvento.mensagem == "corrida desconhecida: 999"))
    assert log is not None
    assert log.tipo == "erro"


def test_ws_telemetria_faz_broadcast_para_o_manager(db_session, id_corrida, monkeypatch):
    """Testa que os dados válidos recebidos do robô disparam o broadcast para o painel."""
    broadcasts = []

    async def fake_broadcast(msg):
        broadcasts.append(msg)

    monkeypatch.setattr(manager, "broadcast", fake_broadcast)

    client = TestClient(app)
    with client.websocket_connect("/ws/telemetria") as ws:
        inicio = {
            "v": 1, "t": "evt", "corrida": 1, "seq": 0, "tempo_ms": 0,
            "evento": "inicio", "tensao_bateria": 8.25
        }
        ws.send_text(format_frame(inicio))
        ws.receive_text()

        tel = {
            "v": 1, "t": "tel", "corrida": 1, "seq": 1, "tempo_ms": 200,
            "pos_x": 1, "pos_y": 1, "orientacao": "N", "paredes": 3,
            "velocidade": 0.4, "tensao_bateria": 8.0, "estado": "explorando"
        }
        ws.send_text(format_frame(tel))
        ws.receive_text()

    assert len(broadcasts) == 2
    assert broadcasts[0]["evento"] == "inicio"
    assert broadcasts[1]["estado"] == "explorando"
    assert broadcasts[1]["pos_x"] == 1


def test_ws_telemetria_tel_duplicada_recebe_ack_mas_nao_faz_broadcast(db_session, id_corrida, monkeypatch):
    """Testa que reenvios de leitura de telemetria recebem ACK mas não gravam nem retransmitem."""
    broadcasts = []

    async def fake_broadcast(msg):
        broadcasts.append(msg)

    monkeypatch.setattr(manager, "broadcast", fake_broadcast)

    client = TestClient(app)
    with client.websocket_connect("/ws/telemetria") as ws:
        inicio = {
            "v": 1, "t": "evt", "corrida": 1, "seq": 0, "tempo_ms": 0,
            "evento": "inicio", "tensao_bateria": 8.25
        }
        ws.send_text(format_frame(inicio))
        ws.receive_text()

        tel = {
            "v": 1, "t": "tel", "corrida": 1, "seq": 1, "tempo_ms": 200,
            "pos_x": 0, "pos_y": 0, "orientacao": "N", "paredes": 11,
            "velocidade": 0.250, "tensao_bateria": 8.20, "estado": "explorando"
        }
        # Envio 1
        ws.send_text(format_frame(tel))
        ack1 = ws.receive_text()
        assert ack1.startswith('{"t":"ack","corrida":1,"seq":1}|')

        # Envio 2 (reenvio com mesmo tempo_ms)
        ws.send_text(format_frame(tel))
        ack2 = ws.receive_text()
        assert ack2.startswith('{"t":"ack","corrida":1,"seq":1}|')

    # Apenas 1 broadcast de tel deve ter ocorrido (mais o do início)
    tels_broadcast = [b for b in broadcasts if b.get("t") == "tel"]
    assert len(tels_broadcast) == 1

    # Apenas 1 registro gravado no banco
    qtd = db_session.scalar(select(func.count()).select_from(LeituraTelemetria).where(LeituraTelemetria.id_corrida == id_corrida))
    assert qtd == 1


def test_ws_telemetria_reenvio_fim_recebe_ack_e_nao_duplica(db_session, id_corrida):
    """Testa que o reenvio de evento fim recebe ACK mesmo após a corrida ter sido concluída."""
    client = TestClient(app)
    with client.websocket_connect("/ws/telemetria") as ws:
        inicio = {
            "v": 1, "t": "evt", "corrida": 1, "seq": 0, "tempo_ms": 0,
            "evento": "inicio", "tensao_bateria": 8.25
        }
        ws.send_text(format_frame(inicio))
        ws.receive_text()

        fim = {
            "v": 1, "t": "evt", "corrida": 1, "seq": 1, "tempo_ms": 5000,
            "evento": "fim", "resultado": "concluida", "tempo_conc_s": 5.0,
            "consumo_medio_ma": 400.0
        }
        # Envio 1 do fim
        ws.send_text(format_frame(fim))
        ack1 = ws.receive_text()
        assert ack1.startswith('{"t":"ack","corrida":1,"seq":1}|')

        # Reenvio do fim (ex.: ack perdido)
        ws.send_text(format_frame(fim))
        ack2 = ws.receive_text()
        assert ack2.startswith('{"t":"ack","corrida":1,"seq":1}|')

    # Confirma que não houve registro de erro "corrida desconhecida"
    erros = db_session.scalars(select(LogEvento).where(LogEvento.tipo == "erro")).all()
    assert len(erros) == 0


def test_ws_telemetria_reenvio_inicio_recebe_ack_e_nao_duplica(db_session, id_corrida, monkeypatch):
    """Testa que o reenvio de evento inicio recebe ACK e não duplica log nem broadcast."""
    broadcasts = []

    async def fake_broadcast(msg):
        broadcasts.append(msg)

    monkeypatch.setattr(manager, "broadcast", fake_broadcast)

    client = TestClient(app)
    with client.websocket_connect("/ws/telemetria") as ws:
        inicio = {
            "v": 1, "t": "evt", "corrida": 1, "seq": 0, "tempo_ms": 0,
            "evento": "inicio", "tensao_bateria": 8.25
        }
        # Envio 1
        ws.send_text(format_frame(inicio))
        ack1 = ws.receive_text()
        assert ack1.startswith('{"t":"ack","corrida":1,"seq":0}|')

        # Reenvio de início (ex.: ACK perdido pelo robô)
        ws.send_text(format_frame(inicio))
        ack2 = ws.receive_text()
        assert ack2.startswith('{"t":"ack","corrida":1,"seq":0}|')

    # Broadcast deve ter ocorrido exatamente uma vez
    inicios_broadcast = [b for b in broadcasts if b.get("evento") == "inicio"]
    assert len(inicios_broadcast) == 1

    # Log de início deve ser registrado apenas uma vez no banco
    logs_inicio = db_session.scalars(
        select(LogEvento).where(LogEvento.id_corrida == id_corrida).where(LogEvento.tipo == "info")
    ).all()
    assert len(logs_inicio) == 1
    assert "evento inicio" in logs_inicio[0].mensagem


def test_ws_telemetria_evento_fim_abandonada_registra_aviso(db_session, id_corrida):
    """Testa que evento de fim com resultado 'abandonada' grava status abandonada e log tipo 'aviso'."""
    client = TestClient(app)
    with client.websocket_connect("/ws/telemetria") as ws:
        inicio = {
            "v": 1, "t": "evt", "corrida": 1, "seq": 0, "tempo_ms": 0,
            "evento": "inicio", "tensao_bateria": 8.25
        }
        ws.send_text(format_frame(inicio))
        ws.receive_text()

        fim = {
            "v": 1, "t": "evt", "corrida": 1, "seq": 1, "tempo_ms": 3000,
            "evento": "fim", "resultado": "abandonada", "motivo": "bateria_critica",
            "consumo_medio_ma": 390.0
        }
        ws.send_text(format_frame(fim))
        ack = ws.receive_text()
        assert ack.startswith('{"t":"ack","corrida":1,"seq":1}|')

    corrida = db_session.get(Corrida, id_corrida)
    assert corrida.status == "abandonada"

    log = db_session.scalar(select(LogEvento).where(LogEvento.tipo == "aviso"))
    assert log is not None
    assert "bateria_critica" in log.mensagem


def test_ws_telemetria_limite_labirinto_descartado_com_log(db_session, id_corrida):
    """Testa que coordenadas que extrapolam o labirinto são descartadas sem ACK e gravadas em log."""
    client = TestClient(app)
    with client.websocket_connect("/ws/telemetria") as ws:
        inicio = {
            "v": 1, "t": "evt", "corrida": 1, "seq": 0, "tempo_ms": 0,
            "evento": "inicio", "tensao_bateria": 8.25
        }
        ws.send_text(format_frame(inicio))
        ws.receive_text()

        # Labirinto de teste tem largura=4, altura=4 (coordenadas válidas 0..3)
        tel_fora = {
            "v": 1, "t": "tel", "corrida": 1, "seq": 1, "tempo_ms": 200,
            "pos_x": 4, "pos_y": 0, "orientacao": "N", "paredes": 1,
            "velocidade": 0.2, "tensao_bateria": 8.0, "estado": "explorando"
        }
        ws.send_text(format_frame(tel_fora))

        # Envia próximo pacote válido para confirmar que a conexão continua aberta
        tel_valido = {**tel_fora, "seq": 2, "tempo_ms": 400, "pos_x": 1}
        ws.send_text(format_frame(tel_valido))
        ack = ws.receive_text()
        assert ack.startswith('{"t":"ack","corrida":1,"seq":2}|')

    log = db_session.scalar(select(LogEvento).where(LogEvento.mensagem == "fora do intervalo: pos_x=4"))
    assert log is not None
    assert log.tipo == "erro"


@pytest.mark.anyio
async def test_connection_manager_broadcast_multiplos_clientes():
    """Valida broadcast simultâneo para múltiplos clientes do painel (RNF13-S)."""
    from unittest.mock import AsyncMock
    cm = manager.__class__()

    clientes = [AsyncMock() for _ in range(5)]
    for c in clientes:
        await cm.connect(c)

    assert cm.total_clients == 5

    payload = {"t": "tel", "pos_x": 2, "pos_y": 1, "estado": "rapido"}
    await cm.broadcast(payload)

    import json
    expected = json.dumps(payload)
    for c in clientes:
        c.send_text.assert_awaited_once_with(expected)

    # Testa desconexão
    await cm.disconnect(clientes[0])
    assert cm.total_clients == 4


def test_ws_telemetria_trata_excecao_com_traceback(db_session, monkeypatch):
    """Testa que exceções inesperadas no WebSocket registram traceback via logger.exception."""
    excecoes_registradas = []

    def fake_exception(msg, *args, **kwargs):
        excecoes_registradas.append(msg)

    monkeypatch.setattr("app.routers.telemetry.logger.exception", fake_exception)

    async def fake_process_frame(*args, **kwargs):
        raise RuntimeError("falha simulada")

    monkeypatch.setattr(telemetry_service, "process_frame", fake_process_frame)

    client = TestClient(app)
    with client.websocket_connect("/ws/telemetria") as ws:
        ws.send_text("qualquer_coisa")

    assert "Erro inesperado no WebSocket de telemetria" in excecoes_registradas


def test_ws_painel_trata_excecao_com_traceback(monkeypatch):
    """Testa que erro inesperado no WebSocket do painel registra traceback via logger.exception."""
    from starlette.websockets import WebSocket

    excecoes_registradas = []

    def fake_exception(msg, *args, **kwargs):
        excecoes_registradas.append(msg)

    monkeypatch.setattr("app.routers.telemetry.logger.exception", fake_exception)

    async def fake_receive_text(self):
        raise RuntimeError("erro painel simulado")

    monkeypatch.setattr(WebSocket, "receive_text", fake_receive_text)

    client = TestClient(app)
    with client.websocket_connect("/ws/painel"):
        pass

    assert "Erro no WebSocket do painel" in excecoes_registradas
