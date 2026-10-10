# Firmware

Firmware do Micromouse para o ESP32-DEVKIT-V1 em C++ com o framework Arduino, usando o [PlatformIO](https://platformio.org/).

## Requisitos
- PlatformIO: extensão do VS Code ou `pip install platformio`
- Para rodar os testes no computador: compilador `gcc`/`g++` instalado (no Windows, MinGW)

## Estrutura
- `src/`: código que só roda no robô (`setup`, `loop`, drivers).
- `lib/`: lógica independente do hardware (mapa, Flood Fill, filtro, telemetria). É o código testado no computador.
- `test/`: testes com [Unity](https://github.com/ThrowTheSwitch/Unity), um diretório `test_<nome>/` por módulo.

## Compilar e gravar no robô
```bash
cd src/firmware
pio run -e esp32dev                    # compila
pio run -e esp32dev -t upload          # grava pela USB
pio device monitor                     # abre o monitor serial (115200)
```

## Testar no computador
```bash
pio test -e native
gcovr -r . --filter lib/ .pio/build/native   # cobertura (pip install gcovr)
```
O GitHub Actions roda os testes, a cobertura e a compilação para o ESP32 em todo Pull Request.
