"""Rotas de corridas e labirintos para o painel (RF07-S, RF08-S, RF09-S, RF10-S)."""

from datetime import date, datetime
from typing import Literal

from fastapi import APIRouter, HTTPException, Query
from pydantic import BaseModel, ConfigDict, Field

from app.db.session import session_scope
from app.services import corridas

router = APIRouter(tags=["corridas"])


class LabirintoResposta(BaseModel):
    id: int
    nome: str
    largura_celula: int
    altura_celula: int
    total_tentativas: int


class NovaCorrida(BaseModel):
    id_labirinto: int = Field(ge=1)


class CorridaResposta(BaseModel):
    id: int
    id_labirinto: int
    labirinto: str
    largura_celula: int
    altura_celula: int
    num_tentativa: int
    num_corrida_robo: int | None
    status: str
    iniciado_em: datetime
    tempo_conc_s: float | None
    duracao_s: float | None
    consumo_medio_ma: float | None
    velocidade_media: float | None
    total_leituras: int


class LeituraResposta(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    tempo_ms: int
    pos_x: int
    pos_y: int
    orientacao: str
    paredes: int
    velocidade: float
    tensao_bateria: float
    valor_flood: int | None


class CorridaDetalhe(CorridaResposta):
    leituras: list[LeituraResposta]


def _detalhe(dados: dict) -> dict:
    """Converte as leituras ainda com a sessão aberta."""
    return {**dados, "leituras": [LeituraResposta.model_validate(leitura) for leitura in dados["leituras"]]}


@router.get("/labirintos", response_model=list[LabirintoResposta])
def listar_labirintos() -> list[dict]:
    with session_scope() as db:
        return corridas.listar_labirintos(db)


@router.get("/corridas", response_model=list[CorridaResposta])
def listar_corridas(
    id_labirinto: int | None = Query(default=None, ge=1),
    status: Literal["em_andamento", "concluida", "abandonada"] | None = None,
    num_tentativa: int | None = Query(default=None, ge=1),
    data: date | None = None,
) -> list[dict]:
    with session_scope() as db:
        return corridas.listar_corridas(db, id_labirinto, status, num_tentativa, data)


@router.get("/corridas/{id_corrida}", response_model=CorridaDetalhe)
def obter_corrida(id_corrida: int) -> dict:
    with session_scope() as db:
        try:
            return _detalhe(corridas.obter_corrida(db, id_corrida))
        except corridas.CorridaNaoEncontrada:
            raise HTTPException(404, "corrida não encontrada")


@router.post("/corridas", response_model=CorridaDetalhe, status_code=201)
def criar_corrida(nova: NovaCorrida) -> dict:
    with session_scope() as db:
        try:
            return _detalhe(corridas.criar_corrida(db, nova.id_labirinto))
        except corridas.LabirintoNaoEncontrado:
            raise HTTPException(404, "labirinto não encontrado")
        except corridas.CorridaEmAndamento:
            raise HTTPException(409, "já existe uma corrida em andamento")


@router.post("/corridas/{id_corrida}/encerrar", response_model=CorridaDetalhe)
def encerrar_corrida(id_corrida: int) -> dict:
    with session_scope() as db:
        try:
            return _detalhe(corridas.encerrar_corrida(db, id_corrida))
        except corridas.CorridaNaoEncontrada:
            raise HTTPException(404, "corrida não encontrada")
        except corridas.CorridaJaEncerrada:
            raise HTTPException(409, "a corrida já foi encerrada")

