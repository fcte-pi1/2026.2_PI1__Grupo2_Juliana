import asyncio
import binascii
import json
import random

import websockets

from simulador import DIRECOES, corromper, gerar_corrida, gerar_labirinto, quadro, transmitir


def test_quadro_igual_aos_exemplos_do_contrato():
    tel = {"v": 1, "t": "tel", "corrida": 7, "seq": 42, "tempo_ms": 8400, "pos_x": 2, "pos_y": 1,
           "orientacao": "L", "paredes": 9, "velocidade": 0.312, "tensao_bateria": 7.86,
           "valor_flood": 3, "estado": "explorando"}
    assert quadro(tel).endswith("|2917")
    assert corromper(tel, "checksum").endswith('"pos_x":3,' + quadro(tel).split('"pos_x":2,')[1])
    assert quadro({"v": 1, "t": "evt", "corrida": 7, "seq": 0, "tempo_ms": 0, "evento": "inicio",
                   "tensao_bateria": 8.21}).endswith("|4886")


def test_labirinto_coerente_e_rota_sem_atravessar_paredes():
    paredes, rota = gerar_labirinto(8, 4, random.Random(1))
    for (x, y), m in paredes.items():
        for d, (dx, dy, bit) in DIRECOES.items():
            viz = (x + dx, y + dy)
            if viz not in paredes:
                assert m & bit, "borda sem parede"
            else:
                assert bool(m & bit) == bool(paredes[viz] & DIRECOES[{"N": "S", "S": "N", "L": "O", "O": "L"}[d]][2])
    assert rota[0] == (0, 0) and rota[-1] == (7, 3)
    for a, b in zip(rota, rota[1:]):
        d = next(d for d, (dx, dy, _) in DIRECOES.items() if (a[0] + dx, a[1] + dy) == b)
        assert not paredes[a] & DIRECOES[d][2]


def test_corrida_e_bateria_critica():
    msgs = [m for t in gerar_corrida(4, 4, 7, random.Random(2)) for m in t]
    assert msgs[0]["evento"] == "inicio" and msgs[-1]["evento"] == "fim"
    tels = [m for m in msgs if m["t"] == "tel"]
    assert all(b["tempo_ms"] - a["tempo_ms"] == 200 for a, b in zip(tels, tels[1:]))
    assert not any(m.get("evento") == "bateria_critica" for m in msgs)

    msgs = [m for t in gerar_corrida(4, 4, 7, random.Random(2), bateria_critica=True) for m in t]
    criticas = [m for m in msgs if m.get("evento") == "bateria_critica"]
    assert len(criticas) == 1 and criticas[0]["tensao_bateria"] < 7.0
    assert max(m["tensao_bateria"] for m in msgs if m["t"] == "tel" and m["tempo_ms"] > criticas[0]["tempo_ms"]) > 8


def test_transmissao_com_corrupcao_e_queda():
    recebidos, invalidos = {}, []

    async def servidor(ws):  # valida CRC e paredes como o backend e responde ack
        async for texto in ws:
            js, _, crc = texto.rpartition("|")
            msg = json.loads(js) if crc == f"{binascii.crc_hqx(js.encode(), 0xFFFF):04X}" else {}
            if "seq" not in msg or (msg["t"] == "tel" and not 0 <= msg.get("paredes", -1) <= 15):
                invalidos.append(texto)
                continue
            recebidos[msg["seq"]] = msg
            await ws.send(quadro({"t": "ack", "corrida": msg["corrida"], "seq": msg["seq"]}))

    async def rodar():
        async with websockets.serve(servidor, "127.0.0.1", 0) as srv:
            url = f"ws://127.0.0.1:{srv.sockets[0].getsockname()[1]}"
            ticks = gerar_corrida(4, 4, 7, random.Random(3))
            return await transmitir(url, ticks, 0.01, ["checksum", "campo", "faixa"], queda_s=1)

    total = asyncio.run(rodar())
    assert len(invalidos) == 3
    assert sorted(recebidos) == list(range(total)), "toda mensagem chega depois do reenvio"
    reconexao = [m for m in recebidos.values() if m.get("evento") == "reconexao"]
    assert len(reconexao) == 1 and reconexao[0]["offline_ms"] >= 1000 and reconexao[0]["pendentes"] >= 5
    assert any(m.get("evento") == "fim" for m in recebidos.values())
