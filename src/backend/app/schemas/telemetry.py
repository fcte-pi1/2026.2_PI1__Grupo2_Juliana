"""Esquemas de dados Pydantic para validação do Contrato de Telemetria (4.4.7)."""

from decimal import Decimal
from typing import Annotated, Literal

from pydantic import BaseModel, ConfigDict, Field


class BaseMessage(BaseModel):
    """Cabeçalho comum a mensagens tel e evt."""
    model_config = ConfigDict(extra="ignore")

    v: int = Field(..., description="Versão do contrato (atualmente 1)")
    t: Literal["tel", "evt"]
    corrida: Annotated[int, Field(ge=1, description="Número da corrida no robô")]
    seq: Annotated[int, Field(ge=0, description="Número de sequência no transporte")]
    tempo_ms: Annotated[int, Field(ge=0, description="Milissegundos desde a largada")]


class TelemetryMessage(BaseMessage):
    """Mensagem de telemetria periódica (t = 'tel')."""
    t: Literal["tel"] = "tel"
    pos_x: Annotated[int, Field(ge=0, description="Coordenada X da célula")]
    pos_y: Annotated[int, Field(ge=0, description="Coordenada Y da célula")]
    orientacao: Literal["N", "S", "L", "O"]
    paredes: Annotated[int, Field(ge=0, le=15, description="Máscara de bits de paredes")]
    velocidade: Annotated[Decimal, Field(ge=0, description="Velocidade em m/s")]
    tensao_bateria: Annotated[Decimal, Field(ge=0, description="Tensão da bateria em V")]
    valor_flood: int | None = Field(default=None, ge=0, description="Métrica de flooding")
    estado: Literal["explorando", "retornando", "rapido", "parado"]


class EventMessage(BaseMessage):
    """Mensagem de evento especial (t = 'evt')."""
    t: Literal["evt"] = "evt"
    evento: Literal["inicio", "fim", "bateria_critica", "reconexao"]
    tensao_bateria: Decimal | None = None
    resultado: Literal["concluida", "abandonada"] | None = None
    tempo_conc_s: Decimal | None = None
    consumo_medio_ma: Decimal | None = None
    motivo: str | None = None
    offline_ms: int | None = None
    pendentes: int | None = None


class AckMessage(BaseModel):
    """Confirmação de recebimento (Ack) enviada ao robô."""
    t: Literal["ack"] = "ack"
    corrida: int
    seq: int
