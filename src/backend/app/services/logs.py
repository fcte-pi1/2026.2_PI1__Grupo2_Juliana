"""Registro de desconexões do robô."""

from sqlalchemy.orm import Session

from app.db.models import Corrida, LogEvento


def registrar_desconexao(db: Session, id_corrida: int | None) -> None:
    """Registra a perda sem encerrar a corrida, permitindo reconexão."""
    if id_corrida is not None:
        corrida = db.get(Corrida, id_corrida)
        if corrida is None:
            id_corrida = None
        elif corrida.status != "em_andamento":
            # Fechar o socket após o fim confirmado não é perda durante a corrida.
            return
    db.add(LogEvento(
        id_corrida=id_corrida,
        tipo="aviso",
        origem="backend",
        mensagem="desconexao: conexao WebSocket do robo encerrada",
    ))
    try:
        db.commit()
    except Exception:
        db.rollback()
        raise
