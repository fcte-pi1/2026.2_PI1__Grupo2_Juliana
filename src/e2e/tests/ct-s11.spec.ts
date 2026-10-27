// CT-S11 - Consulta de labirinto sem registros (RF08-S / HU #91, CA2)
import { capturarTela, salvarEvidencia } from '../helpers/evidencia'
import { abrirPainel } from '../helpers/painel'
import { correrComRoboFalso } from '../helpers/robo'
import { expect, test } from '../helpers/teste'

test('CT-S11 - filtrar o 12x4 sem corridas mostra 0 encontradas e a mensagem de vazio, sem erro no console', async ({ page }, testInfo) => {
  // Uma corrida em outro labirinto, para o filtro ter o que tirar da tela
  await correrComRoboFalso('4x4')

  const console: string[] = []
  page.on('console', (m) => console.push(`[${m.type()}] ${m.text()}`))
  page.on('pageerror', (e) => console.push(`[pageerror] ${e.message}`))

  // 1-2. Histórico com o filtro "Labirinto" em 12 x 4
  await abrirPainel(page, 'Histórico')
  await expect(page.getByTestId('contador-corridas-encontradas')).toHaveText('1 corridas encontradas')
  await page.getByLabel('Labirinto', { exact: true }).selectOption({ label: '12 × 4 (12x4)' })
  await expect(page.getByTestId('contador-corridas-encontradas')).toHaveText('0 corridas encontradas')
  await expect(page.getByTestId('estado-labirinto-sem-registros')).toContainText('Nenhuma corrida registrada para este labirinto.')
  await expect(page.getByTestId('tabela-execucoes-registradas')).toHaveCount(0)
  await expect(page.getByTestId('erro-historico')).toHaveCount(0)

  // 3. Evidências: a mensagem e o console do navegador
  await capturarTela(page, testInfo, 'labirinto-sem-registros')
  await salvarEvidencia(testInfo, 'console-navegador', console.join('\n') || '(console vazio)', 'txt')
  expect(console.filter((l) => l.startsWith('[error]') || l.startsWith('[pageerror]'))).toEqual([])
})
