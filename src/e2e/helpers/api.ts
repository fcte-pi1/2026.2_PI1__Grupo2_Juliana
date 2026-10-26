// Rotas do backend usadas pelos testes para preparar o cenário (as mesmas que o painel usa).
import { BACKEND_URL } from './ambiente'

export type Labirinto = '4x4' | '8x4' | '12x4'

async function pedir<T>(metodo: 'GET' | 'POST', caminho: string, corpo?: object): Promise<T> {
  const resposta = await fetch(`${BACKEND_URL}${caminho}`, {
    method: metodo,
    headers: corpo ? { 'Content-Type': 'application/json' } : undefined,
    body: corpo ? JSON.stringify(corpo) : undefined,
  })
  if (!resposta.ok) throw new Error(`${metodo} ${caminho}: HTTP ${resposta.status} ${await resposta.text()}`)
  return resposta.json() as Promise<T>
}

/** Id do labirinto cadastrado pela migração 0002 (4x4, 8x4 ou 12x4). */
export async function idLabirinto(nome: Labirinto): Promise<number> {
  const labirintos = await pedir<{ id: number; nome: string }[]>('GET', '/labirintos')
  const labirinto = labirintos.find((l) => l.nome === nome)
  if (!labirinto) throw new Error(`labirinto ${nome} não existe: rode "alembic upgrade head"`)
  return labirinto.id
}

/** Abre a próxima tentativa do labirinto (POST /corridas), à espera do evento `inicio` do robô. */
export async function criarCorrida(labirinto: Labirinto = '4x4'): Promise<number> {
  const corrida = await pedir<{ id: number }>('POST', '/corridas', { id_labirinto: await idLabirinto(labirinto) })
  return corrida.id
}

/** Comando de abortar do painel (POST /corridas/{id}/encerrar): grava a tentativa como abandonada. */
export function encerrarCorrida(id: number) {
  return pedir('POST', `/corridas/${id}/encerrar`)
}
