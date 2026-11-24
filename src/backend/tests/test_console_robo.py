"""Abrir e encerrar a corrida pelo painel manda 'A' e 'a' ao console Wi-Fi do robô."""

import socket
import threading

from app.services import console_robo


def test_ignora_host_que_nao_e_ip():
    console_robo.ip_robo = None
    console_robo.registrar_ip("testclient")
    assert console_robo.ip_robo is None
    assert console_robo.enviar("A") is False


def test_envia_o_comando_ao_console(monkeypatch):
    servidor = socket.socket()
    servidor.bind(("127.0.0.1", 0))
    servidor.listen(1)
    monkeypatch.setattr(console_robo, "PORTA_CONSOLE", servidor.getsockname()[1])
    recebido = []

    def aceitar():
        conn, _ = servidor.accept()
        with conn:
            recebido.append(conn.recv(16))

    t = threading.Thread(target=aceitar)
    t.start()
    console_robo.registrar_ip("127.0.0.1")
    assert console_robo.enviar("A") is True
    t.join(2)
    servidor.close()
    console_robo.ip_robo = None
    assert recebido == [b"A"]
