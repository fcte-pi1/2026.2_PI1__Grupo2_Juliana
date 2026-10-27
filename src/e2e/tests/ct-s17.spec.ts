// CT-S17 - Codificação das paredes no pacote de telemetria (RF05-S / HU #88, CA2; RF06-S / HU #89, CA1)
import { criarCorrida } from '../helpers/api'
import { consultar } from '../helpers/banco'
import { capturarTela, csv, salvarEvidencia } from '../helpers/evidencia'
import { abrirPainel } from '../helpers/painel'
import { RoboFalso } from '../helpers/robo'
import { expect, test } from '../helpers/teste'

// Contrato 4.4.7: Norte = 1, Leste = 2, Sul = 4, Oeste = 8
const BITS = { Norte: 1, Leste: 2, Sul: 4, Oeste: 8 } as const
const paredesDoValor = (valor: number) => Object.entries(BITS).filter(([, bit]) => valor & bit).map(([lado]) => lado).sort()

test('CT-S17 - os 16 valores de paredes são gravados sem alteração e desenhados nos lados certos', async ({ page }, testInfo) => {
  const idCorrida = await criarCorrida('4x4')
  await abrirPainel(page, 'Monitoramento')
  const robo = await RoboFalso.conectar()
  await robo.inicio()

  // 1. Um pacote por célula: (x, y) recebe paredes = 4·y + x
  for (let y = 0; y < 4; y++) for (let x = 0; x < 4; x++) await robo.tel({ pos_x: x, pos_y: y, paredes: 4 * y + x })
  robo.fechar()

  // 3. Gravados sem alteração
  const leituras = await consultar<{ pos_x: number; pos_y: number; paredes: number }>(
    'SELECT pos_x, pos_y, paredes FROM leitura_telemetria WHERE id_corrida = $1 ORDER BY tempo_ms',
    [idCorrida],
  )
  await salvarEvidencia(testInfo, 'consulta-banco', csv(leituras), 'csv')
  expect(leituras).toHaveLength(16)
  for (const l of leituras) expect(l.paredes, `célula (${l.pos_x}, ${l.pos_y})`).toBe(4 * l.pos_y + l.pos_x)

  // 2. No mapa, cada parede fica no lado da célula que o nome diz: confere pela posição desenhada,
  // não só pelo nome, para pegar Leste/Oeste ou Norte/Sul trocados no desenho.
  await expect(page.locator('line[data-parede]')).toHaveCount(32) // cada um dos 4 bits está ligado em 8 dos 16 valores
  const desenhadas = await page.evaluate(() => {
    const resultado: Record<string, string[]> = {}
    for (const grupo of document.querySelectorAll<SVGGElement>('g[data-paredes-celula]')) {
      const chave = grupo.dataset.paredesCelula!
      const celula = document.querySelector(`rect[data-celula="${chave}"]`)!.getBoundingClientRect()
      resultado[chave] = [...grupo.querySelectorAll('line')].map((linha) => {
        const r = linha.getBoundingClientRect()
        const [meioX, meioY] = [(r.left + r.right) / 2, (r.top + r.bottom) / 2]
        const perto = (a: number, b: number) => Math.abs(a - b) < 2
        const lado = perto(meioY, celula.top)
          ? 'Norte'
          : perto(meioY, celula.bottom)
            ? 'Sul'
            : perto(meioX, celula.right)
              ? 'Leste'
              : perto(meioX, celula.left)
                ? 'Oeste'
                : '?'
        return lado === linha.dataset.parede ? lado : `${linha.dataset.parede} desenhada em ${lado}`
      })
    }
    return resultado
  })

  const planilha = leituras.map(({ pos_x, pos_y, paredes }) => ({
    celula: `(${pos_x}, ${pos_y})`,
    valor_enviado: paredes,
    paredes_esperadas: paredesDoValor(paredes).join(' '),
    paredes_desenhadas: (desenhadas[`${pos_x},${pos_y}`] ?? []).sort().join(' '),
  }))
  await salvarEvidencia(testInfo, 'enviado-x-desenhado', csv(planilha), 'csv')
  await capturarTela(page, testInfo, 'mapa-16-celulas')
  for (const linha of planilha) expect(linha.paredes_desenhadas, `célula ${linha.celula}, valor ${linha.valor_enviado}`).toBe(linha.paredes_esperadas)
})
