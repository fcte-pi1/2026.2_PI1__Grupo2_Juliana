// CT-S10 - Consulta por labirinto (RF08-S / HU #91, CA1)
import { consultar } from '../helpers/banco'
import { capturarTela, csv, salvarEvidencia } from '../helpers/evidencia'
import { abrirPainel, linhasDoHistorico } from '../helpers/painel'
import { massaDoCtS09 } from '../helpers/robo'
import { expect, test } from '../helpers/teste'

test('CT-S10 - o filtro Labirinto mostra só as corridas do labirinto escolhido, sem recarregar a página', async ({ page }, testInfo) => {
  const ids = await massaDoCtS09()
  const referencia = await consultar(
    `SELECT c.id, l.nome AS labirinto, c.num_tentativa, c.status, c.tempo_conc_s, c.consumo_medio_ma
     FROM corrida c JOIN labirinto l ON l.id = c.id_labirinto ORDER BY c.id`,
  )
  await salvarEvidencia(testInfo, 'consulta-banco', csv(referencia), 'csv')

  await abrirPainel(page, 'Histórico')
  // Marca a página: se o filtro recarregar o site, a marca some
  await page.evaluate(() => ((window as any).semRecarregar = true))

  for (const [labirinto, rotulo] of [
    ['4x4', '4 × 4 (4x4)'],
    ['8x4', '8 × 4 (8x4)'],
  ] as const) {
    // 2-3. Filtro "Labirinto"
    await page.getByLabel('Labirinto', { exact: true }).selectOption({ label: rotulo })
    const esperados = ids[labirinto]
    await expect(page.getByTestId('contador-corridas-encontradas')).toHaveText(`${esperados.length} corridas encontradas`)
    // Mais recente em cima; cada linha igual ao que foi gravado
    expect(await linhasDoHistorico(page)).toEqual([...esperados].reverse())
    for (const id of esperados) {
      const gravada = referencia.find((c) => c.id === id)!
      const celulas = page.getByTestId(`linha-corrida-${id}`).locator('td')
      await expect(celulas.nth(1)).toHaveText(`⊞${rotulo.split(' (')[0]}`)
      await expect(celulas.nth(2)).toHaveText(`Tentativa ${gravada.num_tentativa}`)
      await expect(celulas.nth(5)).toHaveText(`${Number(gravada.consumo_medio_ma).toFixed(1).replace('.', ',')} mA`)
      await expect(celulas.nth(6)).toHaveText('Concluído')
    }
    await capturarTela(page, testInfo, `filtro-${labirinto}`)
  }

  expect(await page.evaluate(() => (window as any).semRecarregar)).toBe(true)
})
