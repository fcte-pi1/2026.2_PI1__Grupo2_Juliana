// CT-S14 - Log de eventos e erros (RF20-S / HU #103, CA1 e CA2)
import { BACKEND_URL } from '../helpers/ambiente'
import { criarCorrida } from '../helpers/api'
import { consultar } from '../helpers/banco'
import { capturarTela, salvarEvidencia } from '../helpers/evidencia'
import { abrirPainel } from '../helpers/painel'
import { quadro, RoboFalso } from '../helpers/robo'
import { expect, test } from '../helpers/teste'

// Data da sessão no fuso que o painel usa para o filtro "Data da ocorrência"
const hoje = () => new Date().toLocaleDateString('sv-SE', { timeZone: 'America/Sao_Paulo' })

// Eventos da sessão em ordem cronológica: [Nível, Evento]
const EVENTOS: [string, RegExp][] = [
  ['Informação', /^evento inicio/],
  ['Erro', /^checksum invalido/],
  ['Aviso', /^desconexao/],
  ['Aviso', /^evento reconexao/],
  ['Informação', /^evento fim: resultado=concluida/],
]

test('CT-S14 - eventos da sessão aparecem por período, filtram por tipo e origem, e Limpar filtros volta à lista', async ({ page }, testInfo) => {
  // Um evento de outro dia, para o filtro de data ter o que tirar
  await consultar(
    "INSERT INTO log_evento (tipo, origem, mensagem, ocorrido_em) VALUES ('info', 'backend', 'evento de outra sessão', now() - interval '3 days')",
  )

  // 1-2. Corrida com um pacote corrompido e uma perda de conexão no meio
  const idCorrida = await criarCorrida('4x4')
  const robo = await RoboFalso.conectar()
  await robo.inicio()
  await robo.tel({ pos_x: 0, pos_y: 0 })
  const corrompido = robo.montarTel({ pos_x: 1 })
  robo.enviarBruto(quadro(corrompido).slice(0, -4) + '0000')
  await robo.tel({ pos_x: 0, pos_y: 1 }) // ack deste: o corrompido já foi registrado
  robo.fechar()
  await expect.poll(() => consultar("SELECT 1 FROM log_evento WHERE mensagem LIKE 'desconexao%'")).toHaveLength(1)
  await robo.reconectar()
  await robo.reconexao(1500)
  await robo.tel({ pos_x: 1, pos_y: 1 })
  await robo.fim('concluida')
  robo.fechar()

  // 3. Logs com a data da sessão
  await abrirPainel(page, 'Logs')
  const linhas = page.getByTestId('tabela-registro-eventos').locator('tbody tr')
  await expect(linhas).toHaveCount(EVENTOS.length + 1)
  await page.getByLabel('Data da ocorrência').fill(hoje())
  await expect(linhas).toHaveCount(EVENTOS.length)

  // Só os eventos da sessão, do mais recente ao mais antigo, com todas as colunas preenchidas
  const sessao = [...EVENTOS].reverse()
  for (const [i, [nivel, evento]] of sessao.entries()) {
    const celulas = linhas.nth(i).locator('td')
    await expect(celulas.nth(0)).toHaveText(/^\d{2}\/\d{2}\/\d{4} \d{2}:\d{2}:\d{2}$/)
    await expect(celulas.nth(1)).toHaveText(nivel)
    await expect(celulas.nth(2)).toHaveText('Backend')
    await expect(celulas.nth(3)).toHaveText(evento)
    await expect(celulas.nth(4)).toHaveText(`#${String(idCorrida).padStart(3, '0')}`)
  }
  await capturarTela(page, testInfo, 'registro-eventos-data')

  // 4. "Tipo de evento": cada nível mostra só os seus
  const tipo = page.getByLabel('Tipo de evento')
  for (const nivel of ['Informação', 'Aviso', 'Erro']) {
    await tipo.selectOption({ label: nivel })
    await expect(linhas).toHaveCount(EVENTOS.filter(([n]) => n === nivel).length)
    for (const celula of await linhas.locator('td:nth-child(2)').all()) await expect(celula).toHaveText(nivel)
  }
  await tipo.selectOption({ label: 'Todos' })

  // "Origem": todo evento da sessão é gravado pelo backend; Firmware e Frontend ficam vazios
  const origem = page.getByLabel('Origem')
  await origem.selectOption({ label: 'Backend' })
  await expect(linhas).toHaveCount(EVENTOS.length)
  for (const outra of ['Firmware', 'Frontend']) {
    await origem.selectOption({ label: outra })
    await expect(page.getByTestId('estado-logs-vazio')).toBeVisible()
  }

  // "Limpar filtros" volta à lista completa, inclusive o evento de outro dia
  await page.getByTestId('botao-limpar-filtros-logs').click()
  await expect(page.getByLabel('Data da ocorrência')).toHaveValue('')
  await expect(linhas).toHaveCount(EVENTOS.length + 1)

  // 5. Log do servidor do mesmo período (a mesma exportação do botão "Exportar CSV" da tela)
  const logServidor = await (await fetch(`${BACKEND_URL}/exportacao/logs.csv?data=${hoje()}`)).text()
  await salvarEvidencia(testInfo, 'log-servidor', logServidor, 'txt')
  expect(logServidor.trimEnd().split('\n')).toHaveLength(EVENTOS.length + 1) // cabeçalho + eventos do dia
})
