"""Lista o que não pode rodar no núcleo 1 da ESP32 da placa atual.

A cache do núcleo 1 desta ESP32 tem um bit grudado em 0 (teste 3.18 do docs/7.3):
o bit 1 dos bytes cujo endereço termina em 0x2172 ou 0x2192 dentro de cada bloco
de 16 KB. Esse byte só vem errado quando o valor certo do bit é 1. O script acha
esses bytes no firmware compilado (código e constantes da flash) e diz a que
função ou variável cada um pertence. O que aparecer aqui não pode ser executado
nem lido pelo núcleo 1; o resto pode.

Uso (em src/firmware, depois de compilar):
    python nucleo1_risco.py                     # ambiente esp32dev
    python nucleo1_risco.py teste_nucleo1       # outro ambiente
"""
import bisect
import glob
import os
import subprocess
import sys

POSICOES = (0x2172, 0x2192)  # byte (endereço & 0x3FFF) com o bit grudado
BIT = 0x02
SECOES = (".flash.text", ".flash.rodata")


def ferramenta(nome):
    achados = glob.glob(os.path.expanduser(f"~/.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-{nome}*"))
    if not achados:
        sys.exit(f"não achei o xtensa-esp32-elf-{nome} do PlatformIO")
    return achados[0]


def secoes(elf):
    """(nome, endereço, tamanho) das seções da flash."""
    saida = subprocess.run([ferramenta("objdump"), "-h", elf], capture_output=True, text=True, check=True).stdout
    for linha in saida.splitlines():
        partes = linha.split()
        if len(partes) >= 4 and partes[1] in SECOES:
            yield partes[1], int(partes[3], 16), int(partes[2], 16)


def conteudo(elf, secao):
    destino = elf + secao.replace(".", "_") + ".bin"
    subprocess.run([ferramenta("objcopy"), "-O", "binary", f"--only-section={secao}", elf, destino], check=True)
    with open(destino, "rb") as f:
        dados = f.read()
    os.remove(destino)
    return dados


def simbolos(elf):
    saida = subprocess.run([ferramenta("nm"), "-n", "-S", "-C", elf], capture_output=True, text=True, check=True).stdout
    tabela = []
    for linha in saida.splitlines():
        partes = linha.split(maxsplit=3)
        if len(partes) == 4:
            endereco, tamanho, _tipo, nome = partes
            tabela.append((int(endereco, 16), int(tamanho, 16), nome))
    tabela.sort()
    return tabela


def dono(tabela, inicios, endereco):
    i = bisect.bisect_right(inicios, endereco) - 1
    while i >= 0:
        inicio, tamanho, nome = tabela[i]
        if inicio <= endereco < inicio + max(tamanho, 1):
            return nome
        if inicio + 0x4000 < endereco:
            break
        i -= 1
    return "?"


def main():
    ambiente = sys.argv[1] if len(sys.argv) > 1 else "esp32dev"
    elf = os.path.join(".pio", "build", ambiente, "firmware.elf")
    if not os.path.exists(elf):
        sys.exit(f"{elf} não existe: rode pio run -e {ambiente} antes")
    tabela = simbolos(elf)
    inicios = [s[0] for s in tabela]
    total = 0
    for secao, base, tamanho in secoes(elf):
        dados = conteudo(elf, secao)
        perigosos = []
        bloco = base & ~0x3FFF
        while bloco < base + tamanho:
            for pos in POSICOES:
                endereco = bloco + pos
                if base <= endereco < base + len(dados) and dados[endereco - base] & BIT:
                    perigosos.append(endereco)
            bloco += 0x4000
        print(f"{secao}: {tamanho // 1024} KB, {len(perigosos)} bytes que o núcleo 1 lê errado")
        for endereco in perigosos:
            print(f"  0x{endereco:08x}  {dono(tabela, inicios, endereco)}")
        total += len(perigosos)
    print(f"\nTotal: {total}. O que estiver na lista não pode rodar (nem ser lido) no núcleo 1.")


if __name__ == "__main__":
    main()
