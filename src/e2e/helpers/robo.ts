// "Robô falso": fala o Contrato de Telemetria (4.4.7) direto pelo WebSocket, pacote a pacote.
// Use quando o caso precisa de pacotes exatos (valores, inválidos, pausas). Para uma corrida
// inteira e realista, use rodarSimulador.
import { BACKEND_HOST } from './ambiente'
import { criarCorrida, type Labirinto } from './api'

/** CRC-16/CCITT-FALSE do contrato (seção 4): polinômio 0x1021, inicial 0xFFFF. */
export function crc16(texto: string): string {
  let crc = 0xffff
  for (const byte of new TextEncoder().encode(texto)) {
    crc ^= byte << 8
    for (let b = 0; b < 8; b++) crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff
  }
  return crc.toString(16).toUpperCase().padStart(4, '0')
}

/** Serializa no formato <JSON>|<CRC>. */
export function quadro(msg: object): string {
  const json = JSON.stringify(msg)
  return `${json}|${crc16(json)}`
}

export type Tel = {
  pos_x: number
  pos_y: number
  orientacao: 'N' | 'S' | 'L' | 'O'
  paredes: number
  velocidade: number
  tensao_bateria: number
  valor_flood: number | null
  estado: 'explorando' | 'retornando' | 'rapido' | 'parado'
}

const TEL_PADRAO: Tel = {
  pos_x: 0,
  pos_y: 0,
  orientacao: 'N',
  paredes: 0,
  velocidade: 0.2,
  tensao_bateria: 8.4,
  valor_flood: 6,
  estado: 'explorando',
}

const ESPERA_ACK_MS = 3000

function abrirSocket(): Promise<WebSocket> {
  const ws = new WebSocket(`ws://${BACKEND_HOST}/ws/telemetria`)
  return new Promise((resolve, reject) => {
    ws.addEventListener('open', () => resolve(ws), { once: true })
    ws.addEventListener('error', () => reject(new Error('robô falso não conectou no backend')), { once: true })
  })
}

export class RoboFalso {
  private seq = 0
  private tempoMs = 0
  private acks = new Set<number>()
  private esperando = new Map<number, () => void>()

  private constructor(
    private ws: WebSocket,
    readonly corrida: number,
  ) {
    this.ouvirAcks()
  }

  private ouvirAcks() {
    this.ws.addEventListener('message', (e) => {
      const texto = String(e.data)
      const json = texto.slice(0, texto.lastIndexOf('|'))
      const ack = JSON.parse(json)
      if (ack.t !== 'ack') return
      this.acks.add(ack.seq)
      this.esperando.get(ack.seq)?.()
    })
  }

  /** Conecta em /ws/telemetria como o robô de número `corrida`. */
  static async conectar(corrida = 1): Promise<RoboFalso> {
    return new RoboFalso(await abrirSocket(), corrida)
  }

  /**
   * Abre outra conexão depois de `fechar()`, como o robô quando o Wi-Fi volta.
   * Mantém seq e tempo_ms: o backend descarta como reenvio um evento com seq já visto na corrida.
   */
  async reconectar() {
    this.ws = await abrirSocket()
    this.ouvirAcks()
  }

  /** Envia um texto qualquer, sem cabeçalho nem CRC (para pacotes malformados). */
  enviarBruto(texto: string) {
    this.ws.send(texto)
  }

  /** Completa o cabeçalho (v, corrida, seq), envia e espera o ack. Devolve o pacote enviado. */
  async enviar<T extends Record<string, unknown>>(msg: T) {
    const pacote = { v: 1, corrida: this.corrida, seq: this.seq++, ...msg }
    const ack = this.esperarAck(pacote.seq)
    this.ws.send(quadro(pacote))
    await ack
    return pacote
  }

  inicio(tensao_bateria = 8.4) {
    this.tempoMs = 0
    return this.enviar({ t: 'evt', tempo_ms: 0, evento: 'inicio', tensao_bateria })
  }

  /** Leitura periódica; o tempo_ms avança 200 ms a cada chamada, como no robô (5 Hz). */
  tel(campos: Partial<Tel> = {}) {
    this.tempoMs += 200
    return this.enviar({ t: 'tel', tempo_ms: this.tempoMs, ...TEL_PADRAO, ...campos })
  }

  fim(resultado: 'concluida' | 'abandonada' = 'concluida', motivo = 'manual') {
    const extra = resultado === 'concluida' ? { tempo_conc_s: this.tempoMs / 1000 } : { motivo }
    return this.enviar({ t: 'evt', tempo_ms: this.tempoMs, evento: 'fim', resultado, consumo_medio_ma: 400, ...extra })
  }

  /** Primeiro pacote depois de voltar a conexão: quanto tempo ficou offline e quantos pacotes ficaram retidos. */
  reconexao(offline_ms: number, pendentes = 0) {
    return this.enviar({ t: 'evt', tempo_ms: this.tempoMs, evento: 'reconexao', offline_ms, pendentes })
  }

  /** Monta um pacote tel com cabeçalho, sem enviar (para o teste corromper antes). */
  montarTel(campos: Partial<Tel> = {}) {
    this.tempoMs += 200
    return { v: 1, t: 'tel', corrida: this.corrida, seq: this.seq++, tempo_ms: this.tempoMs, ...TEL_PADRAO, ...campos }
  }

  recebeuAck(seq: number) {
    return this.acks.has(seq)
  }

  private esperarAck(seq: number) {
    return new Promise<void>((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error(`sem ack do seq ${seq} em ${ESPERA_ACK_MS} ms`)), ESPERA_ACK_MS)
      this.esperando.set(seq, () => {
        clearTimeout(timer)
        this.esperando.delete(seq)
        resolve()
      })
    })
  }

  fechar() {
    this.ws.close()
  }
}

/** Trajeto curto dentro de qualquer labirinto oficial (4 células, 1 s de corrida). */
export const TRAJETO_CURTO: Partial<Tel>[] = [
  { pos_x: 0, pos_y: 0, paredes: 10, velocidade: 0.2 },
  { pos_x: 0, pos_y: 1, paredes: 8, velocidade: 0.3 },
  { pos_x: 1, pos_y: 1, paredes: 5, velocidade: 0.3 },
  { pos_x: 2, pos_y: 1, paredes: 1, velocidade: 0.2 },
]

/**
 * Uma tentativa inteira em ~1 s pelo robô falso: abre a corrida pela rota, manda inicio, as leituras e fim.
 * Para montar a massa de dados das consultas (CT-S09 a CT-S13) sem esperar o simulador.
 */
export async function correrComRoboFalso(
  labirinto: Labirinto = '4x4',
  resultado: 'concluida' | 'abandonada' = 'concluida',
  trajeto = TRAJETO_CURTO,
) {
  const id = await criarCorrida(labirinto)
  const robo = await RoboFalso.conectar()
  await robo.inicio()
  for (const leitura of trajeto) await robo.tel(leitura)
  await robo.fim(resultado)
  robo.fechar()
  return id
}

/** Massa de dados do CT-S09, usada também pelo CT-S10, CT-S12 e CT-S13: 4x4, 4x4, 8x4, 4x4. Devolve os ids. */
export async function massaDoCtS09() {
  const ids: Record<Labirinto, number[]> = { '4x4': [], '8x4': [], '12x4': [] }
  for (const labirinto of ['4x4', '4x4', '8x4', '4x4'] as const) ids[labirinto].push(await correrComRoboFalso(labirinto))
  return ids
}

/** Escuta /ws/painel como se fosse o site e guarda tudo que o backend retransmitir. */
export async function ouvirPainel() {
  const ws = new WebSocket(`ws://${BACKEND_HOST}/ws/painel`)
  const mensagens: Record<string, any>[] = []
  ws.addEventListener('message', (e) => mensagens.push(JSON.parse(String(e.data))))
  await new Promise((resolve, reject) => {
    ws.addEventListener('open', resolve, { once: true })
    ws.addEventListener('error', () => reject(new Error('não conectou em /ws/painel')), { once: true })
  })
  return { mensagens, fechar: () => ws.close() }
}
