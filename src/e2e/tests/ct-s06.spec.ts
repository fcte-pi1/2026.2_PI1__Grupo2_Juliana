// CT-S06 - Alerta de perda de conexão (RF19-S / HU #102, CA2)
import { criarCorrida } from '../helpers/api'
import { capturarTela, csv, salvarEvidencia } from '../helpers/evidencia'
import { abrirPainel, irPara } from '../helpers/painel'
import { RoboFalso } from '../helpers/robo'
import { expect, test } from '../helpers/teste'

const SEM_PACOTES_MS = 3000

test('CT-S06 - depois de 3 s sem pacotes mostra Offline e alerta; ao retomar volta a Conectado', async ({ page }, testInfo) => {
  await criarCorrida('4x4')
  await abrirPainel(page, 'Dashboard')
  const statusRobo = page.getByTestId('card-status-robo')
  const conexaoMicromouse = page.getByTestId('painel-status-sistema').getByRole('listitem').filter({ hasText: 'Conexão com o Micromouse' })
  const robo = await RoboFalso.conectar()
  await robo.inicio()

  // Recebendo telemetria
  for (let i = 0; i < 5; i++) await robo.tel({ tensao_bateria: 8.4 })
  await expect(statusRobo).toContainText('Conectado')
  await expect(conexaoMicromouse).toContainText('Online')

  // 1-2. Para o envio e aguarda mais de 3 s
  const ultimoPacoteEm = Date.now()
  await expect(statusRobo).toContainText('Offline', { timeout: SEM_PACOTES_MS + 2000 })
  const alertaEm = Date.now()
  await expect(statusRobo).toContainText('Conexão perdida')
  await expect(conexaoMicromouse).toContainText('Offline')
  await capturarTela(page, testInfo, 'visao-geral-offline')

  await irPara(page, 'Monitoramento')
  await expect(page.getByTestId('card-conexao')).toContainText('Offline')
  await expect(page.getByTestId('card-conexao')).toContainText('Alerta: Conexão perdida')
  await expect(page.getByTestId('lista-eventos-recentes')).toContainText('Conexão perdida — mais de 3 s sem telemetria')
  await capturarTela(page, testInfo, 'monitoramento-offline')

  // Só ficou Offline depois dos 3 s, e não muito depois (o painel confere a cada 200 ms)
  expect(alertaEm - ultimoPacoteEm).toBeGreaterThanOrEqual(SEM_PACOTES_MS)
  expect(alertaEm - ultimoPacoteEm).toBeLessThan(SEM_PACOTES_MS + 1000)

  // 3. Retoma o envio: volta a Conectado e a tela volta a atualizar
  const retomadaEm = Date.now()
  await robo.tel({ tensao_bateria: 8.25 })
  await expect(page.getByTestId('card-conexao')).toContainText('Conectado')
  await expect(page.getByTestId('tensao-bateria')).toHaveText('8,25 V')
  await capturarTela(page, testInfo, 'monitoramento-reconectado')
  await irPara(page, 'Dashboard')
  await expect(statusRobo).toContainText('Conectado')
  await expect(conexaoMicromouse).toContainText('Online')
  robo.fechar()

  await salvarEvidencia(
    testInfo,
    'tempos',
    csv([{ ultimo_pacote_ms: ultimoPacoteEm, alerta_offline_ms: alertaEm, segundos_ate_alerta: (alertaEm - ultimoPacoteEm) / 1000, retomada_ms: retomadaEm }]),
    'csv',
  )
})
