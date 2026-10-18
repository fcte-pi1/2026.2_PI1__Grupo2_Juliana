// Fumaça da infraestrutura E2E: banco, backend, site e simulador conversam entre si.
// Os casos CT-S ficam em arquivos próprios (ct-s01.spec.ts, ...).
import { BACKEND_URL } from '../helpers/ambiente'
import { consultar, criarCorrida } from '../helpers/banco'
import { rodarSimulador } from '../helpers/simulador'
import { expect, test } from '../helpers/teste'

test('backend responde no /health', async ({ request }) => {
  const resposta = await request.get(`${BACKEND_URL}/health`)
  expect(resposta.ok()).toBeTruthy()
  expect(await resposta.json()).toEqual({ status: 'ok' })
})

test('site abre na tela de monitoramento', async ({ page }) => {
  await page.goto('/')
  await expect(page.getByTestId('card-conexao')).toBeVisible()
})

test('simulador envia uma corrida completa e o backend grava', async () => {
  const idCorrida = await criarCorrida('4x4')

  // Velocidade real (5 Hz). Acelerar o simulador (--intervalo 0.05) passa da vazão do backend:
  // os acks atrasam, os reenvios chegam depois do `fim` e são descartados como "corrida desconhecida".
  const sim = await rodarSimulador(['--semente', '1'])

  expect(sim.codigo, sim.saida).toBe(0)
  expect(sim.falhas).toEqual([])
  expect(sim.acks, 'toda mensagem recebe ack').toBe(sim.mensagens)
  const [corrida] = await consultar('SELECT status FROM corrida WHERE id = $1', [idCorrida])
  expect(corrida.status).toBe('concluida')
  const [{ total }] = await consultar('SELECT count(*)::int AS total FROM leitura_telemetria WHERE id_corrida = $1', [idCorrida])
  expect(total).toBeGreaterThan(0)
})
