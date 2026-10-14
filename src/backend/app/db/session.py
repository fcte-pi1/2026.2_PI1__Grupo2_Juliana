"""Gerenciamento de sessões e conexão com o banco de dados via SQLAlchemy."""

from collections.abc import Generator
from contextlib import contextmanager

from sqlalchemy import create_engine
from sqlalchemy.orm import Session, sessionmaker

from app.core.config import settings

engine = create_engine(settings.DATABASE_URL, pool_pre_ping=True)
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)



@contextmanager
def session_scope() -> Generator[Session, None, None]:
    """Fornece uma sessão transacional do banco de dados com fechamento automático."""
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()
