"""cria tabelas iniciais

Cria as 5 tabelas do Dicionário de Dados (seção 7) da 4.4.4 - Persistência de Dados,
com as restrições de integridade da seção 5.

Revision ID: 0001
Revises: 
Create Date: 2026-10-08 13:07:52.213526

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = '0001'
down_revision: Union[str, None] = None
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table('labirinto',
    sa.Column('id', sa.Integer(), nullable=False),
    sa.Column('nome', sa.String(length=100), nullable=False),
    sa.Column('largura_celula', sa.SmallInteger(), nullable=False),
    sa.Column('altura_celula', sa.SmallInteger(), nullable=False),
    sa.Column('criado_em', sa.DateTime(timezone=True), server_default=sa.text('now()'), nullable=False),
    sa.CheckConstraint('altura_celula > 0', name=op.f('ck_labirinto_altura_positiva')),
    sa.CheckConstraint('largura_celula > 0', name=op.f('ck_labirinto_largura_positiva')),
    sa.PrimaryKeyConstraint('id', name=op.f('pk_labirinto')),
    sa.UniqueConstraint('nome', name=op.f('uq_labirinto_nome'))
    )
    op.create_table('usuario',
    sa.Column('matricula', sa.String(length=20), nullable=False),
    sa.Column('nome', sa.String(length=100), nullable=False),
    sa.Column('senha_hash', sa.String(length=255), nullable=False),
    sa.Column('criado_em', sa.DateTime(timezone=True), server_default=sa.text('now()'), nullable=False),
    sa.PrimaryKeyConstraint('matricula', name=op.f('pk_usuario'))
    )
    op.create_table('corrida',
    sa.Column('id', sa.Integer(), nullable=False),
    sa.Column('id_labirinto', sa.Integer(), nullable=False),
    sa.Column('id_usuario', sa.String(length=20), nullable=False),
    sa.Column('num_tentativa', sa.SmallInteger(), nullable=False),
    sa.Column('status', sa.String(length=20), server_default='em_andamento', nullable=False),
    sa.Column('iniciado_em', sa.DateTime(timezone=True), server_default=sa.text('now()'), nullable=False),
    sa.Column('tempo_conc_s', sa.Numeric(precision=8, scale=3), nullable=True),
    sa.Column('consumo_medio_ma', sa.Numeric(precision=8, scale=2), nullable=True),
    sa.CheckConstraint("status <> 'concluida' OR tempo_conc_s IS NOT NULL", name=op.f('ck_corrida_concluida_tem_tempo')),
    sa.CheckConstraint("status IN ('em_andamento', 'concluida', 'abandonada')", name=op.f('ck_corrida_status_valido')),
    sa.CheckConstraint('consumo_medio_ma >= 0', name=op.f('ck_corrida_consumo_nao_negativo')),
    sa.CheckConstraint('num_tentativa > 0', name=op.f('ck_corrida_num_tentativa_positivo')),
    sa.CheckConstraint('tempo_conc_s >= 0', name=op.f('ck_corrida_tempo_conc_nao_negativo')),
    sa.ForeignKeyConstraint(['id_labirinto'], ['labirinto.id'], name=op.f('fk_corrida_id_labirinto_labirinto'), ondelete='RESTRICT'),
    sa.ForeignKeyConstraint(['id_usuario'], ['usuario.matricula'], name=op.f('fk_corrida_id_usuario_usuario'), ondelete='RESTRICT'),
    sa.PrimaryKeyConstraint('id', name=op.f('pk_corrida')),
    sa.UniqueConstraint('id_labirinto', 'num_tentativa', name=op.f('uq_corrida_id_labirinto_num_tentativa'))
    )
    op.create_table('leitura_telemetria',
    sa.Column('id', sa.BigInteger(), nullable=False),
    sa.Column('id_corrida', sa.Integer(), nullable=False),
    sa.Column('tempo_ms', sa.Integer(), nullable=False),
    sa.Column('recebido_em', sa.DateTime(timezone=True), server_default=sa.text('now()'), nullable=False),
    sa.Column('pos_x', sa.SmallInteger(), nullable=False),
    sa.Column('pos_y', sa.SmallInteger(), nullable=False),
    sa.Column('orientacao', sa.CHAR(length=1), nullable=False),
    sa.Column('paredes', sa.SmallInteger(), nullable=False),
    sa.Column('velocidade', sa.Numeric(precision=6, scale=3), nullable=False),
    sa.Column('tensao_bateria', sa.Numeric(precision=5, scale=2), nullable=False),
    sa.Column('valor_flood', sa.SmallInteger(), nullable=True),
    sa.CheckConstraint("orientacao IN ('N', 'S', 'L', 'O')", name=op.f('ck_leitura_telemetria_orientacao_valida')),
    sa.CheckConstraint('paredes BETWEEN 0 AND 15', name=op.f('ck_leitura_telemetria_paredes_valida')),
    sa.CheckConstraint('pos_x >= 0 AND pos_y >= 0', name=op.f('ck_leitura_telemetria_posicao_nao_negativa')),
    sa.CheckConstraint('tempo_ms >= 0', name=op.f('ck_leitura_telemetria_tempo_ms_nao_negativo')),
    sa.CheckConstraint('tensao_bateria >= 0', name=op.f('ck_leitura_telemetria_tensao_nao_negativa')),
    sa.CheckConstraint('valor_flood >= 0', name=op.f('ck_leitura_telemetria_valor_flood_nao_negativo')),
    sa.CheckConstraint('velocidade >= 0', name=op.f('ck_leitura_telemetria_velocidade_nao_negativa')),
    sa.ForeignKeyConstraint(['id_corrida'], ['corrida.id'], name=op.f('fk_leitura_telemetria_id_corrida_corrida'), ondelete='CASCADE'),
    sa.PrimaryKeyConstraint('id', name=op.f('pk_leitura_telemetria')),
    sa.UniqueConstraint('id_corrida', 'tempo_ms', name=op.f('uq_leitura_telemetria_id_corrida_tempo_ms'))
    )
    op.create_table('log_evento',
    sa.Column('id', sa.BigInteger(), nullable=False),
    sa.Column('id_corrida', sa.Integer(), nullable=True),
    sa.Column('tipo', sa.String(length=10), nullable=False),
    sa.Column('origem', sa.String(length=20), nullable=False),
    sa.Column('mensagem', sa.Text(), nullable=False),
    sa.Column('ocorrido_em', sa.DateTime(timezone=True), server_default=sa.text('now()'), nullable=False),
    sa.CheckConstraint("origem IN ('firmware', 'backend', 'frontend')", name=op.f('ck_log_evento_origem_valida')),
    sa.CheckConstraint("tipo IN ('info', 'aviso', 'erro')", name=op.f('ck_log_evento_tipo_valido')),
    sa.ForeignKeyConstraint(['id_corrida'], ['corrida.id'], name=op.f('fk_log_evento_id_corrida_corrida'), ondelete='SET NULL'),
    sa.PrimaryKeyConstraint('id', name=op.f('pk_log_evento'))
    )
    op.create_index(op.f('ix_log_evento_id_corrida'), 'log_evento', ['id_corrida'], unique=False)


def downgrade() -> None:
    op.drop_index(op.f('ix_log_evento_id_corrida'), table_name='log_evento')
    op.drop_table('log_evento')
    op.drop_table('leitura_telemetria')
    op.drop_table('corrida')
    op.drop_table('usuario')
    op.drop_table('labirinto')
