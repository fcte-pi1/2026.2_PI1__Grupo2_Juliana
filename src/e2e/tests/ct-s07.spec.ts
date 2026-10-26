// CT-S07 - Persistência dos dados finais (RF07-S / HU #90, CA1 e CA2)
import { criarCorrida } from '../helpers/api'
import { consultar } from '../helpers/banco'
import { capturarTela, csv, salvarEvidencia } from '../helpers/evidencia'
import { abrirPainel, irPara } from '../helpers/painel'
import { RoboFalso, TRAJETO_CURTO } from '../helpers/robo'
import { expect, test } from '../helpers/teste'

test('CT-S07 - corrida concluída é gravada e aparece completa no Histórico e em Detalhes da Corrida', async ({ page }, testInfo) => {
  const idCorrida = await criarCorrida('4x4')
  await abrirPainel(page, 'Monitoramento')

  // 1-2. Corrida completa até o fim
  const robo = await RoboFalso.conectar()
  await robo.inicio()
  for (const leitura of TRAJETO_CURTO) await robo.tel(leitura)

  // O que foi visto em tempo real, para comparar com o histórico. Confere antes do fim:
  // depois dele o painel troca a posição por "Saída (...)" e o texto deixa de ser estável
  const ultima = TRAJETO_CURTO.at(-1)!
  await expect(page.getByTestId('celula-atual-badge')).toContainText(`(${ultima.pos_x}, ${ultima.pos_y})`)
  const velocidadeAoVivo = await page.getByTestId('velocidade-media').textContent()
  const fim = await robo.fim('concluida')
  robo.fechar()
  await irPara(page, 'Dashboard')
  const ultimaExecucao = page.getByTestId('painel-ultima-execucao')
  await expect(page.getByTestId('resultado-ultima-execucao')).toContainText('Concluído')
  await expect(ultimaExecucao.getByText('VELOCIDADE MÉDIA').locator('..')).toContainText(velocidadeAoVivo!)
  await expect(ultimaExecucao.getByText('TEMPO', { exact: true }).locator('..')).toContainText('0,80 s')
  await capturarTela(page, testInfo, 'ultima-execucao')

  // 3. Banco: corrida concluída com os totais e o trajeto inteiro
  const [corrida] = await consultar(
    'SELECT id, status, num_tentativa, tempo_conc_s, consumo_medio_ma, iniciado_em FROM corrida WHERE id = $1',
    [idCorrida],
  )
  const leituras = await consultar('SELECT tempo_ms, pos_x, pos_y, paredes, velocidade FROM leitura_telemetria WHERE id_corrida = $1 ORDER BY tempo_ms', [
    idCorrida,
  ])
  await salvarEvidencia(testInfo, 'consulta-banco', csv([corrida]) + '\n' + csv(leituras), 'csv')
  expect(corrida).toMatchObject({ status: 'concluida', num_tentativa: 1 })
  expect(Number(corrida.tempo_conc_s)).toBe(fim.tempo_conc_s)
  expect(Number(corrida.consumo_medio_ma)).toBe(fim.consumo_medio_ma)
  expect(leituras.map((l) => [l.pos_x, l.pos_y])).toEqual(TRAJETO_CURTO.map((l) => [l.pos_x, l.pos_y]))

  // 4. Histórico de Corridas, na mesma página que acompanhou a corrida
  await irPara(page, 'Histórico')
  await expect(page.getByTestId('connection-alert')).toHaveCount(0)
  const linha = page.getByTestId(`linha-corrida-${idCorrida}`)
  const celulas = linha.locator('td')
  await expect(celulas.nth(0)).toHaveText(/^\d{2}\/\d{2}\/\d{4} \d{2}:\d{2}$/)
  await expect(celulas.nth(1)).toHaveText('⊞4 × 4')
  await expect(celulas.nth(2)).toHaveText('Tentativa 1')
  await expect(celulas.nth(3)).toHaveText('00:00 (0,80 s)')
  await expect(celulas.nth(4)).toHaveText(velocidadeAoVivo!)
  await expect(celulas.nth(5)).toHaveText('400,0 mA')
  await expect(celulas.nth(6)).toHaveText('Concluído')
  await capturarTela(page, testInfo, 'historico')

  await page.getByTestId(`botao-detalhes-corrida-${idCorrida}`).click()
  await expect(page.getByTestId('tela-detalhes-corrida')).toBeVisible()
  await expect(page.getByTestId('detalhe-resultado')).toHaveText('Concluído')
  await expect(page.getByTestId('detalhe-tempo-conclusao')).toHaveText('00:00 (0,80 s)')
  await expect(page.getByTestId('detalhe-velocidade-media')).toHaveText(velocidadeAoVivo!)
  await expect(page.getByTestId('detalhe-total-leituras')).toHaveText(String(TRAJETO_CURTO.length))
  await expect(page.getByTestId('espaco-mapa-detalhe-corrida')).toHaveAttribute(
    'data-trajeto',
    TRAJETO_CURTO.map((l) => `${l.pos_x},${l.pos_y}`).join(';'),
  )
  await expect(page.locator('path[data-trajeto]')).toBeVisible()
  await capturarTela(page, testInfo, 'detalhes-corrida')
})
