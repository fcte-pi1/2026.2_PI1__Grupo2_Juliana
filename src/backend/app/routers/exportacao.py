"""Download das corridas e dos logs em CSV para o painel."""

from datetime import date
from typing import Literal

from fastapi import APIRouter, Query
from fastapi.responses import Response

from app.db.session import session_scope
from app.services.exportacao import gerar_csv, gerar_csv_logs

router = APIRouter(tags=["exportacao"])


@router.get("/exportacao/corridas.csv", response_class=Response)
def exportar_corridas(
    id_corrida: int | None = Query(default=None, ge=1),
    id_labirinto: int | None = Query(default=None, ge=1),
    status: Literal["em_andamento", "concluida", "abandonada"] | None = None,
    num_tentativa: int | None = Query(default=None, ge=1),
    data: date | None = None,
) -> Response:
    with session_scope() as db:
        conteudo = gerar_csv(
            db,
            id_corrida=id_corrida,
            id_labirinto=id_labirinto,
            status=status,
            num_tentativa=num_tentativa,
            data=data,
        )
    return Response(
        content=conteudo,
        media_type="text/csv; charset=utf-8",
        headers={"Content-Disposition": 'attachment; filename="corridas.csv"'},
    )


@router.get("/exportacao/logs.csv", response_class=Response)
def exportar_logs(
    id_corrida: int | None = Query(default=None, ge=1),
    id_labirinto: int | None = Query(default=None, ge=1),
    tipo: Literal["info", "aviso", "erro"] | None = None,
    origem: Literal["firmware", "backend", "frontend"] | None = None,
    busca: str | None = None,
    data: date | None = None,
) -> Response:
    with session_scope() as db:
        conteudo = gerar_csv_logs(
            db,
            id_corrida=id_corrida,
            id_labirinto=id_labirinto,
            tipo=tipo,
            origem=origem,
            busca=busca,
            data=data,
        )
    return Response(
        content=conteudo,
        media_type="text/csv; charset=utf-8",
        headers={"Content-Disposition": 'attachment; filename="logs.csv"'},
    )
