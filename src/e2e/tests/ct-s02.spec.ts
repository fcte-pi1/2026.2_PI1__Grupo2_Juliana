// CT-S02 - Aceitação de pacote de telemetria válido (RF24-S / HU #107; RF05-S / HU #88, CA2)
import { consultar, criarCorrida } from '../helpers/banco'
import { capturarTela, csv, salvarEvidencia } from '../helpers/evidencia'
import { abrirPainel } from '../helpers/painel'
import { RoboFalso } from '../helpers/robo'
import { expect, test } from '../helpers/teste'

test('CT-S02 - pacote válido é aceito, gravado na corrida certa e exibido na Telemetria', async ({ page }, testInfo) => {
  const idCorrida = await criarCorrida('4x4')
  await abrirPainel(page, 'Monitoramento')
  const robo = await RoboFalso.conectar()

  await robo.inicio()
  // enviar() só termina quando chega o ack: o pacote passou na validação
  const pacote = await robo.tel({ pos_x: 1, pos_y: 2, orientacao: 'L', paredes: 9, velocidade: 0.25, tensao_bateria: 8.12, valor_flood: 3 })
  robo.fechar()

  // Banco: gravado e associado à corrida correta
  const leituras = await consultar(
    'SELECT id_corrida, tempo_ms, pos_x, pos_y, orientacao, paredes, velocidade, tensao_bateria, valor_flood FROM leitura_telemetria',
  )
  await salvarEvidencia(testInfo, 'consulta-banco', csv(leituras), 'csv')
  expect(leituras).toEqual([
    {
      id_corrida: idCorrida,
      tempo_ms: pacote.tempo_ms,
      pos_x: 1,
      pos_y: 2,
      orientacao: 'L',
      paredes: 9,
      velocidade: '0.250',
      tensao_bateria: '8.12',
      valor_flood: 3,
    },
  ])

  // Tela: painel "Telemetria" com os valores enviados
  await expect(page.getByTestId('tensao-bateria')).toHaveText('8,12 V')
  await expect(page.getByTestId('velocidade-atual')).toHaveText('0,25 m/s')
  await expect(page.getByTestId('celula-atual-badge')).toHaveText('B3 (1, 2)')
  await expect(page.getByTestId('orientacao-atual')).toHaveText('Leste →')
  await expect(page.getByTestId('texto-status-execucao')).toHaveText('Explorando labirinto')
  await capturarTela(page, testInfo, 'monitoramento')

  // Nenhum evento de nível "Erro" registrado (tela Logs e Eventos ainda não existe: conferido no banco)
  const erros = await consultar("SELECT mensagem FROM log_evento WHERE tipo = 'erro'")
  expect(erros).toEqual([])
})
