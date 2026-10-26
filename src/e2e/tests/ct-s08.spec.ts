// CT-S08 - Persistência de tentativa não concluída (RF07-S / HU #90, CA1; RF10-S / HU #93, CA1)
import { criarCorrida, encerrarCorrida } from '../helpers/api'
import { consultar } from '../helpers/banco'
import { capturarTela, csv, salvarEvidencia } from '../helpers/evidencia'
import { abrirPainel } from '../helpers/painel'
import { RoboFalso, TRAJETO_CURTO } from '../helpers/robo'
import { expect, test } from '../helpers/teste'

// O roteiro aceita os dois jeitos de interromper: o robô avisa o fim (retirado, tempo esgotado)
// ou o painel encerra a corrida (comando de abortar) e o robô nem fica sabendo.
const MODOS = {
  'robo-retirado': async (robo: RoboFalso, _idCorrida: number) => {
    await robo.fim('abandonada', 'manual')
  },
  'comando-abortar': async (robo: RoboFalso, idCorrida: number) => {
    await encerrarCorrida(idCorrida)
    await robo.tel({ pos_x: 3, pos_y: 3 }) // o robô segue mandando; o backend dá ack e ignora
  },
}

for (const [modo, interromper] of Object.entries(MODOS)) {
  test(`CT-S08 - tentativa interrompida (${modo}) é gravada como Não concluído e conta nas tentativas`, async ({ page }, testInfo) => {
    const idCorrida = await criarCorrida('4x4')
    const robo = await RoboFalso.conectar()
    await robo.inicio()
    // 1-2. Interrompe na metade do trajeto, antes do objetivo
    const parcial = TRAJETO_CURTO.slice(0, 3)
    for (const leitura of parcial) await robo.tel(leitura)
    await interromper(robo, idCorrida)
    robo.fechar()

    const [corrida] = await consultar('SELECT id, status, num_tentativa, tempo_conc_s FROM corrida WHERE id = $1', [idCorrida])
    const leituras = await consultar('SELECT tempo_ms, pos_x, pos_y FROM leitura_telemetria WHERE id_corrida = $1 ORDER BY tempo_ms', [idCorrida])
    await salvarEvidencia(testInfo, `${modo}_consulta-banco`, csv([corrida]) + '\n' + csv(leituras), 'csv')
    expect(corrida).toMatchObject({ status: 'abandonada', num_tentativa: 1, tempo_conc_s: null })
    expect(leituras.map((l) => [l.pos_x, l.pos_y])).toEqual(parcial.map((l) => [l.pos_x, l.pos_y]))

    // 3. Histórico: "Não concluído", tempo até a interrupção (última leitura em 600 ms) e conta como tentativa
    await abrirPainel(page, 'Histórico')
    const celulas = page.getByTestId(`linha-corrida-${idCorrida}`).locator('td')
    await expect(celulas.nth(2)).toHaveText('Tentativa 1')
    await expect(celulas.nth(3)).toHaveText('00:00 (0,60 s)')
    await expect(celulas.nth(6)).toHaveText('Não concluído')
    await expect(page.getByTestId('chip-labirinto-4x4')).toHaveText('4 × 4: 1')
    await expect(page.getByTestId('valor-corridas-abandonadas')).toHaveText('1')
    await capturarTela(page, testInfo, `${modo}_historico`)

    // Trajeto parcial em Detalhes da Corrida
    await page.getByTestId(`botao-detalhes-corrida-${idCorrida}`).click()
    await expect(page.getByTestId('detalhe-resultado')).toHaveText('Não concluído')
    await expect(page.getByTestId('detalhe-tempo-conclusao')).toHaveText('00:00 (0,60 s)')
    await expect(page.getByTestId('espaco-mapa-detalhe-corrida')).toHaveAttribute(
      'data-trajeto',
      parcial.map((l) => `${l.pos_x},${l.pos_y}`).join(';'),
    )
    await capturarTela(page, testInfo, `${modo}_detalhes-corrida`)
  })
}
