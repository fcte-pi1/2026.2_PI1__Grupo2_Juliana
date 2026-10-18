// CT-S01 - Recepção de telemetria do Micromouse (RF05-S / HU #88, CA1 e CA2)
import { consultar, criarCorrida } from '../helpers/banco'
import { csv, salvarEvidencia } from '../helpers/evidencia'
import { ouvirPainel } from '../helpers/robo'
import { rodarSimulador } from '../helpers/simulador'
import { expect, test } from '../helpers/teste'

const CAMPOS_TEL = ['corrida', 'tempo_ms', 'pos_x', 'pos_y', 'orientacao', 'paredes', 'velocidade', 'tensao_bateria', 'valor_flood', 'estado']

test('CT-S01 - recebe todos os pacotes da corrida, com todos os campos e a 5 pacotes/s', async ({}, testInfo) => {
  const idCorrida = await criarCorrida('4x4')
  const painel = await ouvirPainel()

  // Corrida inteira no labirinto 4x4, na velocidade real do robô (5 Hz)
  const sim = await rodarSimulador(['--semente', '1'])
  // O backend manda o ack antes de retransmitir ao painel: espera o evento "fim" chegar
  await expect.poll(() => painel.mensagens.some((m) => m.evento === 'fim')).toBe(true)
  painel.fechar()

  const leituras = await consultar<{ tempo_ms: number; recebido_ms: string; pos_x: number; pos_y: number; paredes: number }>(
    `SELECT tempo_ms, extract(epoch FROM recebido_em) * 1000 AS recebido_ms, pos_x, pos_y, orientacao, paredes,
            velocidade, tensao_bateria, valor_flood
       FROM leitura_telemetria WHERE id_corrida = $1 ORDER BY tempo_ms`,
    [idCorrida],
  )
  const telsEnviados = new Set(sim.saida.match(/^-> .*"t":"tel".*$/gm)?.map((l) => l.match(/"seq":(\d+)/)![1])).size
  const telsNoPainel = painel.mensagens.filter((m) => m.t === 'tel')
  const recebidos = leituras.map((l) => Number(l.recebido_ms))
  const intervalosRobo = leituras.slice(1).map((l, i) => l.tempo_ms - leituras[i].tempo_ms)
  const intervalosServidor = recebidos.slice(1).map((r, i) => r - recebidos[i])
  const taxaRecebida = (recebidos.length - 1) / ((recebidos.at(-1)! - recebidos[0]) / 1000)

  await salvarEvidencia(testInfo, 'log-simulador', sim.saida, 'txt')
  await salvarEvidencia(testInfo, 'pacotes-recebidos', csv(leituras), 'csv')
  await salvarEvidencia(
    testInfo,
    'resumo',
    csv([{ enviados: telsEnviados, gravados: leituras.length, no_painel: telsNoPainel.length, taxa_recebida_por_s: taxaRecebida.toFixed(3), maior_intervalo_tempo_ms: Math.max(...intervalosRobo), maior_intervalo_servidor_ms: Math.round(Math.max(...intervalosServidor)) }]),
    'csv',
  )

  // O servidor reconhece e processa todos: todo pacote teve ack e nada foi descartado
  expect(sim.falhas).toEqual([])
  expect(sim.acks).toBe(sim.mensagens)

  // Total recebido = total enviado, sem perdas
  expect(leituras.length).toBeGreaterThan(0)
  expect(leituras.length, 'leituras gravadas = pacotes tel enviados').toBe(telsEnviados)
  expect(telsNoPainel.length, 'pacotes retransmitidos ao painel').toBe(telsEnviados)

  // Cada pacote traz todos os campos do Contrato de Telemetria (4.4.7)
  for (const pacote of telsNoPainel) expect(Object.keys(pacote)).toEqual(expect.arrayContaining(CAMPOS_TEL))

  // Nenhum intervalo entre tempo_ms consecutivos passa de 300 ms (no relógio do robô: 5 pacotes/s)
  expect(Math.max(...intervalosRobo)).toBeLessThanOrEqual(300)
  // Na chegada ao servidor: taxa média de 5 pacotes/s e nenhum buraco maior que 300 ms.
  // Tolerância de 2% na média porque o simulador atrasa alguns ms por pacote e depois compensa.
  expect(taxaRecebida).toBeGreaterThanOrEqual(4.9)
  expect(Math.max(...intervalosServidor)).toBeLessThanOrEqual(300)
})
