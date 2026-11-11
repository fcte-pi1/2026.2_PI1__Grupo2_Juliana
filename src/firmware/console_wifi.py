"""Console do robô pelo Wi-Fi (porta 23), para quando ele está na bateria.

A bateria e o USB não podem ficar ligados juntos; com a bateria, este script
faz o papel do monitor serial: mostra tudo o que o robô escreve e manda os
comandos digitados (i, f, a, c, l, s, m, e, p, z, r; Enter envia).

Uso:  python console_wifi.py <IP do robô>      (o IP aparece no boot e no roteador)
"""
import socket
import sys
import threading


def receber(conexao):
    while True:
        dados = conexao.recv(1024)
        if not dados:
            print("\n[conexão encerrada pelo robô]")
            break
        sys.stdout.write(dados.decode("utf-8", "replace"))
        sys.stdout.flush()


def main():
    if len(sys.argv) < 2:
        sys.exit("uso: python console_wifi.py <IP do robô>")
    conexao = socket.create_connection((sys.argv[1], 23), timeout=5)
    conexao.settimeout(None)
    threading.Thread(target=receber, args=(conexao,), daemon=True).start()
    print("Conectado. Digite um comando e Enter (Ctrl+C sai).")
    try:
        for linha in sys.stdin:
            conexao.sendall(linha.strip().encode())
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
