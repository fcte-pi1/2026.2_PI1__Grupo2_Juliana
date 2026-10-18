"""Rotas WebSocket para recepção de telemetria e transmissão para o painel."""

import logging

from fastapi import APIRouter, WebSocket, WebSocketDisconnect

from app.core.websocket_manager import manager
from app.db.session import session_scope
from app.services.telemetry_service import ContextoConexao, telemetry_service
from app.services.logs import registrar_desconexao

logger = logging.getLogger("telemetry_router")

router = APIRouter(tags=["telemetria"])


@router.websocket("/ws/telemetria")
async def websocket_telemetria(websocket: WebSocket) -> None:
    """Endpoint WebSocket para recepção de pacotes do robô e envio de ACKs."""
    await websocket.accept()
    contexto = ContextoConexao()
    logger.info("Conexão WebSocket estabelecida em /ws/telemetria")
    try:
        while True:
            text_frame = await websocket.receive_text()
            with session_scope() as db:
                ack, broadcast_data = await telemetry_service.process_frame(text_frame, db, contexto)
                if ack:
                    await websocket.send_text(ack)
                if broadcast_data:
                    await manager.broadcast(broadcast_data)
    except WebSocketDisconnect:
        logger.info("Conexão WebSocket finalizada em /ws/telemetria")
    except Exception:
        logger.exception("Erro inesperado no WebSocket de telemetria")
    finally:
        # Registrar também quando a conexão falhar durante o envio do ACK.
        try:
            with session_scope() as db:
                registrar_desconexao(db, contexto.id_corrida)
        except Exception:
            logger.exception("Não foi possível registrar a desconexão")


@router.websocket("/ws/painel")
async def websocket_painel(websocket: WebSocket) -> None:
    """Endpoint WebSocket para o painel web receber atualizações da corrida em tempo real."""
    await manager.connect(websocket)
    try:
        while True:
            # O painel apenas escuta broadcasts, mas mantemos o loop vivo
            await websocket.receive_text()
    except WebSocketDisconnect:
        await manager.disconnect(websocket)
    except Exception:
        logger.exception("Erro no WebSocket do painel")
        await manager.disconnect(websocket)
