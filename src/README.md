# Software

Código-fonte do sistema de software do Micromouse: firmware do robô, servidor de telemetria, dashboard web e testes de ponta a ponta.

```
ESP32 (firmware) --Wi-Fi/WebSocket--> Backend (FastAPI + PostgreSQL) --> Frontend (React)
```

O formato das mensagens entre robô e servidor está no [Contrato de Telemetria](../docs/4.4.7%20-%20Contrato%20de%20Telemetria.md). A arquitetura completa está em [4.4.3 - Arquitetura da Solução](../docs/4.4.3%20-%20Arquitetura%20da%20Solucao.md).

## Estrutura

| Pasta | Conteúdo | Tecnologia | Como rodar |
| :--- | :--- | :--- | :--- |
| [`firmware/`](firmware/README.md) | Leitura dos sensores, telemetria por Wi-Fi com buffer para quedas, OTA, mapa do labirinto, Flood Fill, modo automático e testes de bancada | C++ / PlatformIO, ESP32-DEVKIT-V1 | `pio test -e native` |
| [`backend/`](backend/README.md) | Recebe a telemetria, grava corridas, tentativas e logs, e exporta CSV | Python 3.13, FastAPI, PostgreSQL 16, Alembic | `uvicorn app.main:app --reload` |
| [`frontend/`](frontend/README.md) | Dashboard em tempo real, histórico de corridas e logs | React, TypeScript, Vite | `npm run dev` |
| [`e2e/`](e2e/README.md) | Casos de teste do sistema web (CT-S) automatizados no navegador, com o simulador ou com o robô real | Playwright, TypeScript | `npx playwright test` |

Cada pasta tem um README próprio com instalação, execução e testes. O `backend/simulador.py` simula o robô e serve para ver o dashboard funcionando sem o hardware.

## Subindo o sistema completo (sem o robô)

```bash
# 1. banco de dados e backend
cd src/backend
cp .env.example .env
docker compose up -d
pip install -r requirements-dev.txt
alembic upgrade head
uvicorn app.main:app --reload        # http://localhost:8000/docs

# 2. dashboard (outro terminal)
cd src/frontend
npm install
npm run dev                          # http://localhost:5173

# 3. abra uma corrida no dashboard (ou POST /corridas) e, em outro terminal, rode o robô simulado
cd src/backend
python simulador.py
```

## Com o robô de verdade

1. Suba o banco e o dashboard como acima, mas o backend precisa aceitar conexões da rede: `uvicorn app.main:app --host 0.0.0.0 --port 8000`.
2. No firmware, copie `include/credenciais.exemplo.h` para `include/credenciais.h` com o Wi-Fi (2,4 GHz) e o IP do computador. O robô e o computador precisam estar na mesma rede (veja o [README do firmware](firmware/README.md)).
3. Grave o firmware (`pio run -e esp32dev -t upload`) e abra o monitor serial (`pio device monitor`). Quando aparecer `srv=1`, o robô está ligado ao backend.
4. Abra uma corrida no dashboard e inicie no robô: tecla `i` no monitor serial ou botão BOOT.

## Testes

| Parte | Comando | Pasta |
| :--- | :--- | :--- |
| Firmware | `pio test -e native` | `src/firmware` |
| Backend | `pytest` | `src/backend` |
| Frontend | `npm test` | `src/frontend` |
| Ponta a ponta (simulador) | `npx playwright test` | `src/e2e` |
| Ponta a ponta (robô real) | `npx playwright test -c playwright.robo-real.config.ts` | `src/e2e` |

O GitHub Actions roda os testes, a cobertura e o build de cada parte em todo Pull Request. Os resultados e as imagens de cobertura estão em [7.4 - Testes de software](../docs/7.4%20-%20Testes%20de%20software.md); os testes na placa estão em [7.3 - Testes de hardware](../docs/7.3%20-%20Testes%20de%20hardware.md).

## Compilar e gravar no robô

```bash
cd src/firmware
pio run -e esp32dev                  # compila
pio run -e esp32dev -t upload        # grava pela USB
pio run -e esp32dev_ota -t upload --upload-port <IP do robô>   # grava pelo Wi-Fi (OTA)
pio device monitor                   # monitor serial (115200)
```

## Organização do repositório

Código-fonte fica só em `src/`. Arquivos de eletrônica ficam em [`hw/`](../hw/README.md) e de estrutura em [`mec/`](../mec/README.md).
