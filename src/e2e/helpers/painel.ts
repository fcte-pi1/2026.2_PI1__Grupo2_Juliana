// Atalhos para o site (Dashboard.jsx). Os nomes de telas e cards seguem o roteiro 4.4.6.
import { expect, type Page } from '@playwright/test'

export type Aba = 'Dashboard' | 'Monitoramento' | 'Histórico' | 'Logs'

/**
 * Abre o site já com o labirinto escolhido e espera o painel conectar no backend.
 * Só depois disso vale mandar pacotes: o que o backend transmitir antes, o painel perde.
 */
export async function abrirPainel(page: Page, aba: Aba = 'Monitoramento', labirinto = '4x4') {
  await page.addInitScript((tipo) => window.localStorage.setItem('micromouse_tipo_labirinto', tipo), labirinto)
  await page.goto('/')
  await irPara(page, aba)
  // Sem nenhum pacote ainda, "Conectado" significa que o WebSocket do painel abriu.
  await expect(page.getByTestId('horario-atualizacao').locator('..')).toHaveClass(/online/)
}

export async function irPara(page: Page, aba: Aba) {
  await page.getByRole('button', { name: aba, exact: true }).click()
}

/** Ids das corridas na tabela "Execuções Registradas", na ordem da tela. */
export async function linhasDoHistorico(page: Page) {
  const testids = await page.getByTestId('tabela-execucoes-registradas').locator('tbody tr').evaluateAll((trs) => trs.map((tr) => tr.dataset.testid!))
  return testids.map((t) => Number(t.replace('linha-corrida-', '')))
}

/** Célula do "Mapa do Labirinto" (SVG do MazeMap). */
export function celulaDoMapa(page: Page, x: number, y: number) {
  return page.locator(`rect[data-celula="${x},${y}"]`)
}
