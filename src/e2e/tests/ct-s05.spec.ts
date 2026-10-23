// CT-S05 - Alerta de bateria em nível crítico (RF19-S / HU #102, CA1)
// Limiar crítico 7,0 V (3,5 V por célula); o alerta só sai acima de 7,2 V (rearme do contrato 4.4.7).
import { criarCorrida } from '../helpers/api'
import { capturarTela, csv, salvarEvidencia } from '../helpers/evidencia'
import { abrirPainel } from '../helpers/painel'
import { RoboFalso } from '../helpers/robo'
import { expect, test } from '../helpers/teste'

test('CT-S05 - card Bateria da Visão Geral alerta abaixo de 7,0 V e volta ao normal', async ({ page }, testInfo) => {
  await criarCorrida('4x4')
  await abrirPainel(page, 'Dashboard')
  const cardBateria = page.getByTestId('card-bateria-visao-geral')
  const robo = await RoboFalso.conectar()
  await robo.inicio()
  const enviadas: Record<string, unknown>[] = []

  const passos = [
    { passo: 1, tensao: 8.4, texto: 'Nível normal' },
    { passo: 2, tensao: 6.4, texto: 'Bateria crítica' },
    { passo: 3, tensao: 8.4, texto: 'Nível normal' },
  ]
  for (const { passo, tensao, texto } of passos) {
    const pacote = await robo.tel({ tensao_bateria: tensao })
    enviadas.push({ passo, tempo_ms: pacote.tempo_ms, tensao_bateria: tensao })
    // "Aparece na hora": mesmo limite de 500 ms do tempo real (RNF06-S)
    await expect(cardBateria).toContainText(texto, { timeout: 500 })
    // Visível sem rolar a tela
    await expect(cardBateria.getByText(texto)).toBeInViewport()
    await capturarTela(page, testInfo, `passo${passo}-${tensao.toFixed(1).replace('.', 'v')}`)
  }
  robo.fechar()
  await salvarEvidencia(testInfo, 'tensoes-enviadas', csv(enviadas), 'csv')

  // No passo 3 o alerta sumiu de vez
  await expect(cardBateria).not.toContainText('Bateria crítica')
})
