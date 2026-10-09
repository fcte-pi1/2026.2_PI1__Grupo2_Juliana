# Liga o gcov na etapa de link do ambiente native, para medir a cobertura dos testes.
Import("env")
env.Append(LINKFLAGS=["--coverage"])
