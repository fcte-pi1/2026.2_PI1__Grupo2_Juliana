# Depois de gerar o firmware.elf, confere se o que roda no núcleo 1 escapa do
# bit defeituoso da cache dele (docs/7.3, teste 3.18). Se não escapar, a
# compilação falha: basta mudar qualquer coisa no código (o endereço das
# funções muda) ou tirar a função do núcleo 1.
#
# As funções do núcleo 1 ficam em custom_nucleo1 no platformio.ini (expressões
# regulares sobre o nome C++ completo). Vazio: não confere nada.
import os
import sys

Import("env")  # noqa: F821 (definido pelo PlatformIO)

sys.path.insert(0, env.subst("$PROJECT_DIR"))  # noqa: F821
import nucleo1_risco  # noqa: E402

PADROES = env.GetProjectOption("custom_nucleo1", "").split()  # noqa: F821


def conferir(target, source, env):
    if not PADROES:
        return
    elf = str(target[0])
    raizes, alcancadas, problemas = nucleo1_risco.verificar(elf, PADROES)
    if not raizes:
        sys.stderr.write(f"\n[nucleo1] nenhuma função casou com {PADROES}\n")
        env.Exit(1)
    if problemas:
        sys.stderr.write("\n[nucleo1] código do núcleo 1 cai no bit defeituoso da cache:\n")
        for p in problemas:
            sys.stderr.write(f"  {p}\n")
        env.Exit(1)
    print(f"[nucleo1] OK: {len(raizes)} funções do núcleo 1 e {len(alcancadas)} alcançadas, "
          "nenhuma no bit defeituoso")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", conferir)  # noqa: F821
