"""Tabelas do banco, conforme o Dicionário de Dados (seção 7) da 4.4.4 - Persistência de Dados."""

from datetime import datetime
from decimal import Decimal

from sqlalchemy import (
    CHAR,
    BigInteger,
    CheckConstraint,
    DateTime,
    ForeignKey,
    Integer,
    Numeric,
    SmallInteger,
    String,
    Text,
    UniqueConstraint,
    func,
)
from sqlalchemy.orm import Mapped, mapped_column

from app.db.base import Base


class Usuario(Base):
    """Operador que acessa o sistema web e inicia as corridas (RF21-S)."""

    __tablename__ = "usuario"

    matricula: Mapped[str] = mapped_column(String(20), primary_key=True)
    nome: Mapped[str] = mapped_column(String(100))
    senha_hash: Mapped[str] = mapped_column(String(255))
    criado_em: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())


class Labirinto(Base):
    """Configuração de labirinto em que o robô corre (RF08-S, RF09-S)."""

    __tablename__ = "labirinto"
    __table_args__ = (
        CheckConstraint("largura_celula > 0", name="largura_positiva"),
        CheckConstraint("altura_celula > 0", name="altura_positiva"),
    )

    id: Mapped[int] = mapped_column(primary_key=True)
    nome: Mapped[str] = mapped_column(String(100), unique=True)
    largura_celula: Mapped[int] = mapped_column(SmallInteger)
    altura_celula: Mapped[int] = mapped_column(SmallInteger)
    criado_em: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())


class Corrida(Base):
    """Uma tentativa do robô em um labirinto, com os dados finais (RF07-S, RF10-S).

    O UNIQUE (id_labirinto, num_tentativa) também serve de índice para a consulta por labirinto.
    """

    __tablename__ = "corrida"
    __table_args__ = (
        UniqueConstraint("id_labirinto", "num_tentativa"),
        CheckConstraint("num_tentativa > 0", name="num_tentativa_positivo"),
        CheckConstraint("status IN ('em_andamento', 'concluida', 'abandonada')", name="status_valido"),
        CheckConstraint("status <> 'concluida' OR tempo_conc_s IS NOT NULL", name="concluida_tem_tempo"),
        CheckConstraint("tempo_conc_s >= 0", name="tempo_conc_nao_negativo"),
        CheckConstraint("consumo_medio_ma >= 0", name="consumo_nao_negativo"),
    )

    id: Mapped[int] = mapped_column(primary_key=True)
    id_labirinto: Mapped[int] = mapped_column(ForeignKey("labirinto.id", ondelete="RESTRICT"))
    id_usuario: Mapped[str] = mapped_column(String(20), ForeignKey("usuario.matricula", ondelete="RESTRICT"))
    num_tentativa: Mapped[int] = mapped_column(SmallInteger)
    status: Mapped[str] = mapped_column(String(20), server_default="em_andamento")
    iniciado_em: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
    tempo_conc_s: Mapped[Decimal | None] = mapped_column(Numeric(8, 3))
    consumo_medio_ma: Mapped[Decimal | None] = mapped_column(Numeric(8, 2))


class LeituraTelemetria(Base):
    """Um pacote de telemetria válido enviado pelo robô (RF07-S, RF24-S, RF25-S, RNF06-S).

    O limite superior de pos_x e pos_y depende do labirinto e é validado pelo backend (RF24-S).
    """

    __tablename__ = "leitura_telemetria"
    __table_args__ = (
        UniqueConstraint("id_corrida", "tempo_ms"),
        CheckConstraint("tempo_ms >= 0", name="tempo_ms_nao_negativo"),
        CheckConstraint("pos_x >= 0 AND pos_y >= 0", name="posicao_nao_negativa"),
        CheckConstraint("orientacao IN ('N', 'S', 'L', 'O')", name="orientacao_valida"),
        CheckConstraint("paredes BETWEEN 0 AND 15", name="paredes_valida"),
        CheckConstraint("velocidade >= 0", name="velocidade_nao_negativa"),
        CheckConstraint("tensao_bateria >= 0", name="tensao_nao_negativa"),
        CheckConstraint("valor_flood >= 0", name="valor_flood_nao_negativo"),
    )

    id: Mapped[int] = mapped_column(BigInteger, primary_key=True)
    id_corrida: Mapped[int] = mapped_column(ForeignKey("corrida.id", ondelete="CASCADE"))
    tempo_ms: Mapped[int] = mapped_column(Integer)
    recebido_em: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
    pos_x: Mapped[int] = mapped_column(SmallInteger)
    pos_y: Mapped[int] = mapped_column(SmallInteger)
    orientacao: Mapped[str] = mapped_column(CHAR(1))
    # Máscara de bits das paredes da célula: Norte = 1, Leste = 2, Sul = 4, Oeste = 8
    paredes: Mapped[int] = mapped_column(SmallInteger)
    velocidade: Mapped[Decimal] = mapped_column(Numeric(6, 3))
    tensao_bateria: Mapped[Decimal] = mapped_column(Numeric(5, 2))
    valor_flood: Mapped[int | None] = mapped_column(SmallInteger)


class LogEvento(Base):
    """Evento, aviso ou erro de firmware, backend ou frontend (RF20-S, RF24-S)."""

    __tablename__ = "log_evento"
    __table_args__ = (
        CheckConstraint("tipo IN ('info', 'aviso', 'erro')", name="tipo_valido"),
        CheckConstraint("origem IN ('firmware', 'backend', 'frontend')", name="origem_valida"),
    )

    id: Mapped[int] = mapped_column(BigInteger, primary_key=True)
    id_corrida: Mapped[int | None] = mapped_column(ForeignKey("corrida.id", ondelete="SET NULL"), index=True)
    tipo: Mapped[str] = mapped_column(String(10))
    origem: Mapped[str] = mapped_column(String(20))
    mensagem: Mapped[str] = mapped_column(Text)
    ocorrido_em: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
