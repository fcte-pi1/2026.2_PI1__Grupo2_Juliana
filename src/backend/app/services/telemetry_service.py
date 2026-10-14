"""Serviço de orquestração, persistência e validação da telemetria (RF24-S, RF25-S, RNF06-S)."""

import logging
from typing import Any

from sqlalchemy import select
from sqlalchemy.dialects.postgresql import insert
from sqlalchemy.orm import Session

from app.core.websocket_manager import manager
from app.db.models import Corrida, Labirinto, LeituraTelemetria, LogEvento
from app.services.protocol import format_frame, validate_frame

logger = logging.getLogger("telemetry_service")


class TelemetryService:
    """Gerencia o ciclo de vida e regras de negócio da recepção de telemetria."""

    def __init__(self) -> None:
        # Cache em memória de pares (corrida.id, seq) de eventos já processados para deduplicação
        self.processed_events: set[tuple[int, int]] = set()
        # Fallback de corrida ativa se num_corrida_robo ainda não estiver no banco
        self.fallback_active_corrida_robo: int | None = None

    async def process_frame(self, raw_text: str, db: Session) -> str | None:
        """Processa um quadro de telemetria, grava no banco e retorna o quadro ACK se válido."""
        # 1 a 6: Validação estrutural do protocolo
        result = validate_frame(raw_text)
        if not result.valid:
            self._registrar_erro(db, mensagem=result.error or "quadro invalido", id_corrida=None)
            return None

        data: dict[str, Any] = result.data or {}
        num_corrida_robo = data["corrida"]
        seq = data["seq"]
        tipo_msg = data["t"]
        evento = data.get("evento")

        # 7: Associação da corrida (Seção 5 do Contrato 4.4.7)
        corrida = self._obter_corrida_ativa(db, num_corrida_robo, evento)
        if not corrida:
            self._registrar_erro(db, mensagem=f"corrida desconhecida: {num_corrida_robo}", id_corrida=None)
            return None

        # Validação de limites contra o labirinto cadastrado
        if tipo_msg == "tel":
            labirinto = db.scalar(select(Labirinto).where(Labirinto.id == corrida.id_labirinto))
            if labirinto:
                if data["pos_x"] >= labirinto.largura_celula:
                    self._registrar_erro(db, mensagem=f"fora do intervalo: pos_x={data['pos_x']}", id_corrida=corrida.id)
                    return None
                if data["pos_y"] >= labirinto.altura_celula:
                    self._registrar_erro(db, mensagem=f"fora do intervalo: pos_y={data['pos_y']}", id_corrida=corrida.id)
                    return None

        # Pacote válido: gera o ACK
        ack_frame = format_frame({"t": "ack", "corrida": num_corrida_robo, "seq": seq})

        if tipo_msg == "tel":
            # Persistência idempotente: UNIQUE (id_corrida, tempo_ms) ignora reenvios
            stmt = (
                insert(LeituraTelemetria)
                .values(
                    id_corrida=corrida.id,
                    tempo_ms=data["tempo_ms"],
                    pos_x=data["pos_x"],
                    pos_y=data["pos_y"],
                    orientacao=data["orientacao"],
                    paredes=data["paredes"],
                    velocidade=data["velocidade"],
                    tensao_bateria=data["tensao_bateria"],
                    valor_flood=data.get("valor_flood"),
                )
                .on_conflict_do_nothing(index_elements=["id_corrida", "tempo_ms"])
            )
            db.execute(stmt)
            db.commit()

            # Broadcast ao vivo para os clientes do painel (incluindo o estado)
            await manager.broadcast({**data, "id_corrida": corrida.id})

        elif tipo_msg == "evt":
            # Deduplicação de eventos pelo par (corrida.id, seq) na corrida em andamento
            chave_evento = (corrida.id, seq)
            if chave_evento not in self.processed_events:
                self.processed_events.add(chave_evento)
                tipo_log = "info" if evento in ("inicio",) or (evento == "fim" and data.get("resultado") == "concluida") else "aviso"
                msg_log = self._formatar_mensagem_evento(data)

                log_entry = LogEvento(
                    id_corrida=corrida.id,
                    tipo=tipo_log,
                    origem="backend",
                    mensagem=msg_log,
                )
                db.add(log_entry)

                if evento == "fim":
                    corrida.status = data.get("resultado", "concluida")
                    if data.get("tempo_conc_s") is not None:
                        corrida.tempo_conc_s = data["tempo_conc_s"]
                    if data.get("consumo_medio_ma") is not None:
                        corrida.consumo_medio_ma = data["consumo_medio_ma"]
                    self.processed_events.discard(chave_evento)
                    self.fallback_active_corrida_robo = None

                db.commit()
                # Broadcast do evento ao painel
                await manager.broadcast({**data, "id_corrida": corrida.id})

        return ack_frame

    def reset(self) -> None:
        """Limpa estados em memória (útil entre testes)."""
        self.processed_events.clear()
        self.fallback_active_corrida_robo = None

    def _obter_corrida_ativa(self, db: Session, num_corrida_robo: int, evento: str | None) -> Corrida | None:
        """Busca e associa a corrida em andamento conforme a seção 5 do Contrato."""
        tem_num_corrida_coluna = hasattr(Corrida, "num_corrida_robo")

        if tem_num_corrida_coluna:
            if evento == "inicio":
                corrida = db.scalar(
                    select(Corrida)
                    .where(Corrida.status == "em_andamento")
                    .where(
                        (getattr(Corrida, "num_corrida_robo").is_(None))
                        | (getattr(Corrida, "num_corrida_robo") == num_corrida_robo)
                    )
                )
                if corrida and getattr(corrida, "num_corrida_robo") is None:
                    setattr(corrida, "num_corrida_robo", num_corrida_robo)
                    db.commit()
                return corrida
            else:
                return db.scalar(
                    select(Corrida)
                    .where(Corrida.status == "em_andamento")
                    .where(getattr(Corrida, "num_corrida_robo") == num_corrida_robo)
                )
        else:
            # Fallback caso a migração do PR #236 ainda não tenha sido aplicada
            corrida = db.scalar(select(Corrida).where(Corrida.status == "em_andamento"))
            if not corrida:
                return None
            if evento == "inicio":
                self.fallback_active_corrida_robo = num_corrida_robo
                return corrida
            elif self.fallback_active_corrida_robo == num_corrida_robo:
                return corrida
            return None

    def _registrar_erro(self, db: Session, mensagem: str, id_corrida: int | None) -> None:
        """Grava descarte de pacote em log_evento (RF24-S)."""
        logger.warning("Telemetria descartada: %s (id_corrida=%s)", mensagem, id_corrida)
        try:
            log = LogEvento(
                id_corrida=id_corrida,
                tipo="erro",
                origem="backend",
                mensagem=mensagem,
            )
            db.add(log)
            db.commit()
        except Exception as e:
            db.rollback()
            logger.error("Erro ao gravar log de descarte: %s", e)

    def _formatar_mensagem_evento(self, data: dict[str, Any]) -> str:
        """Formata a mensagem para o log de eventos."""
        evento = data.get("evento")
        if evento == "inicio":
            return f"evento inicio: tensao={data.get('tensao_bateria')}V"
        elif evento == "fim":
            res = data.get("resultado")
            motivo = f", motivo={data.get('motivo')}" if data.get("motivo") else ""
            tempo = f", tempo={data.get('tempo_conc_s')}s" if data.get("tempo_conc_s") is not None else ""
            return f"evento fim: resultado={res}{motivo}{tempo}"
        elif evento == "bateria_critica":
            return f"evento bateria_critica: tensao={data.get('tensao_bateria')}V"
        elif evento == "reconexao":
            return f"evento reconexao: offline_ms={data.get('offline_ms')}, pendentes={data.get('pendentes')}"
        return f"evento {evento}"


telemetry_service = TelemetryService()
