"""Gerenciador de conexões WebSocket para o painel em tempo real (RNF13-S)."""

import asyncio
import json
import logging
from typing import Any

from fastapi import WebSocket

logger = logging.getLogger("websocket_manager")


class ConnectionManager:
    """Gerencia conexões WebSocket ativas com os clientes do painel."""

    def __init__(self) -> None:
        self.active_connections: list[WebSocket] = []
        self._lock = asyncio.Lock()

    async def connect(self, websocket: WebSocket) -> None:
        """Aceita a conexão e a adiciona à lista de clientes ativos."""
        await websocket.accept()
        async with self._lock:
            self.active_connections.append(websocket)
        logger.info("Cliente conectado ao painel. Total ativos: %d", len(self.active_connections))

    async def disconnect(self, websocket: WebSocket) -> None:
        """Remove a conexão da lista de clientes ativos."""
        async with self._lock:
            if websocket in self.active_connections:
                self.active_connections.remove(websocket)
        logger.info("Cliente desconectado do painel. Total ativos: %d", len(self.active_connections))

    async def broadcast(self, message: dict[str, Any] | str) -> None:
        """Transmite uma mensagem para todos os clientes conectados simultaneamente."""
        text = json.dumps(message) if isinstance(message, dict) else message
        async with self._lock:
            connections = list(self.active_connections)

        if not connections:
            return

        dead_connections = []
        for connection in connections:
            try:
                await connection.send_text(text)
            except Exception as e:
                logger.warning("Falha ao enviar para cliente do painel: %s", e)
                dead_connections.append(connection)

        if dead_connections:
            async with self._lock:
                for dead in dead_connections:
                    if dead in self.active_connections:
                        self.active_connections.remove(dead)

    @property
    def total_clients(self) -> int:
        return len(self.active_connections)


manager = ConnectionManager()
