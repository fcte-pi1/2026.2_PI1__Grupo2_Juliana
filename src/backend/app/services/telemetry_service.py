"""Serviço de orquestração, persistência e validação da telemetria (RF24-S, RF25-S, RNF06-S)."""

import logging
from dataclasses import dataclass
from typing import Any

from sqlalchemy import select
from sqlalchemy.dialects.postgresql import insert
from sqlalchemy.exc import SQLAlchemyError
from sqlalchemy.orm import Session

from app.db.models import Corrida, Labirinto, LeituraTelemetria, LogEvento
from app.services.protocol import format_frame, validate_frame, validate_limites_labirinto

logger = logging.getLogger("telemetry_service")


@dataclass
class ContextoConexao:
    """Associação confirmada para UMA conexão, nunca compartilhada entre sockets."""

    id_corrida: int | None = None


class TelemetryService:
    """Gerencia o ciclo de vida e regras de negócio da recepção de telemetria."""

    def __init__(self) -> None:
        # Cache em memória de pares (id_corrida, seq) de eventos já processados para deduplicação
        self.processed_events: set[tuple[int, int]] = set()

    async def process_frame(
        self, raw_text: str, db: Session, contexto: ContextoConexao | None = None
    ) -> tuple[str | None, dict[str, Any] | None]:
        """Processa um quadro de telemetria, grava no banco e retorna (ack_frame, broadcast_data)."""
        corrida_id_contexto: int | None = None
        try:
            # 1 a 6: Validação estrutural do protocolo
            result = validate_frame(raw_text)
            if not result.valid:
                # Não confiar no campo corrida de um quadro corrompido. Usar
                # apenas a associação previamente confirmada nesta conexão.
                self._registrar_erro(
                    db, mensagem=result.error or "quadro invalido",
                    id_corrida=contexto.id_corrida if contexto else None,
                )
                return None, None

            data: dict[str, Any] = result.data or {}
            num_corrida_robo = data["corrida"]
            seq = data["seq"]
            tipo_msg = data["t"]
            evento = data.get("evento")

            # 7: Associação da corrida (Seção 5 do Contrato 4.4.7)
            corrida, ja_encerrada = self._obter_corrida(db, num_corrida_robo, evento)
            if not corrida:
                self._registrar_erro(db, mensagem=f"corrida desconhecida: {num_corrida_robo}", id_corrida=None)
                return None, None

            corrida_id_contexto = corrida.id

            # Pacote válido: gera o ACK
            ack_frame = format_frame({"t": "ack", "corrida": num_corrida_robo, "seq": seq})

            if tipo_msg == "tel":
                # Validação de limites contra o labirinto cadastrado
                labirinto = db.get(Labirinto, corrida.id_labirinto)
                if labirinto:
                    erro_limite = validate_limites_labirinto(
                        data["pos_x"], data["pos_y"], labirinto.largura_celula, labirinto.altura_celula
                    )
                    if erro_limite:
                        self._registrar_erro(db, mensagem=erro_limite, id_corrida=corrida.id)
                        return None, None

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
                    .returning(LeituraTelemetria.id)
                )
                result_insert = db.execute(stmt)
                inserted_id = result_insert.scalar_one_or_none()
                db.commit()

                # Transmite ao painel apenas se a leitura foi inserida pela primeira vez (não reenviada)
                if contexto is not None:
                    contexto.id_corrida = corrida.id
                broadcast_data = {**data, "id_corrida": corrida.id} if inserted_id is not None else None
                return ack_frame, broadcast_data

            elif tipo_msg == "evt":
                # Reenvio de fim para corrida já finalizada
                if ja_encerrada:
                    if contexto is not None:
                        contexto.id_corrida = corrida.id
                    return ack_frame, None

                # Deduplicação de eventos pelo par (corrida.id, seq)
                chave_evento = (corrida.id, seq)
                if chave_evento not in self.processed_events:
                    self.processed_events.add(chave_evento)
                    tipo_log = "info" if evento == "inicio" or (evento == "fim" and data.get("resultado") == "concluida") else "aviso"
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

                    db.commit()
                    if contexto is not None:
                        contexto.id_corrida = corrida.id
                    return ack_frame, {**data, "id_corrida": corrida.id}
                else:
                    # Evento repetido (reenvio): retorna ACK e não grava nem transmite
                    if contexto is not None:
                        contexto.id_corrida = corrida.id
                    return ack_frame, None

            return ack_frame, None

        except SQLAlchemyError as e:
            db.rollback()
            logger.error("Erro de banco ao processar telemetria: %s", e)
            self._registrar_erro(db, mensagem=f"erro interno banco: {type(e).__name__}", id_corrida=corrida_id_contexto)
            return None, None
        except Exception as e:
            db.rollback()
            logger.error("Erro inesperado ao processar telemetria: %s", e)
            return None, None

    def reset(self) -> None:
        """Limpa estados em memória (útil entre testes)."""
        self.processed_events.clear()

    def _obter_corrida(self, db: Session, num_corrida_robo: int, evento: str | None) -> tuple[Corrida | None, bool]:
        """Busca a corrida e retorna (corrida, ja_encerrada) conforme a seção 5 do Contrato."""
        if evento == "inicio":
            corrida = db.scalar(
                select(Corrida)
                .where(Corrida.status == "em_andamento")
                .where(
                    (Corrida.num_corrida_robo.is_(None))
                    | (Corrida.num_corrida_robo == num_corrida_robo)
                )
            )
            if corrida and corrida.num_corrida_robo is None:
                # Limpa cache de eventos de corridas anteriores apenas na primeira associação
                self.processed_events.clear()
                corrida.num_corrida_robo = num_corrida_robo
                db.commit()
            return corrida, False

        # Demais mensagens: busca corrida em_andamento com num_corrida_robo associado
        corrida_ativa = db.scalar(
            select(Corrida)
            .where(Corrida.status == "em_andamento")
            .where(Corrida.num_corrida_robo == num_corrida_robo)
        )
        if corrida_ativa:
            return corrida_ativa, False

        # Se não há corrida em_andamento e o evento é fim, trata reenvio de fim de corrida já encerrada
        if evento == "fim":
            corrida_encerrada = db.scalar(
                select(Corrida)
                .where(Corrida.num_corrida_robo == num_corrida_robo)
                .where(Corrida.status.in_(("concluida", "abandonada")))
                .order_by(Corrida.id.desc())
            )
            if corrida_encerrada:
                return corrida_encerrada, True

        return None, False

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
