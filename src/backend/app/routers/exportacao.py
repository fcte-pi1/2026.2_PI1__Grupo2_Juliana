"""Download das corridas e dos logs em CSV para o painel."""

from fastapi import APIRouter, Query
from fastapi.responses import Response

from app.db.session import session_scope
from app.services.exportacao import gerar_csv, gerar_csv_logs

router = APIRouter(tags=["exportacao"])


@router.get("/exportacao/corridas.csv", response_class=Response)
def exportar_corridas(
    id_corrida: int | None = Query(default=None, ge=1),
    id_labirinto: int | None = Query(default=None, ge=1),
) -> Response:
    with session_scope() as db:
        conteudo = gerar_csv(db, id_corrida=id_corrida, id_labirinto=id_labirinto)
    return Response(
        content=conteudo,
        media_type="text/csv; charset=utf-8",
        headers={"Content-Disposition": 'attachment; filename="corridas.csv"'},
    )


@router.get("/exportacao/logs.csv", response_class=Response)
def exportar_logs(
    id_corrida: int | None = Query(default=None, ge=1),
    id_labirinto: int | None = Query(default=None, ge=1),
) -> Response:
    with session_scope() as db:
        conteudo = gerar_csv_logs(db, id_corrida=id_corrida, id_labirinto=id_labirinto)
    return Response(
        content=conteudo,
        media_type="text/csv; charset=utf-8",
        headers={"Content-Disposition": 'attachment; filename="logs.csv"'},
    )
