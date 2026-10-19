"""Registro de desconexões do robô."""

from sqlalchemy.orm import Session

from app.db.models import Corrida, LogEvento


def registrar_desconexao(db: Session, id_corrida: int | None) -> None:
    """Registra a perda sem encerrar a corrida, permitindo reconexão."""
    # Sem corrida confirmada (conexão de teste, navegador) ou após o fim confirmado,
    # o fechamento do socket não é perda durante a corrida.
    if id_corrida is None:
        return
    corrida = db.get(Corrida, id_corrida)
    if corrida is None or corrida.status != "em_andamento":
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
