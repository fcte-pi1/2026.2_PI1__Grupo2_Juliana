"""permite saida negativa em leitura telemetria

Relaxa a restrição ck_leitura_telemetria_posicao_nao_negativa para aceitar pos_x >= -1 e
pos_y >= -1 (em um único eixo por vez, proibindo diagonal (-1, -1)), permitindo persistir
o último pacote de telemetria quando o robô sai do labirinto pela borda Oeste (pos_x = -1)
ou Norte (pos_y = -1) (4.4.4 v1.5).

Revision ID: 0006
Revises: 0005
Create Date: 2026-10-20 22:00:00.000000

"""
from typing import Sequence, Union

from alembic import op


# revision identifiers, used by Alembic.
revision: str = '0006'
down_revision: Union[str, None] = '0005'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.drop_constraint(
        op.f('ck_leitura_telemetria_posicao_nao_negativa'),
        'leitura_telemetria',
        type_='check',
    )
    op.create_check_constraint(
        op.f('ck_leitura_telemetria_posicao_nao_negativa'),
        'leitura_telemetria',
        'pos_x >= -1 AND pos_y >= -1 AND NOT (pos_x = -1 AND pos_y = -1)',
    )


def downgrade() -> None:
    op.drop_constraint(
        op.f('ck_leitura_telemetria_posicao_nao_negativa'),
        'leitura_telemetria',
        type_='check',
    )
    op.create_check_constraint(
        op.f('ck_leitura_telemetria_posicao_nao_negativa'),
        'leitura_telemetria',
        'pos_x >= 0 AND pos_y >= 0',
    )
