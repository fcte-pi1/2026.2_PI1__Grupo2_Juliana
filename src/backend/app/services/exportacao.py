"""Exportação das corridas, leituras e logs em CSV (RF18)."""

import csv
import io
from datetime import date, datetime
from decimal import Decimal

from sqlalchemy import func, select
from sqlalchemy.orm import Session

from app.db.models import Corrida, Labirinto, LeituraTelemetria, LogEvento

# Fuso horário idêntico ao de consultas do histórico
FUSO_HORARIO = "America/Sao_Paulo"

COLUNAS = [
    "id_corrida", "labirinto", "num_tentativa", "num_corrida_robo", "status",
    "iniciado_em", "tempo_conc_s", "consumo_medio_ma",
    "tempo_ms", "recebido_em", "pos_x", "pos_y", "orientacao", "paredes",
    "velocidade", "tensao_bateria", "valor_flood",
]

COLUNAS_LOGS = ["id", "id_corrida", "tipo", "origem", "mensagem", "ocorrido_em"]


def _celula(valor) -> str:
    """Converte um valor do banco para texto; nulo vira célula vazia."""
    if valor is None:
        return ""
    if isinstance(valor, datetime):
        return valor.isoformat()
    if isinstance(valor, Decimal):
        return format(valor, "f")
    return str(valor)


def gerar_csv(
    db: Session,
    id_corrida: int | None = None,
    id_labirinto: int | None = None,
    status: str | None = None,
    num_tentativa: int | None = None,
    data: date | None = None,
) -> str:
    """Uma linha por leitura; corrida sem leituras sai em uma linha com as colunas de leitura vazias."""
    consulta = (
        select(Corrida, Labirinto.nome, LeituraTelemetria)
        .join(Labirinto, Corrida.id_labirinto == Labirinto.id)
        .outerjoin(LeituraTelemetria, LeituraTelemetria.id_corrida == Corrida.id)
    )
    if id_corrida is not None:
        consulta = consulta.where(Corrida.id == id_corrida)
    if id_labirinto is not None:
        consulta = consulta.where(Corrida.id_labirinto == id_labirinto)
    if status is not None:
        consulta = consulta.where(Corrida.status == status)
    if num_tentativa is not None:
        consulta = consulta.where(Corrida.num_tentativa == num_tentativa)
    if data is not None:
        consulta = consulta.where(func.date(func.timezone(FUSO_HORARIO, Corrida.iniciado_em)) == data)
    consulta = consulta.order_by(Corrida.id, LeituraTelemetria.tempo_ms)

    saida = io.StringIO()
    escritor = csv.writer(saida, lineterminator="\n")
    escritor.writerow(COLUNAS)
    for corrida, nome_labirinto, leitura in db.execute(consulta):
        dados_corrida = [
            corrida.id, nome_labirinto, corrida.num_tentativa, corrida.num_corrida_robo,
            corrida.status, corrida.iniciado_em, corrida.tempo_conc_s, corrida.consumo_medio_ma,
        ]
        if leitura is None:
            dados_leitura = [None] * 9
        else:
            dados_leitura = [
                leitura.tempo_ms, leitura.recebido_em, leitura.pos_x, leitura.pos_y,
                leitura.orientacao, leitura.paredes, leitura.velocidade,
                leitura.tensao_bateria, leitura.valor_flood,
            ]
        escritor.writerow([_celula(valor) for valor in dados_corrida + dados_leitura])
    return saida.getvalue()


def gerar_csv_logs(
    db: Session,
    id_corrida: int | None = None,
    id_labirinto: int | None = None,
    tipo: str | None = None,
    origem: str | None = None,
    busca: str | None = None,
    data: date | None = None,
) -> str:
    """Uma linha por log, do mais antigo ao mais recente; sem filtro, inclui os logs sem corrida."""
    consulta = select(LogEvento)
    if id_corrida is not None:
        consulta = consulta.where(LogEvento.id_corrida == id_corrida)
    if id_labirinto is not None:
        consulta = consulta.join(Corrida, LogEvento.id_corrida == Corrida.id).where(
            Corrida.id_labirinto == id_labirinto
        )
    if tipo is not None:
        consulta = consulta.where(LogEvento.tipo == tipo)
    if origem is not None:
        consulta = consulta.where(LogEvento.origem == origem)
    if busca:
        consulta = consulta.where(LogEvento.mensagem.ilike(f"%{busca.strip()}%"))
    if data is not None:
        consulta = consulta.where(func.date(func.timezone(FUSO_HORARIO, LogEvento.ocorrido_em)) == data)
    consulta = consulta.order_by(LogEvento.id)

    saida = io.StringIO()
    escritor = csv.writer(saida, lineterminator="\n")
    escritor.writerow(COLUNAS_LOGS)
    for log in db.scalars(consulta):
        dados = [log.id, log.id_corrida, log.tipo, log.origem, log.mensagem, log.ocorrido_em]
        escritor.writerow([_celula(valor) for valor in dados])
    return saida.getvalue()
