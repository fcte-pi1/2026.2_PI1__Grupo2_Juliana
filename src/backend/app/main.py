from fastapi import FastAPI

app = FastAPI(title="Micromouse - Telemetria")


@app.get("/health")
def health():
    return {"status": "ok"}
