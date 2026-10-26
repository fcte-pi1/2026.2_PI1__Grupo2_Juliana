import { existsSync, readFileSync } from 'node:fs'
import path from 'node:path'

export const BACKEND_DIR = path.resolve(import.meta.dirname, '../../backend')
export const FRONTEND_DIR = path.resolve(import.meta.dirname, '../../frontend')

// Portas próprias: um backend de desenvolvimento aberto na 8000 está ligado no banco principal
// e não pode ser reaproveitado pelos testes. Mude por variável se a porta estiver ocupada.
export const BACKEND_PORTA = Number(process.env.E2E_BACKEND_PORTA ?? 18000)
export const FRONTEND_PORTA = Number(process.env.E2E_FRONTEND_PORTA ?? 15173)
// 127.0.0.1 e não localhost: no Windows o Node tenta primeiro o IPv6 (::1), onde o uvicorn não escuta,
// e a espera pelo backend passava de 30 s.
export const BACKEND_HOST = `127.0.0.1:${BACKEND_PORTA}`
export const BACKEND_URL = `http://${BACKEND_HOST}`

// Banco principal: DATABASE_URL do ambiente ou de src/backend/.env (o mesmo do pytest).
function lerDatabaseUrl(): string {
  if (process.env.DATABASE_URL) return process.env.DATABASE_URL
  const arquivo = path.join(BACKEND_DIR, '.env')
  const linha = existsSync(arquivo)
    ? readFileSync(arquivo, 'utf8').split(/\r?\n/).find((l) => l.startsWith('DATABASE_URL='))
    : undefined
  if (!linha) throw new Error('Defina DATABASE_URL ou crie src/backend/.env (veja src/backend/README.md).')
  return linha.slice('DATABASE_URL='.length).trim()
}

export const DATABASE_URL = lerDatabaseUrl()

// Os testes apagam dados, então rodam num banco à parte no mesmo servidor: <nome>_e2e.
function bancoE2e(): string {
  const url = new URL(process.env.E2E_DATABASE_URL ?? DATABASE_URL)
  if (!process.env.E2E_DATABASE_URL) url.pathname += '_e2e'
  if (url.pathname === new URL(DATABASE_URL).pathname) {
    throw new Error('E2E_DATABASE_URL não pode ser o mesmo banco do DATABASE_URL: os testes apagam dados.')
  }
  return url.toString()
}

export const E2E_DATABASE_URL = bancoE2e()

// Python do venv do backend, se existir; senão o do PATH (caso do CI).
function acharPython(): string {
  if (process.env.PYTHON) return process.env.PYTHON
  const venv = process.platform === 'win32' ? '.venv/Scripts/python.exe' : '.venv/bin/python'
  return existsSync(path.join(BACKEND_DIR, venv)) ? path.join(BACKEND_DIR, venv) : 'python'
}

export const PYTHON = acharPython()
