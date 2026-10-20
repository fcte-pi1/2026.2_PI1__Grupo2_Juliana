from fastapi import FastAPI

from app.routers.telemetry import router as telemetry_router
from app.routers.logs import router as logs_router
from app.routers.exportacao import router as exportacao_router
from app.routers.corridas import router as corridas_router

app = FastAPI(title="Micromouse - Telemetria")

app.include_router(telemetry_router)
app.include_router(logs_router)
app.include_router(exportacao_router)
app.include_router(corridas_router)


@app.get("/health")
def health():
    return {"status": "ok"}
