"""insere labirintos da disciplina

Seed de desenvolvimento: os três labirintos da apresentação final (4 x 4, 8 x 4 e 12 x 4),
usados nos casos de teste CT-S09 a CT-S12. O usuário de teste não entra aqui porque
o hash da senha depende da autenticação (Atividade 3.6).

Revision ID: 0002
Revises: 0001
Create Date: 2026-10-08 13:07:52.658901

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = '0002'
down_revision: Union[str, None] = '0001'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.execute(
        """
        INSERT INTO labirinto (nome, largura_celula, altura_celula) VALUES
            ('4x4', 4, 4),
            ('8x4', 8, 4),
            ('12x4', 12, 4)
        """
    )


def downgrade() -> None:
    op.execute("DELETE FROM labirinto WHERE nome IN ('4x4', '8x4', '12x4')")
