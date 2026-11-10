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
    python nucleo1_risco.py esp32dev --verificar tarefaNavegacao RoboSimuladoNucleo1
        # falha se as funções do núcleo 1 (e o que elas chamam) caem num byte
        # defeituoso. A compilação roda isso sozinha (custom_nucleo1 no platformio.ini).
"""
import bisect
import glob
import os
import re
import subprocess
import sys

POSICOES = (0x2172, 0x2192)  # byte (endereço & 0x3FFF) com o bit grudado
BIT = 0x02
SECOES = (".flash.text", ".flash.rodata")


# Pasta do compilador. Dentro da compilação o nucleo1_verificar.py preenche com
# a do PlatformIO; fora dela, procura nos pacotes instalados.
PASTA_TOOLCHAIN = None


def ferramenta(nome):
    pastas = [PASTA_TOOLCHAIN] if PASTA_TOOLCHAIN else []
    nucleo = os.environ.get("PLATFORMIO_CORE_DIR", os.path.expanduser("~/.platformio"))
    pastas += glob.glob(os.path.join(nucleo, "packages", "toolchain-xtensa*", "bin"))
    for pasta in pastas:
        achados = glob.glob(os.path.join(pasta, f"xtensa-esp32-elf-{nome}*")) or glob.glob(
            os.path.join(pasta, f"xtensa-esp-elf-{nome}*"))
        if achados:
            return achados[0]
    raise FileNotFoundError(f"não achei o xtensa-esp32-elf-{nome} (procurei em {pastas})")


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


def bytes_perigosos(elf):
    """{seção: [endereços]} dos bytes que o núcleo 1 lê errado."""
    resultado = {}
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
        resultado[secao] = (tamanho, perigosos)
    return resultado


CABECALHO = re.compile(r"^([0-9a-f]{8}) <(.+)>:$")
CHAMADA = re.compile(r"\s(?:call(?:0|4|8|12)|j)\s+([0-9a-f]{8}) <([^>]+)>")


def grafo(elf):
    """Funções (início -> nome) e chamadas diretas (nome -> {nomes}), pelo objdump."""
    saida = subprocess.run([ferramenta("objdump"), "-d", "-C", elf], capture_output=True, text=True, check=True).stdout
    inicios, chamadas, atual = {}, {}, None
    for linha in saida.splitlines():
        m = CABECALHO.match(linha)
        if m:
            atual = m.group(2)
            inicios[int(m.group(1), 16)] = atual
            chamadas.setdefault(atual, set())
            continue
        m = CHAMADA.search(linha)
        if m and atual and "+0x" not in m.group(2):
            chamadas[atual].add(m.group(2))
    return inicios, chamadas


def verificar(elf, padroes):
    """Falha se o núcleo 1 alcança um byte defeituoso a partir das funções em padroes.

    Segue as chamadas diretas (call/j) a partir das raízes. Chamadas indiretas
    (ponteiro de função, métodos virtuais) não aparecem no objdump: as classes
    usadas assim no núcleo 1 precisam estar nos padrões. Um byte defeituoso
    conta para a função que o contém e também para a seguinte, porque o bloco de
    constantes (literal pool) de uma função fica logo antes dela.
    Devolve a lista de problemas (vazia = pode rodar no núcleo 1).
    """
    inicios, chamadas = grafo(elf)
    regex = [re.compile(p) for p in padroes]
    raizes = {n for n in chamadas if any(r.search(n) for r in regex)}
    alcancadas, pilha = set(), list(raizes)
    while pilha:
        f = pilha.pop()
        if f in alcancadas:
            continue
        alcancadas.add(f)
        pilha.extend(chamadas.get(f, ()))
    ordenados = sorted(inicios)
    problemas = []
    tabela = simbolos(elf)
    comecos = [t[0] for t in tabela]
    for secao, (_, perigosos) in bytes_perigosos(elf).items():
        for endereco in perigosos:
            # Constantes com o nome das funções do núcleo 1 (ex.: a vtable do
            # robô simulado, lida em toda chamada virtual).
            objeto = dono(tabela, comecos, endereco)
            if any(r.search(objeto) for r in regex):
                problemas.append(f"0x{endereco:08x} ({secao}) na constante {objeto}")
            i = bisect.bisect_right(ordenados, endereco) - 1
            candidatos = []
            if i >= 0:
                candidatos.append(inicios[ordenados[i]])
            if i + 1 < len(ordenados):
                candidatos.append(inicios[ordenados[i + 1]])
            for nome in candidatos:
                if nome in alcancadas:
                    problemas.append(f"0x{endereco:08x} ({secao}) em/antes de {nome}")
    return raizes, alcancadas, problemas


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    ambiente = args[0] if args else "esp32dev"
    elf = os.path.join(".pio", "build", ambiente, "firmware.elf")
    if not os.path.exists(elf):
        sys.exit(f"{elf} não existe: rode pio run -e {ambiente} antes")
    if "--verificar" in sys.argv:
        padroes = args[1:]
        raizes, alcancadas, problemas = verificar(elf, padroes)
        print(f"{len(raizes)} funções do núcleo 1, {len(alcancadas)} alcançadas pelas chamadas")
        inicios, _ = grafo(elf)
        na_flash = sorted(nome for endereco, nome in inicios.items()
                          if nome in alcancadas and 0x400D0000 <= endereco < 0x40400000)
        print(f"{len(na_flash)} delas ainda na flash (passam pela cache do núcleo 1):")
        for nome in na_flash:
            print("   ", nome)
        for p in problemas:
            print("  PROBLEMA:", p)
        sys.exit(1 if problemas or not raizes else 0)
    tabela = simbolos(elf)
    inicios = [s[0] for s in tabela]
    total = 0
    for secao, (tamanho, perigosos) in bytes_perigosos(elf).items():
        print(f"{secao}: {tamanho // 1024} KB, {len(perigosos)} bytes que o núcleo 1 lê errado")
        for endereco in perigosos:
            print(f"  0x{endereco:08x}  {dono(tabela, inicios, endereco)}")
        total += len(perigosos)
    print(f"\nTotal: {total}. O que estiver na lista não pode rodar (nem ser lido) no núcleo 1.")


if __name__ == "__main__":
    main()
