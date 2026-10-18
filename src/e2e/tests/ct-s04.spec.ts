// CT-S04 - Monitoramento da corrida em tempo real (RF06-S / HU #89, CA1 e CA2; RNF06-S)
import { criarCorrida } from '../helpers/banco'
import { capturarTela, csv, salvarEvidencia } from '../helpers/evidencia'
import { abrirPainel, celulaDoMapa } from '../helpers/painel'
import { RoboFalso, type Tel } from '../helpers/robo'
import { expect, test } from '../helpers/teste'

const LATENCIA_MAX_MS = 500 // RNF06-S

// Trajeto no 4x4 saindo de (0, 0); tensão diferente em cada pacote para saber qual está na tela
const CORRIDA: Partial<Tel>[] = [
  { pos_x: 0, pos_y: 0, orientacao: 'N', paredes: 14, velocidade: 0.18, tensao_bateria: 8.4, estado: 'explorando' },
  { pos_x: 0, pos_y: 1, orientacao: 'N', paredes: 10, velocidade: 0.19, tensao_bateria: 8.38, estado: 'explorando' },
  { pos_x: 0, pos_y: 2, orientacao: 'L', paredes: 9, velocidade: 0.2, tensao_bateria: 8.36, estado: 'explorando' },
  { pos_x: 1, pos_y: 2, orientacao: 'L', paredes: 5, velocidade: 0.21, tensao_bateria: 8.34, estado: 'explorando' },
  { pos_x: 2, pos_y: 2, orientacao: 'S', paredes: 3, velocidade: 0.22, tensao_bateria: 8.32, estado: 'retornando' },
  { pos_x: 2, pos_y: 1, orientacao: 'S', paredes: 6, velocidade: 0.23, tensao_bateria: 8.3, estado: 'rapido' },
]
const ROTULO_ESTADO = { explorando: 'Explorando labirinto', retornando: 'Retornando ao início', rapido: 'Corrida rápida', parado: 'Parado' }
const br = (n: number) => n.toFixed(2).replace('.', ',')

test('CT-S04 - painel atualiza mapa, paredes, bateria, velocidade e status em até 500 ms', async ({ page }, testInfo) => {
  await criarCorrida('4x4')
  await abrirPainel(page, 'Monitoramento')
  const robo = await RoboFalso.conectar()
  await robo.inicio()

  const latencias: Record<string, unknown>[] = []
  for (const [i, tel] of CORRIDA.entries()) {
    const enviadoEm = Date.now()
    const pacote = await robo.tel(tel)
    // Tempo do envio até a tensão deste pacote aparecer na tela (sem recarregar a página)
    await expect(page.getByTestId('tensao-bateria')).toHaveText(`${br(tel.tensao_bateria!)} V`, { timeout: LATENCIA_MAX_MS })
    const renderizadoEm = Date.now()
    latencias.push({ pacote: i + 1, tempo_ms: pacote.tempo_ms, enviado_em: enviadoEm, renderizado_em: renderizadoEm, latencia_ms: renderizadoEm - enviadoEm })

    // Mesmos valores que foram enviados
    await expect(page.getByTestId('velocidade-atual')).toHaveText(`${br(tel.velocidade!)} m/s`)
    await expect(page.getByTestId('texto-status-execucao')).toHaveText(ROTULO_ESTADO[tel.estado!])
    // Trajeto desenhado célula a célula, com as paredes enviadas
    await expect(celulaDoMapa(page, tel.pos_x!, tel.pos_y!)).toHaveClass(/observed/)
    await expect(celulaDoMapa(page, tel.pos_x!, tel.pos_y!).locator('title')).toHaveText(
      `Célula (${tel.pos_x}, ${tel.pos_y}): máscara ${tel.paredes}`,
    )
    await expect(page.getByTestId('espaco-mapa-labirinto')).toHaveAttribute(
      'data-trajeto',
      CORRIDA.slice(0, i + 1).map((c) => `${c.pos_x},${c.pos_y}`).join(';'),
    )
    // "Atualizado às" acompanha a chegada dos pacotes: mostra um horário entre o envio e a conferência
    const horariosPossiveis = [...new Set([horario(enviadoEm), horario(renderizadoEm)])].map((h) => `Atualizado às ${h}`)
    await expect(page.getByTestId('horario-atualizacao')).toHaveText(new RegExp(`^(${horariosPossiveis.join('|')})$`))
  }
  robo.fechar()
  await salvarEvidencia(testInfo, 'latencias', csv(latencias), 'csv')
  await capturarTela(page, testInfo, 'durante-corrida')

  // Pausa de 2 s no envio (passo 4 do roteiro): os últimos valores continuam na tela.
  // A espera fixa aqui é o próprio cenário do teste, não uma espera por algo acontecer.
  const ultimo = CORRIDA.at(-1)!
  await page.waitForTimeout(2000)
  await expect(page.getByTestId('tensao-bateria')).toHaveText(`${br(ultimo.tensao_bateria!)} V`)
  await expect(page.getByTestId('velocidade-atual')).toHaveText(`${br(ultimo.velocidade!)} m/s`)
  await expect(page.getByTestId('texto-status-execucao')).toHaveText(ROTULO_ESTADO[ultimo.estado!])
  await expect(page.getByTestId('espaco-mapa-labirinto')).toHaveAttribute('data-celulas-count', String(CORRIDA.length))
  await capturarTela(page, testInfo, 'apos-pausa-2s')
})

/** HH:MM:SS no fuso local, igual ao formatarHorario do Dashboard.jsx. */
function horario(ms: number) {
  return new Date(ms).toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit', second: '2-digit' })
}
