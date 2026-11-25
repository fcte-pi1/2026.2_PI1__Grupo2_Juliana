"""Comandos para o console Wi-Fi do robô (porta 23, um caractere por comando).

O IP vem da última conexão do robô em /ws/telemetria, então não há nada para
configurar: com o robô conectado, abrir a corrida pelo painel já manda 'A'
(corrida automática).
"""

import ipaddress
import logging
import socket

logger = logging.getLogger("console_robo")

PORTA_CONSOLE = 23
ip_robo: str | None = None


def registrar_ip(host: str | None) -> None:
    global ip_robo
    try:
        ipaddress.ip_address(host or "")
    except ValueError:
        return  # cliente de teste ("testclient") ou sem endereço
    ip_robo = host


def enviar(comando: str) -> bool:
    """Envia o comando; False se o robô nunca conectou ou não respondeu em 1 s."""
    if not ip_robo:
        logger.warning("Robô sem IP conhecido: comando %r não enviado", comando)
        return False
    try:
        with socket.create_connection((ip_robo, PORTA_CONSOLE), timeout=1) as s:
            s.sendall(comando.encode())
        logger.info("Comando %r enviado ao robô em %s", comando, ip_robo)
        return True
    except OSError as erro:
        logger.warning("Falha ao enviar %r ao robô em %s: %s", comando, ip_robo, erro)
        return False
