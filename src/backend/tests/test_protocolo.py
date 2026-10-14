"""Testes unitários do protocolo de telemetria e validação dos 7 passos (Contrato 4.4.7)."""

from app.services.protocol import calc_crc, format_frame, validate_frame


def test_crc16_calculo_padrao_contrato():
    """Valida o CRC-16/CCITT-FALSE com o vetor de teste do contrato: 123456789 -> 29B1."""
    assert calc_crc(b"123456789") == 0x29B1


def test_format_frame_anexa_crc_correto():
    tel = {
        "v": 1, "t": "tel", "corrida": 7, "seq": 42, "tempo_ms": 8400,
        "pos_x": 2, "pos_y": 1, "orientacao": "L", "paredes": 9,
        "velocidade": 0.312, "tensao_bateria": 7.86, "valor_flood": 3,
        "estado": "explorando"
    }
    frame = format_frame(tel)
    assert frame.endswith("|2917")


def test_validacao_passo_1_formato_invalido():
    assert validate_frame("").error == "formato invalido"
    assert validate_frame("json_sem_pipe").error == "formato invalido"
    assert validate_frame("{}|12").error == "formato invalido"
    assert validate_frame("{}|ZZZZ").error == "formato invalido"
    assert validate_frame("|1234").error == "formato invalido"


def test_validacao_passo_2_checksum_invalido():
    tel = {
        "v": 1, "t": "tel", "corrida": 7, "seq": 42, "tempo_ms": 8400,
        "pos_x": 2, "pos_y": 1, "orientacao": "L", "paredes": 9,
        "velocidade": 0.312, "tensao_bateria": 7.86, "valor_flood": 3,
        "estado": "explorando"
    }
    # Altera pos_x de 2 para 3 mantendo o CRC antigo 2917
    corrompido = format_frame({**tel, "pos_x": 3})[:-4] + "2917"
    res = validate_frame(corrompido)
    assert not res.valid
    assert "checksum invalido (recebido 2917, calculado 8E28)" in res.error


def test_validacao_passo_3_json_invalido():
    res = validate_frame("{nao_e_json|1234")
    # se o CRC não bater cai no checksum ou json
    assert not res.valid


def test_validacao_passo_4_versao_nao_suportada():
    base = {"v": 2, "t": "tel", "corrida": 7, "seq": 0, "tempo_ms": 0,
            "pos_x": 0, "pos_y": 0, "orientacao": "N", "paredes": 0,
            "velocidade": 0.0, "tensao_bateria": 8.0, "estado": "parado"}
    res = validate_frame(format_frame(base))
    assert not res.valid
    assert res.error == "versao nao suportada"


def test_validacao_passo_5_campos_obrigatorios_e_tipos():
    # Campo t ausente
    sem_t = {"v": 1, "corrida": 7, "seq": 0, "tempo_ms": 0}
    res = validate_frame(format_frame(sem_t))
    assert not res.valid
    assert res.error == "campo ausente: t"

    # Tipo inválido para corrida
    corrida_str = {"v": 1, "t": "tel", "corrida": "sete", "seq": 0, "tempo_ms": 0}
    res = validate_frame(format_frame(corrida_str))
    assert not res.valid
    assert res.error == "tipo invalido: corrida"


def test_validacao_passo_6_fora_do_intervalo():
    tel = {
        "v": 1, "t": "tel", "corrida": 7, "seq": 1, "tempo_ms": 200,
        "pos_x": 0, "pos_y": 0, "orientacao": "N", "paredes": 16,  # paredes > 15
        "velocidade": 0.5, "tensao_bateria": 7.4, "estado": "explorando"
    }
    res = validate_frame(format_frame(tel))
    assert not res.valid
    assert res.error == "fora do intervalo: paredes=16"

    # Orientação inválida
    tel["paredes"] = 5
    tel["orientacao"] = "X"
    res = validate_frame(format_frame(tel))
    assert not res.valid
    assert res.error == "fora do intervalo: orientacao=X"

    # Velocidade negativa
    tel["orientacao"] = "N"
    tel["velocidade"] = -1.0
    res = validate_frame(format_frame(tel))
    assert not res.valid
    assert res.error == "fora do intervalo: velocidade=-1.0"


def test_validacao_evento_inicio_com_seq_invalido():
    evt = {"v": 1, "t": "evt", "corrida": 7, "seq": 1, "tempo_ms": 0, "evento": "inicio", "tensao_bateria": 8.2}
    res = validate_frame(format_frame(evt))
    assert not res.valid
    assert res.error == "fora do intervalo: seq=1"


def test_validacao_quadro_valido():
    tel = {
        "v": 1, "t": "tel", "corrida": 7, "seq": 42, "tempo_ms": 8400,
        "pos_x": 2, "pos_y": 1, "orientacao": "L", "paredes": 9,
        "velocidade": 0.312, "tensao_bateria": 7.86, "valor_flood": 3,
        "estado": "explorando"
    }
    res = validate_frame(format_frame(tel))
    assert res.valid
    assert res.error is None
    assert res.corrida == 7
    assert res.seq == 42


def test_validacao_limites_labirinto():
    tel = {
        "v": 1, "t": "tel", "corrida": 7, "seq": 1, "tempo_ms": 200,
        "pos_x": 4, "pos_y": 1, "orientacao": "N", "paredes": 1,
        "velocidade": 0.1, "tensao_bateria": 8.0, "estado": "explorando"
    }
    # Labirinto 4x4 aceita pos_x de 0 a 3; pos_x = 4 deve ser rejeitado
    res = validate_frame(format_frame(tel), largura_celula=4, altura_celula=4)
    assert not res.valid
    assert res.error == "fora do intervalo: pos_x=4"


def test_validacao_evento_bateria_critica():
    evt = {"v": 1, "t": "evt", "corrida": 1, "seq": 10, "tempo_ms": 2000, "evento": "bateria_critica", "tensao_bateria": 6.8}
    res = validate_frame(format_frame(evt))
    assert res.valid

    evt_sem_tensao = {"v": 1, "t": "evt", "corrida": 1, "seq": 10, "tempo_ms": 2000, "evento": "bateria_critica"}
    assert validate_frame(format_frame(evt_sem_tensao)).error == "campo ausente: tensao_bateria"


def test_validacao_evento_reconexao():
    evt = {"v": 1, "t": "evt", "corrida": 1, "seq": 20, "tempo_ms": 5000, "evento": "reconexao", "offline_ms": 3000, "pendentes": 15}
    res = validate_frame(format_frame(evt))
    assert res.valid

    evt_sem_pendentes = {"v": 1, "t": "evt", "corrida": 1, "seq": 20, "tempo_ms": 5000, "evento": "reconexao", "offline_ms": 3000}
    assert validate_frame(format_frame(evt_sem_pendentes)).error == "campo ausente: pendentes"


def test_validacao_schemas_pydantic():
    from app.schemas.telemetry import TelemetryMessage, EventMessage, AckMessage
    tel = TelemetryMessage(
        v=1, t="tel", corrida=1, seq=0, tempo_ms=0,
        pos_x=0, pos_y=0, orientacao="N", paredes=5,
        velocidade=0.2, tensao_bateria=8.0, estado="explorando"
    )
    assert tel.t == "tel"
    evt = EventMessage(v=1, t="evt", corrida=1, seq=0, tempo_ms=0, evento="inicio", tensao_bateria=8.2)
    assert evt.evento == "inicio"
    ack = AckMessage(t="ack", corrida=1, seq=0)
    assert ack.seq == 0
