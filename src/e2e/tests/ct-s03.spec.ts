// CT-S03 - Descarte de pacote de telemetria corrompido (RF24-S / HU #107; RF20-S / HU #103, CA1)
import { consultar, criarCorrida } from '../helpers/banco'
import { capturarTela, csv, salvarEvidencia } from '../helpers/evidencia'
import { abrirPainel, celulaDoMapa } from '../helpers/painel'
import { ouvirPainel, quadro, RoboFalso } from '../helpers/robo'
import { expect, test } from '../helpers/teste'

test('CT-S03 - pacotes inválidos são descartados, registrados como erro e o servidor segue aceitando', async ({ page }, testInfo) => {
  await criarCorrida('4x4')
  await abrirPainel(page, 'Monitoramento')
  const painel = await ouvirPainel()
  const robo = await RoboFalso.conectar()
  await robo.inicio()

  // 1. checksum inválido: muda pos_x depois de calcular o CRC (erro de transmissão)
  const p1 = robo.montarTel({ pos_x: 1 })
  const crcOriginal = quadro(p1).slice(-4)
  robo.enviarBruto(quadro({ ...p1, pos_x: 2 }).slice(0, -4) + crcOriginal)
  // 2. sem um campo obrigatório
  const { paredes: _, ...p2 } = robo.montarTel()
  robo.enviarBruto(quadro(p2))
  // 3. valores fora do intervalo: paredes = 16 e célula (99, 99) num labirinto 4x4
  const p3 = robo.montarTel({ paredes: 16 })
  robo.enviarBruto(quadro(p3))
  const p4 = robo.montarTel({ pos_x: 99, pos_y: 99 })
  robo.enviarBruto(quadro(p4))

  // O servidor continua funcionando e aceita o próximo pacote válido.
  // O backend trata as mensagens da conexão em ordem: chegou o ack deste, os anteriores já foram julgados.
  const valido = await robo.tel({ pos_x: 0, pos_y: 1, paredes: 5 })
  robo.fechar()
  // O backend manda o ack antes de retransmitir ao painel: espera a retransmissão chegar
  await expect.poll(() => painel.mensagens.some((m) => m.tempo_ms === valido.tempo_ms)).toBe(true)
  painel.fechar()

  // Nenhum dos inválidos recebeu ack
  for (const p of [p1, p2, p3, p4]) expect(robo.recebeuAck(p.seq), `seq ${p.seq} não pode ter ack`).toBe(false)

  // Nenhum foi gravado: só a leitura válida está no banco
  const leituras = await consultar('SELECT tempo_ms, pos_x, pos_y, paredes FROM leitura_telemetria')
  await salvarEvidencia(testInfo, 'consulta-banco', csv(leituras), 'csv')
  expect(leituras).toEqual([{ tempo_ms: valido.tempo_ms, pos_x: 0, pos_y: 1, paredes: 5 }])

  // Nenhum foi exibido: o backend não retransmitiu ao painel, e o mapa não mostra a célula (1, 0) do pacote 1
  const telsNoPainel = painel.mensagens.filter((m) => m.t === 'tel').map((m) => m.tempo_ms)
  expect(telsNoPainel).toEqual([valido.tempo_ms])
  await expect(celulaDoMapa(page, 0, 1)).toHaveClass(/observed/)
  await expect(celulaDoMapa(page, 1, 0)).not.toHaveClass(/observed/)
  await capturarTela(page, testInfo, 'monitoramento')

  // Cada descarte vira um evento de nível "erro", origem "backend", com data, hora e motivo
  const logs = await consultar<{ tipo: string; origem: string; mensagem: string; ocorrido_em: Date }>(
    "SELECT tipo, origem, mensagem, ocorrido_em FROM log_evento WHERE tipo = 'erro' ORDER BY id",
  )
  await salvarEvidencia(testInfo, 'log-eventos', csv(logs), 'csv')
  expect(logs.map((l) => l.mensagem)).toEqual([
    expect.stringMatching(/^checksum invalido/),
    'campo ausente: paredes',
    'fora do intervalo: paredes=16',
    'fora do intervalo: pos_x=99',
  ])
  for (const log of logs) {
    expect(log.origem).toBe('backend')
    expect(log.ocorrido_em).toBeInstanceOf(Date)
  }
})

// Passos 4 e 5 do roteiro: filtro "Erro" e painel "Detalhes do evento" na tela Logs e Eventos.
// A tela entra com a 3.8.7 (PR #245); até lá os descartes são conferidos no banco, no teste acima.
test.fixme('CT-S03 - descartes aparecem em Logs e Eventos com nível Erro, origem Backend e motivo', async () => {})
