// CT-S13 - Exportação de relatórios (RF18-S / HU #101, CA1 e CA2)
import type { Locator, Page } from '@playwright/test'
import { readFileSync } from 'node:fs'
import { capturarTela, salvarEvidencia } from '../helpers/evidencia'
import { abrirPainel, linhasDoHistorico } from '../helpers/painel'
import { massaDoCtS09 } from '../helpers/robo'
import { expect, test } from '../helpers/teste'

const CABECALHO =
  'id_corrida,labirinto,num_tentativa,num_corrida_robo,status,iniciado_em,tempo_conc_s,consumo_medio_ma,tempo_ms,recebido_em,pos_x,pos_y,orientacao,paredes,velocidade,tensao_bateria,valor_flood'

/** Clica no botão de exportar CSV e devolve as linhas do arquivo baixado como objetos. */
async function baixarCsv(page: Page, botao: Locator) {
  const [download] = await Promise.all([page.waitForEvent('download'), botao.click()])
  const texto = readFileSync(await download.path(), 'utf8')
  const [cabecalho, ...linhas] = texto.trimEnd().split('\n')
  const colunas = cabecalho.split(',')
  return {
    texto,
    nome: download.suggestedFilename(),
    cabecalho,
    linhas: linhas.map((l) => Object.fromEntries(l.split(',').map((v, i) => [colunas[i], v]))),
  }
}

/** "Exportar PDF" chama a impressão do navegador; conta as chamadas e gera o PDF da tela como o usuário veria. */
async function exportarPdf(page: Page, botao: Locator) {
  const antes = await page.evaluate(() => (window as any).impressoes)
  await botao.click()
  await expect.poll(() => page.evaluate(() => (window as any).impressoes)).toBe(antes + 1)
  await page.emulateMedia({ media: 'print' })
  const pdf = await page.pdf({ format: 'A4', landscape: true, printBackground: true })
  await page.emulateMedia({ media: 'screen' })
  expect(pdf.subarray(0, 5).toString()).toBe('%PDF-')
  return pdf
}

test('CT-S13 - CSV e PDF do histórico e da corrida aberta contêm só o que está filtrado', async ({ page }, testInfo) => {
  const ids = await massaDoCtS09()
  await page.addInitScript(() => {
    ;(window as any).impressoes = 0
    window.print = () => (window as any).impressoes++
  })

  // 1. Filtro "Labirinto" em 4 x 4
  await abrirPainel(page, 'Histórico')
  await page.getByLabel('Labirinto', { exact: true }).selectOption({ label: '4 × 4 (4x4)' })
  await expect(page.getByTestId('contador-corridas-encontradas')).toHaveText('3 corridas encontradas')
  await capturarTela(page, testInfo, 'filtro-exportacao')

  // 2. "Exportar CSV": cabeçalho, só as corridas do filtro e os mesmos valores da tela
  const geral = await baixarCsv(page, page.getByTestId('botao-exportar-csv'))
  await salvarEvidencia(testInfo, 'historico', geral.texto, 'csv')
  expect(geral.nome).toBe('corridas.csv')
  expect(geral.cabecalho).toBe(CABECALHO)
  expect(geral.texto).not.toContain('�') // nenhum caractere perdido na decodificação UTF-8
  expect([...new Set(geral.linhas.map((l) => Number(l.id_corrida)))].sort()).toEqual([...ids['4x4']].sort())
  expect(new Set(geral.linhas.map((l) => l.labirinto))).toEqual(new Set(['4x4']))
  for (const id of ids['4x4']) {
    const doArquivo = geral.linhas.filter((l) => Number(l.id_corrida) === id)
    const celulas = page.getByTestId(`linha-corrida-${id}`).locator('td')
    await expect(celulas.nth(2)).toHaveText(`Tentativa ${doArquivo[0].num_tentativa}`)
    await expect(celulas.nth(5)).toHaveText(`${Number(doArquivo[0].consumo_medio_ma).toFixed(1).replace('.', ',')} mA`)
    expect(doArquivo[0].status).toBe('concluida')
  }

  // 3. "Exportar PDF": a impressão leva só as linhas do filtro
  await salvarEvidencia(testInfo, 'historico', await exportarPdf(page, page.getByTestId('botao-exportar-pdf')), 'pdf')
  expect(await linhasDoHistorico(page)).toEqual([...ids['4x4']].reverse())

  // Detalhes da Corrida: os dois botões exportam só a corrida aberta
  const aberta = ids['4x4'][1]
  await page.getByTestId(`botao-detalhes-corrida-${aberta}`).click()
  await expect(page.getByTestId('detalhe-id-corrida')).toHaveText(`#${String(aberta).padStart(3, '0')}`)
  const detalhe = await baixarCsv(page, page.getByTestId('botao-exportar-csv-detalhe'))
  await salvarEvidencia(testInfo, 'detalhes-corrida', detalhe.texto, 'csv')
  expect(detalhe.cabecalho).toBe(CABECALHO)
  expect(new Set(detalhe.linhas.map((l) => Number(l.id_corrida)))).toEqual(new Set([aberta]))
  await expect(page.getByTestId('detalhe-total-leituras')).toHaveText(String(detalhe.linhas.length))
  await salvarEvidencia(testInfo, 'detalhes-corrida', await exportarPdf(page, page.getByTestId('botao-exportar-pdf-detalhe')), 'pdf')
})
