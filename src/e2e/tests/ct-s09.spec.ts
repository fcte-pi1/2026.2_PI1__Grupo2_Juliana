// CT-S09 - Controle de tentativas por labirinto (RF10-S / HU #93, CA1 e CA2)
import { consultar } from '../helpers/banco'
import { capturarTela, csv, salvarEvidencia } from '../helpers/evidencia'
import { abrirPainel } from '../helpers/painel'
import { massaDoCtS09 } from '../helpers/robo'
import { expect, test } from '../helpers/teste'

test('CT-S09 - tentativas são numeradas e contadas separadamente em cada labirinto', async ({ page }, testInfo) => {
  // 1-3. Duas no 4x4, uma no 8x4 e a terceira no 4x4
  const ids = await massaDoCtS09()

  // 4. Total por labirinto, inclusive o 12x4 sem tentativas
  const totais = await consultar<{ labirinto: string; total: string }>(
    `SELECT l.nome AS labirinto, COUNT(c.id) AS total
     FROM labirinto l LEFT JOIN corrida c ON c.id_labirinto = l.id
     GROUP BY l.id ORDER BY l.id`,
  )
  await salvarEvidencia(testInfo, 'tentativas-por-labirinto', csv(totais), 'csv')
  expect(totais.map((t) => [t.labirinto, Number(t.total)])).toEqual([
    ['4x4', 3],
    ['8x4', 1],
    ['12x4', 0],
  ])

  // O contador sobe a cada tentativa gravada, por labirinto
  const numeros = await consultar('SELECT id, num_tentativa FROM corrida ORDER BY id')
  const tentativaDe = new Map(numeros.map((c) => [c.id, c.num_tentativa]))
  expect(ids['4x4'].map((id) => tentativaDe.get(id))).toEqual([1, 2, 3])
  expect(ids['8x4'].map((id) => tentativaDe.get(id))).toEqual([1])

  // Tela: coluna "Tentativa" e o card "Tentativas por Labirinto"
  await abrirPainel(page, 'Histórico')
  for (const [i, id] of ids['4x4'].entries()) {
    await expect(page.getByTestId(`linha-corrida-${id}`).locator('td').nth(2)).toHaveText(`Tentativa ${i + 1}`)
  }
  await expect(page.getByTestId(`linha-corrida-${ids['8x4'][0]}`).locator('td').nth(2)).toHaveText('Tentativa 1')
  await expect(page.getByTestId('chip-labirinto-4x4')).toHaveText('4 × 4: 3')
  await expect(page.getByTestId('chip-labirinto-8x4')).toHaveText('8 × 4: 1')
  await expect(page.getByTestId('chip-labirinto-12x4')).toHaveText('12 × 4: 0')
  await capturarTela(page, testInfo, 'historico')
})
