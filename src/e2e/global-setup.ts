// Antes da suíte: garante que o banco de testes existe e está na última migração.
import { execFileSync } from 'node:child_process'
import pg from 'pg'
import { BACKEND_DIR, DATABASE_URL, E2E_DATABASE_URL, PYTHON } from './helpers/ambiente'

const paraPg = (url: string) => url.replace(/^postgresql\+\w+:/, 'postgresql:')

export default async function globalSetup() {
  const nome = new URL(E2E_DATABASE_URL).pathname.slice(1)
  const principal = new pg.Client({ connectionString: paraPg(DATABASE_URL) })
  await principal.connect()
  try {
    const { rowCount } = await principal.query('SELECT 1 FROM pg_database WHERE datname = $1', [nome])
    if (!rowCount) await principal.query(`CREATE DATABASE "${nome}"`)
  } finally {
    await principal.end()
  }

  execFileSync(PYTHON, ['-m', 'alembic', 'upgrade', 'head'], {
    cwd: BACKEND_DIR,
    env: { ...process.env, DATABASE_URL: E2E_DATABASE_URL },
    stdio: 'inherit',
  })
}
