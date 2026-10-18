import { spawn } from 'node:child_process'
import { BACKEND_DIR, BACKEND_HOST, PYTHON } from './ambiente'

/**
 * mensagens: seqs distintos enviados (reenvios não contam). acks: confirmações recebidas.
 * falhas: linhas "xx" do simulador (mensagem descartada sem ack ou nenhum ack recebido).
 */
export type ResultadoSimulador = { codigo: number | null; saida: string; mensagens: number; acks: number; falhas: string[] }

/**
 * Roda src/backend/simulador.py até o fim e devolve a saída.
 * Argumentos iguais aos da linha de comando, ex.: ['--corromper', 'checksum', '--semente', '1'].
 * Se passar de `limiteMs`, o processo é morto (código null) para não ficar Python órfão.
 */
export function rodarSimulador(args: string[] = [], limiteMs = 45_000): Promise<ResultadoSimulador> {
  return new Promise((resolve, reject) => {
    const url = `ws://${BACKEND_HOST}/ws/telemetria`
    const processo = spawn(PYTHON, ['-u', 'simulador.py', '--url', url, ...args], { cwd: BACKEND_DIR, timeout: limiteMs })
    let saida = ''
    processo.stdout.on('data', (d) => (saida += d))
    processo.stderr.on('data', (d) => (saida += d))
    processo.on('error', reject)
    processo.on('close', (codigo) =>
      resolve({
        codigo,
        saida,
        mensagens: new Set(saida.match(/^-> .*?"seq":\d+/gm)?.map((l) => l.match(/"seq":(\d+)/)![1])).size,
        acks: saida.match(/^<- ack/gm)?.length ?? 0,
        falhas: saida.match(/^xx .*/gm) ?? [],
      }),
    )
  })
}
