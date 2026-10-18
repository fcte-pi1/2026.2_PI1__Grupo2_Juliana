"""Consulta paginada de logs para o painel, sem mudanças no banco (#222)."""

from datetime import datetime
from typing import Literal

from fastapi import APIRouter, Query
from pydantic import BaseModel, ConfigDict
from sqlalchemy import select

from app.db.models import LogEvento
from app.db.session import session_scope

router = APIRouter(tags=["logs"])


class LogResposta(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    id_corrida: int | None
    tipo: str
    origem: str
    mensagem: str
    ocorrido_em: datetime


@router.get("/logs", response_model=list[LogResposta])
def consultar_logs(
    id_corrida: int | None = Query(default=None, ge=1),
    tipo: Literal["info", "aviso", "erro"] | None = None,
    origem: Literal["firmware", "backend", "frontend"] | None = None,
    limite: int = Query(default=100, ge=1, le=200),
    deslocamento: int = Query(default=0, ge=0),
) -> list[LogResposta]:
    consulta = select(LogEvento)
    if id_corrida is not None:
        consulta = consulta.where(LogEvento.id_corrida == id_corrida)
    if tipo is not None:
        consulta = consulta.where(LogEvento.tipo == tipo)
    if origem is not None:
        consulta = consulta.where(LogEvento.origem == origem)
    consulta = consulta.order_by(LogEvento.id.desc()).offset(deslocamento).limit(limite)
    with session_scope() as db:
        return [LogResposta.model_validate(log) for log in db.scalars(consulta)]
