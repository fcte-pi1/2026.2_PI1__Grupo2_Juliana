import pg from 'pg'
import { E2E_DATABASE_URL } from './ambiente'

// O backend usa o driver psycopg do SQLAlchemy; o node-postgres só entende "postgresql://".
// Um pool para todos os specs; allowExitOnIdle deixa o processo terminar sem fechar o pool à mão.
const pool = new pg.Pool({
  connectionString: E2E_DATABASE_URL.replace(/^postgresql\+\w+:/, 'postgresql:'),
  allowExitOnIdle: true,
})

export async function consultar<T extends pg.QueryResultRow = pg.QueryResultRow>(sql: string, params: unknown[] = []) {
  const { rows } = await pool.query<T>(sql, params)
  return rows
}

/** Apaga corridas, leituras e logs. Os labirintos 4x4, 8x4 e 12x4 da migração 0002 ficam. */
export async function limparBanco() {
  await pool.query('TRUNCATE leitura_telemetria, log_evento, corrida RESTART IDENTITY CASCADE')
}

/**
 * Cria a corrida em_andamento que o backend espera antes do evento `inicio`.
 * Provisório: troca pela rota de criar corrida quando a 3.8.2 entrar na develop.
 */
export async function criarCorrida(labirinto: '4x4' | '8x4' | '12x4' = '4x4', numTentativa = 1): Promise<number> {
  const [linha] = await consultar<{ id: number }>(
    `INSERT INTO corrida (id_labirinto, num_tentativa)
     SELECT id, $2 FROM labirinto WHERE nome = $1
     RETURNING id`,
    [labirinto, numTentativa],
  )
  if (!linha) throw new Error(`labirinto ${labirinto} não existe: rode "alembic upgrade head"`)
  return linha.id
}
