"""Testes unitários do protocolo de telemetria e validação dos 7 passos (Contrato 4.4.7)."""

from app.services.protocol import (
    calc_crc,
    format_frame,
    validate_frame,
    validate_limites_labirinto,
)


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
    # Testa string malformada com CRC válido
    json_invalido = "{nao_e_json: 123}"
    crc = calc_crc(json_invalido.encode("utf-8"))
    res = validate_frame(f"{json_invalido}|{crc:04X}")
    assert not res.valid
    assert res.error == "json invalido"

    # Testa rejeição de constante NaN com CRC válido
    json_nan = '{"v": 1, "t": "tel", "velocidade": NaN}'
    crc_nan = calc_crc(json_nan.encode("utf-8"))
    res_nan = validate_frame(f"{json_nan}|{crc_nan:04X}")
    assert not res_nan.valid
    assert res_nan.error == "json invalido"

    # Testa JSON que não é objeto (dict)
    json_lista = "[1, 2, 3]"
    crc_lista = calc_crc(json_lista.encode("utf-8"))
    res_lista = validate_frame(f"{json_lista}|{crc_lista:04X}")
    assert not res_lista.valid
    assert res_lista.error == "json invalido"


def test_validacao_passo_4_versao_nao_suportada():
    base = {"v": 2, "t": "tel", "corrida": 7, "seq": 0, "tempo_ms": 0,
            "pos_x": 0, "pos_y": 0, "orientacao": "N", "paredes": 0,
            "velocidade": 0.0, "tensao_bateria": 8.0, "estado": "parado"}
    res = validate_frame(format_frame(base))
    assert not res.valid
    assert res.error == "versao nao suportada"

    sem_v = {"t": "tel", "corrida": 7, "seq": 0, "tempo_ms": 0}
    assert validate_frame(format_frame(sem_v)).error == "campo ausente: v"


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

    # Faixas do cabeçalho
    assert validate_frame(format_frame({"v": 1, "t": "tel", "corrida": 0, "seq": 0, "tempo_ms": 0})).error == "fora do intervalo: corrida=0"
    assert validate_frame(format_frame({"v": 1, "t": "tel", "corrida": 1, "seq": -1, "tempo_ms": 0})).error == "fora do intervalo: seq=-1"
    assert validate_frame(format_frame({"v": 1, "t": "tel", "corrida": 1, "seq": 0, "tempo_ms": -5})).error == "fora do intervalo: tempo_ms=-5"


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

    # Velocidade negativa e acima do limite (5,000 m/s)
    tel["orientacao"] = "N"
    tel["velocidade"] = -1.0
    assert validate_frame(format_frame(tel)).error == "fora do intervalo: velocidade=-1.0"
    tel["velocidade"] = 5.001
    assert validate_frame(format_frame(tel)).error == "fora do intervalo: velocidade=5.001"

    # Tensão da bateria acima do limite (9,00 V) e negativa
    tel["velocidade"] = 0.5
    tel["tensao_bateria"] = 9.01
    assert validate_frame(format_frame(tel)).error == "fora do intervalo: tensao_bateria=9.01"
    tel["tensao_bateria"] = -0.5
    assert validate_frame(format_frame(tel)).error == "fora do intervalo: tensao_bateria=-0.5"


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
    assert res.data["corrida"] == 7
    assert res.data["seq"] == 42


def test_validacao_limites_labirinto():
    # Células internas válidas (0..3 em um labirinto 4x4)
    assert validate_limites_labirinto(pos_x=0, pos_y=0, largura_celula=4, altura_celula=4) is None
    assert validate_limites_labirinto(pos_x=3, pos_y=3, largura_celula=4, altura_celula=4) is None

    # Saídas válidas: exatamente 1 passo fora em um único eixo
    assert validate_limites_labirinto(pos_x=-1, pos_y=0, largura_celula=4, altura_celula=4) is None
    assert validate_limites_labirinto(pos_x=0, pos_y=-1, largura_celula=4, altura_celula=4) is None
    assert validate_limites_labirinto(pos_x=4, pos_y=3, largura_celula=4, altura_celula=4) is None
    assert validate_limites_labirinto(pos_x=2, pos_y=4, largura_celula=4, altura_celula=4) is None

    # Saídas inválidas: coordenadas distantes (> limite ou < -1)
    assert validate_limites_labirinto(pos_x=50, pos_y=3, largura_celula=4, altura_celula=4) == "fora do intervalo: pos_x=50"
    assert validate_limites_labirinto(pos_x=5, pos_y=1, largura_celula=4, altura_celula=4) == "fora do intervalo: pos_x=5"
    assert validate_limites_labirinto(pos_x=1, pos_y=5, largura_celula=4, altura_celula=4) == "fora do intervalo: pos_y=5"
    assert validate_limites_labirinto(pos_x=-2, pos_y=0, largura_celula=4, altura_celula=4) == "fora do intervalo: pos_x=-2"
    assert validate_limites_labirinto(pos_x=0, pos_y=-2, largura_celula=4, altura_celula=4) == "fora do intervalo: pos_y=-2"

    # Saídas inválidas: diagonais fora da matriz (ambos os eixos fora de 0..limite-1)
    assert validate_limites_labirinto(pos_x=4, pos_y=4, largura_celula=4, altura_celula=4) == "fora do intervalo: pos_x=4, pos_y=4"
    assert validate_limites_labirinto(pos_x=-1, pos_y=-1, largura_celula=4, altura_celula=4) == "fora do intervalo: pos_x=-1, pos_y=-1"
    assert validate_limites_labirinto(pos_x=-1, pos_y=4, largura_celula=4, altura_celula=4) == "fora do intervalo: pos_x=-1, pos_y=4"
    assert validate_limites_labirinto(pos_x=4, pos_y=-1, largura_celula=4, altura_celula=4) == "fora do intervalo: pos_x=4, pos_y=-1"


def test_validacao_frame_coordenadas_saida_e_invalidas():
    base = {
        "v": 1, "t": "tel", "corrida": 7, "seq": 10, "tempo_ms": 2000,
        "pos_x": 0, "pos_y": 0, "orientacao": "N", "paredes": 0,
        "velocidade": 0.25, "tensao_bateria": 7.9, "estado": "explorando",
    }

    # Saídas válidas no protocolo (-1 em um único eixo ou coordenadas positivas)
    for px, py in [(-1, 0), (0, -1), (4, 3), (2, 4)]:
        res = validate_frame(format_frame({**base, "pos_x": px, "pos_y": py}))
        assert res.valid, f"Esperava válido para ({px}, {py}), obteve erro: {res.error}"

    # Coordenadas inválidas no protocolo (< -1 ou diagonal (-1, -1))
    assert validate_frame(format_frame({**base, "pos_x": -2, "pos_y": 0})).error == "fora do intervalo: pos_x=-2"
    assert validate_frame(format_frame({**base, "pos_x": 0, "pos_y": -2})).error == "fora do intervalo: pos_y=-2"
    assert validate_frame(format_frame({**base, "pos_x": -1, "pos_y": -1})).error == "fora do intervalo: pos_x=-1, pos_y=-1"


def test_validacao_evento_fim_regras_contrato():
    base = {"v": 1, "t": "evt", "corrida": 1, "seq": 2, "tempo_ms": 1000, "evento": "fim"}

    # Resultado inválido ou ausente
    assert validate_frame(format_frame(base)).error == "campo ausente: resultado"
    assert validate_frame(format_frame({**base, "resultado": "invalido"})).error == "fora do intervalo: resultado=invalido"

    # Concluída sem tempo_conc_s
    assert validate_frame(format_frame({**base, "resultado": "concluida"})).error == "campo ausente: tempo_conc_s"

    # Concluída com tempo negativo
    fim_tempo_neg = {**base, "resultado": "concluida", "tempo_conc_s": -2.0}
    assert validate_frame(format_frame(fim_tempo_neg)).error == "fora do intervalo: tempo_conc_s=-2.0"

    # Consumo médio negativo
    fim_consumo_neg = {**base, "resultado": "concluida", "tempo_conc_s": 10.0, "consumo_medio_ma": -15.0}
    assert validate_frame(format_frame(fim_consumo_neg)).error == "fora do intervalo: consumo_medio_ma=-15.0"

    # Abandonada com motivo inválido
    fim_motivo_inv = {**base, "resultado": "abandonada", "motivo": "capotou"}
    assert validate_frame(format_frame(fim_motivo_inv)).error == "fora do intervalo: motivo=capotou"

    # Abandonada válida
    fim_aband_val = {**base, "resultado": "abandonada", "motivo": "bateria_critica"}
    assert validate_frame(format_frame(fim_aband_val)).valid


def test_validacao_evento_bateria_critica():
    evt = {"v": 1, "t": "evt", "corrida": 1, "seq": 10, "tempo_ms": 2000, "evento": "bateria_critica", "tensao_bateria": 6.8}
    res = validate_frame(format_frame(evt))
    assert res.valid

    evt_sem_tensao = {"v": 1, "t": "evt", "corrida": 1, "seq": 10, "tempo_ms": 2000, "evento": "bateria_critica"}
    assert validate_frame(format_frame(evt_sem_tensao)).error == "campo ausente: tensao_bateria"

    evt_tensao_neg = {"v": 1, "t": "evt", "corrida": 1, "seq": 10, "tempo_ms": 2000, "evento": "bateria_critica", "tensao_bateria": -1.0}
    assert validate_frame(format_frame(evt_tensao_neg)).error == "fora do intervalo: tensao_bateria=-1.0"


def test_validacao_evento_reconexao():
    evt = {"v": 1, "t": "evt", "corrida": 1, "seq": 20, "tempo_ms": 5000, "evento": "reconexao", "offline_ms": 3000, "pendentes": 15}
    res = validate_frame(format_frame(evt))
    assert res.valid

    evt_sem_pendentes = {"v": 1, "t": "evt", "corrida": 1, "seq": 20, "tempo_ms": 5000, "evento": "reconexao", "offline_ms": 3000}
    assert validate_frame(format_frame(evt_sem_pendentes)).error == "campo ausente: pendentes"

    evt_offline_neg = {"v": 1, "t": "evt", "corrida": 1, "seq": 20, "tempo_ms": 5000, "evento": "reconexao", "offline_ms": -10, "pendentes": 5}
    assert validate_frame(format_frame(evt_offline_neg)).error == "fora do intervalo: offline_ms=-10"
