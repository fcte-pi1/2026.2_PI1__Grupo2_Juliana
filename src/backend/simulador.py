"""Simulador de telemetria do Micromouse (atividade 1.4).

Imita o robô: gera um labirinto aleatório, percorre do canto (0, 0) até o canto
oposto e envia a corrida ao servidor por WebSocket seguindo o contrato de
telemetria (docs/4.4.7). Usado para desenvolver e testar o sistema web sem o robô
(CT-S01 a CT-S06).

Exemplos:
    python simulador.py                                  # corrida normal (CT-S01, CT-S02, CT-S04)
    python simulador.py --corromper checksum,campo,faixa # pacotes inválidos (CT-S03)
    python simulador.py --bateria-critica                # tensão cai para 6,4 V e volta (CT-S05)
    python simulador.py --queda 5                        # conexão cai por 5 s (CT-S06)

Convenção de coordenadas: x cresce para Leste e y cresce para Norte.
"""
import argparse
import asyncio
import binascii
import json
import random

import websockets

PERIODO_MS = 200  # telemetria a 5 Hz
TICKS_POR_CELULA = 5  # ~1 s por célula (0,18 m/s com giros)
ACK_TICKS = 10  # 2 s sem ack -> reenvia
TENTATIVAS = 3
CRITICA_V, REARME_V = 7.0, 7.2

# direção -> (dx, dy, bit da parede)
DIRECOES = {"N": (0, 1, 1), "L": (1, 0, 2), "S": (0, -1, 4), "O": (-1, 0, 8)}
OPOSTA = {"N": "S", "S": "N", "L": "O", "O": "L"}


def quadro(msg):
    """Serializa no formato <JSON>|<CRC>."""
    js = json.dumps(msg, separators=(",", ":"))
    return f"{js}|{binascii.crc_hqx(js.encode(), 0xFFFF):04X}"


def corromper(msg, tipo):
    """Gera a versão inválida de um pacote `tel` para o CT-S03."""
    if tipo == "checksum":  # erro de transmissão: muda pos_x e mantém o CRC antigo
        crc = quadro(msg)[-4:]
        return quadro({**msg, "pos_x": msg["pos_x"] + 1})[:-4] + crc
    if tipo == "campo":
        return quadro({k: v for k, v in msg.items() if k != "paredes"})
    if tipo == "faixa":
        return quadro({**msg, "paredes": 16})
    raise ValueError(tipo)


def gerar_labirinto(largura, altura, rng):
    """Labirinto perfeito por busca em profundidade a partir de (0, 0).

    Retorna a máscara de paredes de cada célula e a rota de (0, 0) até o canto oposto.
    """
    paredes = {(x, y): 15 for x in range(largura) for y in range(altura)}
    pai, pilha = {(0, 0): None}, [(0, 0)]
    while pilha:
        x, y = pilha[-1]
        livres = [(d, (x + dx, y + dy)) for d, (dx, dy, _) in DIRECOES.items()
                  if (x + dx, y + dy) in paredes and (x + dx, y + dy) not in pai]
        if not livres:
            pilha.pop()
            continue
        d, prox = rng.choice(livres)
        paredes[(x, y)] &= ~DIRECOES[d][2]
        paredes[prox] &= ~DIRECOES[OPOSTA[d]][2]
        pai[prox] = (x, y)
        pilha.append(prox)

    rota, celula = [], (largura - 1, altura - 1)
    while celula:
        rota.append(celula)
        celula = pai[celula]
    return paredes, rota[::-1]


def direcao(a, b):
    return next(d for d, (dx, dy, _) in DIRECOES.items() if (a[0] + dx, a[1] + dy) == b)


def gerar_corrida(largura, altura, corrida, rng, bateria_critica=False):
    """Mensagens da corrida agrupadas por tick de 200 ms (sem `seq`, que é do transporte)."""
    paredes, rota = gerar_labirinto(largura, altura, rng)
    total = len(rota) * TICKS_POR_CELULA
    queda_v = range(total // 3, total // 3 + 15) if bateria_critica else range(0)
    cab = {"v": 1, "corrida": corrida}
    ticks, abaixo_desde, armado = [], None, True

    for tick in range(total):
        i = tick // TICKS_POR_CELULA
        tempo_ms = tick * PERIODO_MS
        ultimo = tick == total - 1
        tensao = 6.40 if tick in queda_v else round(8.40 - 0.004 * tick + rng.uniform(-0.01, 0.01), 2)
        orientacao = direcao(rota[i], rota[i + 1]) if i + 1 < len(rota) else direcao(rota[i - 1], rota[i]) if i else "N"
        msgs = []
        if tick == 0:
            msgs.append({**cab, "t": "evt", "tempo_ms": 0, "evento": "inicio", "tensao_bateria": tensao})
        msgs.append({
            **cab, "t": "tel", "tempo_ms": tempo_ms,
            "pos_x": rota[i][0], "pos_y": rota[i][1], "orientacao": orientacao,
            "paredes": paredes[rota[i]],
            "velocidade": 0.0 if ultimo else round(0.18 + rng.uniform(-0.02, 0.02), 3),
            "tensao_bateria": tensao,
            "valor_flood": len(rota) - 1 - i,
            "estado": "parado" if ultimo else "explorando",
        })
        # bateria_critica: < 7,0 V por 1 s; só repete depois de voltar acima de 7,2 V
        if tensao < CRITICA_V:
            abaixo_desde = tempo_ms if abaixo_desde is None else abaixo_desde
            if armado and tempo_ms - abaixo_desde >= 1000:
                msgs.append({**cab, "t": "evt", "tempo_ms": tempo_ms, "evento": "bateria_critica", "tensao_bateria": tensao})
                armado = False
        else:
            abaixo_desde = None
            armado = armado or tensao > REARME_V
        if ultimo:
            msgs.append({**cab, "t": "evt", "tempo_ms": tempo_ms, "evento": "fim", "resultado": "concluida",
                         "tempo_conc_s": tempo_ms / 1000, "consumo_medio_ma": round(rng.uniform(380, 440), 1)})
        ticks.append(msgs)
    return ticks


async def transmitir(url, ticks, intervalo=PERIODO_MS / 1000, corromper_tipos=(), queda_s=0.0):
    """Envia as mensagens tick a tick com ack, reenvio e armazenamento durante quedas (seção 6 do contrato)."""
    pendentes = {}  # seq -> [quadro, tick do último envio, tentativas]
    corrompidos = dict(zip(range(3, 3 * len(corromper_tipos) + 1, 3), corromper_tipos))  # n-ésimo tel -> tipo
    queda = range(len(ticks) // 2, len(ticks) // 2 + round(queda_s * 1000 / PERIODO_MS)) if queda_s else range(0)
    ws, conectando, leitor, ja_conectou, offline_desde, seq, n_tel, tick = None, None, None, False, 0, 0, 0, 0

    async def ler_acks(conexao):
        try:
            async for texto in conexao:
                js, _, crc = texto.rpartition("|")
                if crc != f"{binascii.crc_hqx(js.encode(), 0xFFFF):04X}":
                    continue
                ack = json.loads(js)
                if ack.get("t") == "ack" and pendentes.pop(ack["seq"], None):
                    print(f"<- ack seq={ack['seq']}")
        except websockets.ConnectionClosed:
            pass

    async def cair(motivo):
        nonlocal ws, offline_desde
        print(f"!! {motivo}")
        await ws.close()
        ws, offline_desde = None, tick

    async def enviar(s, texto):
        try:
            await ws.send(texto)
            print(f"-> {texto}")
        except (websockets.ConnectionClosed, OSError):
            await cair("conexão perdida")
            return
        if s in pendentes:  # o ack pode chegar durante o send
            pendentes[s][1] = tick
            pendentes[s][2] += 1

    def novo(msg):
        nonlocal seq
        pendentes[seq] = [quadro({**msg, "seq": seq}), None, 0]
        seq += 1
        return seq - 1

    while tick < len(ticks) or (pendentes and tick < len(ticks) + len(queda) + ACK_TICKS * TENTATIVAS + 10):
        if tick in queda and ws:
            await cair(f"simulando queda de conexão por {queda_s} s")
        if ws is None and tick not in queda:
            # conecta em segundo plano para o robô seguir navegando enquanto tenta
            conectando = conectando or asyncio.ensure_future(websockets.connect(url, open_timeout=5))
            if conectando.done():
                if conectando.exception() is None:
                    ws = conectando.result()
                conectando = None
            if ws:
                leitor = asyncio.create_task(ler_acks(ws))
                antigos = sorted(pendentes)
                if ja_conectou:  # reconexao e depois os pendentes
                    s = novo({"v": 1, "t": "evt", "corrida": ticks[0][0]["corrida"], "tempo_ms": tick * PERIODO_MS,
                              "evento": "reconexao", "offline_ms": (tick - offline_desde) * PERIODO_MS,
                              "pendentes": len(antigos)})
                    await enviar(s, pendentes[s][0])
                for s in antigos:
                    if ws and s in pendentes:
                        await enviar(s, pendentes[s][0])
                ja_conectou = True

        for msg in ticks[tick] if tick < len(ticks) else []:
            s = novo(msg)
            if ws is None:
                continue
            tipo = None
            if msg["t"] == "tel":
                n_tel += 1
                tipo = corrompidos.get(n_tel)
            await enviar(s, corromper(msg | {"seq": s}, tipo) if tipo else pendentes[s][0])

        for s, (texto, enviado, tentativas) in list(pendentes.items()):
            if ws and s in pendentes and enviado is not None and tick - enviado >= ACK_TICKS:
                if tentativas >= TENTATIVAS:
                    print(f"xx seq={s} descartada após {TENTATIVAS} tentativas")
                    pendentes.pop(s)
                else:
                    await enviar(s, texto)

        tick += 1
        await asyncio.sleep(intervalo)

    if conectando:
        conectando.cancel()
    if ws:
        await ws.close()
    if leitor:
        await leitor
    if pendentes:
        print(f"xx {len(pendentes)} mensagens sem ack: seq {sorted(pendentes)}")
    return seq


def tamanho(texto):
    largura, altura = (int(n) for n in texto.lower().split("x"))
    return largura, altura


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--url", default="ws://localhost:8000/ws/telemetria")
    p.add_argument("--corrida", type=int, default=1, help="número da corrida no robô (padrão: 1)")
    p.add_argument("--labirinto", type=tamanho, default=(4, 4), help="LARGURAxALTURA em células (padrão: 4x4)")
    p.add_argument("--semente", type=int, help="semente do labirinto, para repetir a mesma corrida")
    p.add_argument("--intervalo", type=float, default=PERIODO_MS / 1000, help="segundos reais por tick de 200 ms")
    p.add_argument("--corromper", type=lambda s: s.split(","), default=[],
                   help="envia o 3º, 6º, 9º... pacote tel inválido: checksum, campo, faixa")
    p.add_argument("--bateria-critica", action="store_true", help="tensão cai para 6,4 V por 3 s no 1º terço da corrida")
    p.add_argument("--queda", type=float, default=0, metavar="SEGUNDOS", help="derruba a conexão no meio da corrida")
    a = p.parse_args()
    for tipo in a.corromper:
        if tipo not in ("checksum", "campo", "faixa"):
            p.error(f"--corromper: tipo desconhecido {tipo!r}")

    ticks = gerar_corrida(*a.labirinto, a.corrida, random.Random(a.semente), a.bateria_critica)
    asyncio.run(transmitir(a.url, ticks, a.intervalo, a.corromper, a.queda))


if __name__ == "__main__":
    main()
