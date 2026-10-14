"""Configurações da aplicação backend."""

import os
from dotenv import load_dotenv

load_dotenv()


class Settings:
    DATABASE_URL: str = os.getenv(
        "DATABASE_URL",
        "postgresql+psycopg://micromouse:troque_esta_senha@localhost:5432/micromouse",
    )
    TIMEOUT_OFFLINE_SEGUNDOS: float = 3.0


settings = Settings()
