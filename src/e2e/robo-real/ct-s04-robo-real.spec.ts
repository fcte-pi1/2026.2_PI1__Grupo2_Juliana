// CT-S04 com o robô real (#281, RNF06-S): o painel desenha a telemetria do ESP32
// em menos de 500 ms. Diferente do ct-s04.spec.ts, aqui os pacotes vêm do robô
// pelo Wi-Fi, não do RoboFalso.
//
// Como o relógio do ESP32 não é o do computador, o atraso total é medido em duas
// partes que se somam:
//   1. robô -> backend -> painel: o backend grava, responde o ack e só então
//      repassa ao painel. O RTT (envio -> ack) medido no ESP32 é um limite
//      superior desse trecho. Aparece no monitor serial como rtt=último/médio/máximo.
//   2. chegada no navegador -> tela desenhada: medido aqui, do evento 'message'
//      do WebSocket até o segundo requestAnimationFrame (quadro já pintado).
// Atraso total <= RTT máximo + o maior valor da parte 2.
import { expect, test } from '@playwright/test'
import { capturarTela, csv, salvarEvidencia } from '../helpers/evidencia'
import { abrirPainel, celulaDoMapa } from '../helpers/painel'

const API = process.env.E2E_API_URL ?? 'http://127.0.0.1:8000'
const LATENCIA_MAX_MS = 500 // RNF06-S
const MIN_PACOTES = Number(process.env.E2E_MIN_PACOTES ?? 50)

type Amostra = { t: string; tempo_ms: number | null; tensao: number | null; paredes: number | null; render_ms: number }

test('CT-S04 (robô real) - painel desenha a telemetria do ESP32 em até 500 ms', async ({ page }, testInfo) => {
  // Mede cada mensagem do WebSocket do painel, antes do Dashboard tratá-la.
  await page.addInitScript(() => {
    const amostras: Amostra[] = []
    ;(window as unknown as { __latencias: Amostra[] }).__latencias = amostras
    const Original = window.WebSocket
    class Medido extends Original {
      constructor(url: string | URL, protocolos?: string | string[]) {
        super(url, protocolos)
        this.addEventListener('message', (ev: MessageEvent) => {
          const t0 = performance.now()
          let d: Record<string, unknown> = {}
          try {
            d = JSON.parse(String(ev.data).split('|')[0])
          } catch {
            /* quadro não JSON: mede mesmo assim */
          }
          requestAnimationFrame(() =>
            requestAnimationFrame(() =>
              amostras.push({
                t: String(d.t ?? ''),
                tempo_ms: typeof d.tempo_ms === 'number' ? d.tempo_ms : null,
                tensao: typeof d.tensao_bateria === 'number' ? d.tensao_bateria : null,
                paredes: typeof d.paredes === 'number' ? d.paredes : null,
                render_ms: Number((performance.now() - t0).toFixed(1)),
              }),
            ),
          )
        })
      }
    }
    window.WebSocket = Medido as typeof WebSocket
  })

  // Corrida aberta no sistema web, como a equipe faz antes de soltar o robô.
  const resposta = await fetch(`${API}/corridas`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ id_labirinto: 1 }),
  })
  expect(resposta.status, 'criar corrida 4x4 (há outra em andamento?)').toBe(201)
  const corrida = await resposta.json()

  await abrirPainel(page, 'Monitoramento', '4x4')
  console.log(`\n>>> Corrida ${corrida.id} aberta. Inicie a corrida no robô (comando 'i' ou botão BOOT).\n`)

  // Espera a telemetria do robô chegar.
  await expect
    .poll(async () => page.evaluate(() => (window as unknown as { __latencias: Amostra[] }).__latencias.filter((a) => a.t === 'tel').length), {
      timeout: 150_000,
      intervals: [1000],
    })
    .toBeGreaterThanOrEqual(MIN_PACOTES)

  const amostras: Amostra[] = await page.evaluate(() => (window as unknown as { __latencias: Amostra[] }).__latencias)
  const tel = amostras.filter((a) => a.t === 'tel')
  const render = tel.map((a) => a.render_ms).sort((a, b) => a - b)
  const resumo = {
    pacotes: tel.length,
    render_medio_ms: Number((render.reduce((s, v) => s + v, 0) / render.length).toFixed(1)),
    render_p95_ms: render[Math.floor(render.length * 0.95)],
    render_max_ms: render[render.length - 1],
  }
  console.log('>>> Latência chegada -> tela:', resumo)
  await salvarEvidencia(testInfo, 'latencias-render', csv(tel), 'csv')
  await salvarEvidencia(testInfo, 'resumo', csv([resumo]), 'csv')

  // A tela mostra o último pacote: tensão e paredes da célula (0,0).
  const ultimo = tel[tel.length - 1]
  await expect(page.getByTestId('tensao-bateria')).toHaveText(`${ultimo.tensao!.toFixed(2).replace('.', ',')} V`)
  await expect(celulaDoMapa(page, 0, 0)).toBeVisible()
  await capturarTela(page, testInfo, 'painel-robo-real')

  // Nenhum pacote pode levar mais que 500 ms só para ser desenhado; o total
  // (somando o RTT do monitor serial) é conferido no relatório.
  expect(resumo.render_max_ms).toBeLessThan(LATENCIA_MAX_MS)
})
