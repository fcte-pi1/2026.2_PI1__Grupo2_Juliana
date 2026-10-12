"""Testes das restrições de integridade da seção 5 da 4.4.4 - Persistência de Dados (RNF07-S)."""

import pytest
from sqlalchemy import text
from sqlalchemy.exc import IntegrityError

LEITURA_VALIDA = {
    "tempo_ms": 0,
    "pos_x": 0,
    "pos_y": 0,
    "orientacao": "N",
    "paredes": 10,
    "velocidade": 0.25,
    "tensao_bateria": 8.2,
    "valor_flood": 3,
}


def inserir_leitura(conexao, id_corrida, **alteracoes):
    dados = {**LEITURA_VALIDA, **alteracoes, "id_corrida": id_corrida}
    conexao.execute(
        text(
            "INSERT INTO leitura_telemetria "
            "(id_corrida, tempo_ms, pos_x, pos_y, orientacao, paredes, velocidade, tensao_bateria, valor_flood) "
            "VALUES (:id_corrida, :tempo_ms, :pos_x, :pos_y, :orientacao, :paredes, :velocidade, "
            ":tensao_bateria, :valor_flood)"
        ),
        dados,
    )


def deve_rejeitar(conexao, restricao, comando):
    """Executa o comando num savepoint e confere que o banco o recusou pela restrição esperada."""
    with pytest.raises(IntegrityError, match=restricao):
        with conexao.begin_nested():
            comando()


def contar(conexao, sql, **parametros):
    return conexao.execute(text(sql), parametros).scalar_one()


def test_seed_cria_os_tres_labirintos(conexao):
    nomes = conexao.execute(text("SELECT nome FROM labirinto WHERE nome IN ('4x4', '8x4', '12x4')")).scalars()
    assert sorted(nomes) == ["12x4", "4x4", "8x4"]


@pytest.mark.parametrize("paredes", [0, 15])
def test_leitura_valida_e_gravada(conexao, id_corrida, paredes):
    inserir_leitura(conexao, id_corrida, paredes=paredes)
    assert contar(conexao, "SELECT count(*) FROM leitura_telemetria WHERE id_corrida = :c", c=id_corrida) == 1


@pytest.mark.parametrize(
    ("campo", "valor", "restricao"),
    [
        ("paredes", 16, "ck_leitura_telemetria_paredes_valida"),
        ("paredes", -1, "ck_leitura_telemetria_paredes_valida"),
        ("orientacao", "X", "ck_leitura_telemetria_orientacao_valida"),
        ("pos_x", -1, "ck_leitura_telemetria_posicao_nao_negativa"),
        ("pos_y", -1, "ck_leitura_telemetria_posicao_nao_negativa"),
        ("tempo_ms", -1, "ck_leitura_telemetria_tempo_ms_nao_negativo"),
        ("velocidade", -0.1, "ck_leitura_telemetria_velocidade_nao_negativa"),
        ("tensao_bateria", -1, "ck_leitura_telemetria_tensao_nao_negativa"),
        ("valor_flood", -1, "ck_leitura_telemetria_valor_flood_nao_negativo"),
    ],
)
def test_leitura_com_valor_invalido_e_rejeitada(conexao, id_corrida, campo, valor, restricao):
    deve_rejeitar(conexao, restricao, lambda: inserir_leitura(conexao, id_corrida, **{campo: valor}))


def test_leitura_repetida_e_rejeitada(conexao, id_corrida):
    """Reenvio de um pacote já gravado após queda de conexão (RF25-S, RNF12-S)."""
    inserir_leitura(conexao, id_corrida)
    deve_rejeitar(
        conexao, "uq_leitura_telemetria_id_corrida_tempo_ms", lambda: inserir_leitura(conexao, id_corrida)
    )


def test_tentativa_repetida_no_mesmo_labirinto_e_rejeitada(conexao, id_corrida, id_labirinto):
    deve_rejeitar(
        conexao,
        "uq_corrida_id_labirinto_num_tentativa",
        lambda: conexao.execute(
            text("INSERT INTO corrida (id_labirinto, id_usuario, num_tentativa) VALUES (:l, 'pytest', 1)"),
            {"l": id_labirinto},
        ),
    )


def test_corrida_concluida_exige_tempo_de_conclusao(conexao, id_corrida):
    def concluir(tempo):
        conexao.execute(
            text("UPDATE corrida SET status = 'concluida', tempo_conc_s = :t WHERE id = :c"),
            {"t": tempo, "c": id_corrida},
        )

    deve_rejeitar(conexao, "ck_corrida_concluida_tem_tempo", lambda: concluir(None))
    concluir(42.5)
    assert contar(conexao, "SELECT count(*) FROM corrida WHERE id = :c AND status = 'concluida'", c=id_corrida) == 1


def test_status_invalido_e_rejeitado(conexao, id_corrida):
    deve_rejeitar(
        conexao,
        "ck_corrida_status_valido",
        lambda: conexao.execute(text("UPDATE corrida SET status = 'pausada' WHERE id = :c"), {"c": id_corrida}),
    )


@pytest.mark.parametrize(
    ("tipo", "origem", "restricao"),
    [
        ("critico", "backend", "ck_log_evento_tipo_valido"),
        ("erro", "telemetria", "ck_log_evento_origem_valida"),
    ],
)
def test_log_com_tipo_ou_origem_invalidos_e_rejeitado(conexao, tipo, origem, restricao):
    deve_rejeitar(
        conexao,
        restricao,
        lambda: conexao.execute(
            text("INSERT INTO log_evento (tipo, origem, mensagem) VALUES (:t, :o, 'teste')"),
            {"t": tipo, "o": origem},
        ),
    )


def test_labirinto_com_corridas_nao_pode_ser_excluido(conexao, id_corrida, id_labirinto):
    deve_rejeitar(
        conexao,
        "fk_corrida_id_labirinto_labirinto",
        lambda: conexao.execute(text("DELETE FROM labirinto WHERE id = :l"), {"l": id_labirinto}),
    )


def test_excluir_corrida_apaga_leituras_e_mantem_logs(conexao, id_corrida):
    inserir_leitura(conexao, id_corrida)
    conexao.execute(
        text("INSERT INTO log_evento (id_corrida, tipo, origem, mensagem) VALUES (:c, 'info', 'backend', 'log-pytest')"),
        {"c": id_corrida},
    )

    conexao.execute(text("DELETE FROM corrida WHERE id = :c"), {"c": id_corrida})

    assert contar(conexao, "SELECT count(*) FROM leitura_telemetria WHERE id_corrida = :c", c=id_corrida) == 0
    assert contar(conexao, "SELECT count(*) FROM log_evento WHERE mensagem = 'log-pytest' AND id_corrida IS NULL") == 1
