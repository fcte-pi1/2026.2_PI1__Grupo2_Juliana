// CT-S12 - Consulta geral de resultados (RF09-S / HU #92, CA1 e CA2)
import { criarCorrida } from '../helpers/api'
import { consultar } from '../helpers/banco'
import { capturarTela, csv, salvarEvidencia } from '../helpers/evidencia'
import { abrirPainel, linhasDoHistorico } from '../helpers/painel'
import { massaDoCtS09 } from '../helpers/robo'
import { rodarSimulador } from '../helpers/simulador'
import { expect, test } from '../helpers/teste'

test('CT-S12 - com Labirinto em Todos aparecem as corridas de todos os labirintos, e uma nova entra na lista', async ({ page }, testInfo) => {
  const ids = await massaDoCtS09()

  // 1-2. Histórico com "Todos": total igual à soma das consultas por labirinto
  await abrirPainel(page, 'Histórico')
  await expect(page.getByLabel('Labirinto', { exact: true })).toHaveValue('')
  const porLabirinto = await consultar<{ labirinto: string; total: string }>(
    'SELECT l.nome AS labirinto, COUNT(*) AS total FROM corrida c JOIN labirinto l ON l.id = c.id_labirinto GROUP BY l.nome ORDER BY l.nome',
  )
  const soma = porLabirinto.reduce((s, l) => s + Number(l.total), 0)
  await expect(page.getByTestId('contador-corridas-encontradas')).toHaveText(`${soma} corridas encontradas`)
  expect((await linhasDoHistorico(page)).sort()).toEqual([...ids['4x4'], ...ids['8x4']].sort())
  await expect(page.getByTestId(`linha-corrida-${ids['8x4'][0]}`).locator('td').nth(1)).toHaveText('⊞8 × 4')
  await expect(page.getByTestId(`linha-corrida-${ids['4x4'][0]}`).locator('td').nth(1)).toHaveText('⊞4 × 4')
  await capturarTela(page, testInfo, 'lista-antes')

  // 3. Nova corrida pelo simulador e "Atualizar"
  const nova = await criarCorrida('4x4')
  const sim = await rodarSimulador(['--semente', '1'])
  expect(sim.falhas).toEqual([])
  await page.getByTestId('botao-atualizar-historico').click()
  await expect(page.getByTestId('contador-corridas-encontradas')).toHaveText(`${soma + 1} corridas encontradas`)
  await expect(page.getByTestId(`linha-corrida-${nova}`)).toContainText('4 × 4')
  await capturarTela(page, testInfo, 'lista-depois')

  const total = await consultar('SELECT COUNT(*)::int AS total_corridas FROM corrida')
  await salvarEvidencia(testInfo, 'consulta-banco', csv(porLabirinto) + '\n' + csv(total), 'csv')
  expect(total[0].total_corridas).toBe(soma + 1)
})
