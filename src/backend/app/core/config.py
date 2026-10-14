"""Configurações da aplicação backend."""

import os
from dotenv import load_dotenv

load_dotenv()


class Settings:
    def __init__(self) -> None:
        db_url = os.environ.get("DATABASE_URL")
        if not db_url:
            raise RuntimeError(
                "A variável de ambiente DATABASE_URL é obrigatória. "
                "Defina-a no arquivo .env ou no ambiente antes de iniciar a aplicação."
            )
        self.DATABASE_URL: str = db_url


settings = Settings()
