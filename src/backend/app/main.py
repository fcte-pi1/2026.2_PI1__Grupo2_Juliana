from fastapi import FastAPI

from app.routers.telemetry import router as telemetry_router

app = FastAPI(title="Micromouse - Telemetria")

app.include_router(telemetry_router)


@app.get("/health")
def health():
    return {"status": "ok"}
