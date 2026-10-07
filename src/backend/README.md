# Backend

Servidor de telemetria do Micromouse em Python com [FastAPI](https://fastapi.tiangolo.com/).

## Requisitos
- Python 3.13

## Instalação
```bash
cd src/backend
python -m venv .venv
# Windows: .venv\Scripts\activate | Linux/macOS: source .venv/bin/activate
pip install -r requirements-dev.txt
```

## Rodar
```bash
uvicorn app.main:app --reload
```
A API sobe em http://localhost:8000 (documentação em http://localhost:8000/docs).

## Testar
```bash
pytest
```
Os testes ficam em `tests/` e a cobertura do pacote `app` é mostrada ao final. O mesmo comando roda no GitHub Actions em todo Pull Request.
