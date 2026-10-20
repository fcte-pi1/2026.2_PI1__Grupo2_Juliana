"""Corridas, tentativas por labirinto e consultas do histórico (RF07-S, RF08-S, RF09-S, RF10-S)."""

from datetime import date
from typing import Any

from sqlalchemy import Select, func, select
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session

from app.db.models import Corrida, Labirinto, LeituraTelemetria, LogEvento

# Fuso usado para filtrar pela "Data da execução" do histórico
FUSO_HORARIO = "America/Sao_Paulo"


class LabirintoNaoEncontrado(Exception):
    pass


class CorridaNaoEncontrada(Exception):
    pass


class CorridaEmAndamento(Exception):
    """Já existe uma corrida em andamento (só uma por vez, ver 4.4.7 seção 5)."""


class CorridaJaEncerrada(Exception):
    pass


def _consulta_resumo() -> Select:
    """Corrida com o labirinto e os totais das leituras, para não percorrer as leituras no Python."""
    leituras = (
        select(
            LeituraTelemetria.id_corrida,
            func.count().label("total_leituras"),
            func.avg(LeituraTelemetria.velocidade).label("velocidade_media"),
            func.max(LeituraTelemetria.tempo_ms).label("ultimo_tempo_ms"),
        )
        .group_by(LeituraTelemetria.id_corrida)
        .subquery()
    )
    return (
        select(
            Corrida,
            Labirinto,
            func.coalesce(leituras.c.total_leituras, 0),
            leituras.c.velocidade_media,
            leituras.c.ultimo_tempo_ms,
        )
        .join(Labirinto, Corrida.id_labirinto == Labirinto.id)
        .outerjoin(leituras, leituras.c.id_corrida == Corrida.id)
    )


def _resumo(corrida: Corrida, labirinto: Labirinto, total_leituras: int, velocidade_media, ultimo_tempo_ms) -> dict[str, Any]:
    # Tentativa não concluída não tem tempo_conc_s: a duração é a da última leitura (CT-S08).
    if corrida.tempo_conc_s is not None:
        duracao_s = float(corrida.tempo_conc_s)
    elif ultimo_tempo_ms is not None:
        duracao_s = ultimo_tempo_ms / 1000
    else:
        duracao_s = None
    return {
        "id": corrida.id,
        "id_labirinto": labirinto.id,
        "labirinto": labirinto.nome,
        "largura_celula": labirinto.largura_celula,
        "altura_celula": labirinto.altura_celula,
        "num_tentativa": corrida.num_tentativa,
        "num_corrida_robo": corrida.num_corrida_robo,
        "status": corrida.status,
        "iniciado_em": corrida.iniciado_em,
        "tempo_conc_s": corrida.tempo_conc_s,
        "duracao_s": duracao_s,
        "consumo_medio_ma": corrida.consumo_medio_ma,
        "velocidade_media": round(float(velocidade_media), 3) if velocidade_media is not None else None,
        "total_leituras": total_leituras,
    }


def listar_labirintos(db: Session) -> list[dict[str, Any]]:
    """Labirintos com o total de tentativas de cada um, inclusive os sem tentativas (RF10-S)."""
    consulta = (
        select(Labirinto, func.count(Corrida.id))
        .outerjoin(Corrida, Corrida.id_labirinto == Labirinto.id)
        .group_by(Labirinto.id)
        .order_by(Labirinto.id)
    )
    return [
        {
            "id": labirinto.id,
            "nome": labirinto.nome,
            "largura_celula": labirinto.largura_celula,
            "altura_celula": labirinto.altura_celula,
            "total_tentativas": total,
        }
        for labirinto, total in db.execute(consulta)
    ]


def listar_corridas(
    db: Session,
    id_labirinto: int | None = None,
    status: str | None = None,
    num_tentativa: int | None = None,
    data: date | None = None,
) -> list[dict[str, Any]]:
    """Histórico, da corrida mais recente à mais antiga; sem filtros é a consulta geral (RF08-S, RF09-S)."""
    consulta = _consulta_resumo()
    if id_labirinto is not None:
        consulta = consulta.where(Corrida.id_labirinto == id_labirinto)
    if status is not None:
        consulta = consulta.where(Corrida.status == status)
    if num_tentativa is not None:
        consulta = consulta.where(Corrida.num_tentativa == num_tentativa)
    if data is not None:
        consulta = consulta.where(func.date(func.timezone(FUSO_HORARIO, Corrida.iniciado_em)) == data)
    consulta = consulta.order_by(Corrida.iniciado_em.desc(), Corrida.id.desc())
    return [_resumo(*linha) for linha in db.execute(consulta)]


def obter_corrida(db: Session, id_corrida: int) -> dict[str, Any]:
    """Resumo da corrida e as leituras na ordem da corrida, para o trajeto em Detalhes da Corrida."""
    linha = db.execute(_consulta_resumo().where(Corrida.id == id_corrida)).first()
    if linha is None:
        raise CorridaNaoEncontrada(id_corrida)
    leituras = db.scalars(
        select(LeituraTelemetria)
        .where(LeituraTelemetria.id_corrida == id_corrida)
        .order_by(LeituraTelemetria.tempo_ms)
    )
    return {**_resumo(*linha), "leituras": list(leituras)}


def criar_corrida(db: Session, id_labirinto: int) -> dict[str, Any]:
    """Abre a próxima tentativa do labirinto como em_andamento, à espera do evento inicio do robô (RF10-S)."""
    if db.get(Labirinto, id_labirinto) is None:
        raise LabirintoNaoEncontrado(id_labirinto)
    if db.scalar(select(Corrida.id).where(Corrida.status == "em_andamento")) is not None:
        raise CorridaEmAndamento()
    ultima = db.scalar(select(func.max(Corrida.num_tentativa)).where(Corrida.id_labirinto == id_labirinto))
    corrida = Corrida(id_labirinto=id_labirinto, num_tentativa=(ultima or 0) + 1)
    db.add(corrida)
    try:
        db.commit()
    except IntegrityError as erro:
        # Outra corrida foi criada ao mesmo tempo: os índices únicos barram a segunda.
        db.rollback()
        raise CorridaEmAndamento() from erro
    return obter_corrida(db, corrida.id)


def encerrar_corrida(db: Session, id_corrida: int) -> dict[str, Any]:
    """Comando de abortar: grava a tentativa como abandonada com o que já foi recebido (RF07-S, CT-S08)."""
    corrida = db.get(Corrida, id_corrida)
    if corrida is None:
        raise CorridaNaoEncontrada(id_corrida)
    if corrida.status != "em_andamento":
        raise CorridaJaEncerrada(id_corrida)
    corrida.status = "abandonada"
    db.add(LogEvento(
        id_corrida=id_corrida,
        tipo="aviso",
        origem="backend",
        mensagem="corrida encerrada manualmente pelo painel",
    ))
    db.commit()
    return obter_corrida(db, id_corrida)
