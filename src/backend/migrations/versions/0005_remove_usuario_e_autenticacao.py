"""remove usuario e autenticacao

A autenticação (RF21-S) saiu do escopo deste ciclo (issue #221), então a corrida deixa de
ter dono: sai a tabela usuario e a coluna corrida.id_usuario (4.4.4 v1.4).

A ordem do --autogenerate foi corrigida à mão: no upgrade, a FK e a coluna saem antes da
tabela que elas referenciam; no downgrade, a tabela volta antes da FK. No downgrade, a
coluna id_usuario volta aceitando nulo, porque as corridas criadas sem usuário não têm
um valor para preencher.

Revision ID: 0005
Revises: 0004
Create Date: 2026-10-14 16:30:49.259809

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects import postgresql

# revision identifiers, used by Alembic.
revision: str = '0005'
down_revision: Union[str, None] = '0004'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.drop_constraint('fk_corrida_id_usuario_usuario', 'corrida', type_='foreignkey')
    op.drop_column('corrida', 'id_usuario')
    op.drop_table('usuario')


def downgrade() -> None:
    op.create_table('usuario',
    sa.Column('matricula', sa.VARCHAR(length=20), autoincrement=False, nullable=False),
    sa.Column('nome', sa.VARCHAR(length=100), autoincrement=False, nullable=False),
    sa.Column('senha_hash', sa.VARCHAR(length=255), autoincrement=False, nullable=False),
    sa.Column('criado_em', postgresql.TIMESTAMP(timezone=True), server_default=sa.text('now()'), autoincrement=False, nullable=False),
    sa.PrimaryKeyConstraint('matricula', name='pk_usuario')
    )
    op.add_column('corrida', sa.Column('id_usuario', sa.VARCHAR(length=20), autoincrement=False, nullable=True))
    op.create_foreign_key('fk_corrida_id_usuario_usuario', 'corrida', 'usuario', ['id_usuario'], ['matricula'], ondelete='RESTRICT')
