import { StrictMode } from 'react'
import { act, cleanup, fireEvent, render, screen } from '@testing-library/react'
import { afterEach, beforeEach, describe, expect, test, vi } from 'vitest'
import App from './App'
import Dashboard from './Dashboard.jsx'

/**
 * Calcula CRC-16/CCITT-FALSE nos testes para montar quadros <JSON>|<CRC> idênticos ao simulador.py.
 */
function gerarQuadroCrc(objeto: Record<string, unknown>) {
  const json = JSON.stringify(objeto)
  const bytes = new TextEncoder().encode(json)
  let crc = 0xffff
  for (let i = 0; i < bytes.length; i += 1) {
    crc ^= bytes[i] << 8
    for (let b = 0; b < 8; b += 1) {
      crc = (crc & 0x8000) !== 0 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff
    }
  }
  return `${json}|${crc.toString(16).toUpperCase().padStart(4, '0')}`
}

class MockWebSocket {
  static instancias: MockWebSocket[] = []
  url: string
  readyState = 1
  onopen: (() => void) | null = null
  onmessage: ((ev: { data: unknown }) => void) | null = null
  onerror: (() => void) | null = null
  onclose: (() => void) | null = null
  enviados: string[] = []

  constructor(url: string) {
    this.url = url
    MockWebSocket.instancias.push(this)
    setTimeout(() => {
      if (this.onopen) {
        this.onopen()
      }
    }, 0)
  }

  send(dados: string) {
    this.enviados.push(dados)
  }

  close() {
    this.readyState = 3
  }

  emitirMensagem(dados: unknown) {
    if (this.onmessage) {
      this.onmessage({ data: dados })
    }
  }
}

describe('Painel de Corrida em Tempo Real (Issue #218)', () => {
  beforeEach(() => {
    vi.useFakeTimers()
    MockWebSocket.instancias = []
    vi.stubGlobal('WebSocket', MockWebSocket)
  })

  afterEach(() => {
    cleanup()
    vi.useRealTimers()
    vi.unstubAllGlobals()
  })

  test('renderiza estado inicial Aguardando, sem valores hardcoded falsos e com legenda compatível com MazeMap', () => {
    render(<App />)

    act(() => {
      vi.advanceTimersByTime(10)
    })

    expect(screen.getByRole('heading', { name: 'Monitoramento em Tempo Real' })).toBeTruthy()
    expect(screen.getByText('Micromouse')).toBeTruthy()
    expect(screen.getByTestId('espaco-mapa-labirinto')).toBeTruthy()

    // Item 5: Antes de iniciar a corrida, Estado da Corrida deve exibir "Aguardando"
    expect(screen.getByTestId('card-estado-corrida').textContent).toContain('Aguardando')

    // Item 3 e Item 6: Sem telemetria recebida, indicadores sem dados reais exibem "--"
    expect(screen.getByTestId('latencia-ms').textContent).toBe('--')
    expect(screen.getByTestId('ultimo-pacote-tempo').textContent).toBe('--')
    expect(screen.getByTestId('recalculos-rota').textContent).toBe('--')
    expect(screen.getByTestId('taxa-telemetria-hz').textContent).toContain('--')
    expect(screen.getByTestId('limite-tempo').textContent).toBe('Limite: --')

    // Item 6: Legenda do mapa exibe apenas itens suportados pelo MazeMap (sem Trajeto/Objetivo)
    const legenda = screen.getByTestId('legenda-mapa').textContent || ''
    expect(legenda).toContain('Robô')
    expect(legenda).toContain('Área explorada')
    expect(legenda).toContain('Parede confirmada')
    expect(legenda).not.toContain('Trajeto')
    expect(legenda).not.toContain('Objetivo')

    // Aba Visão Geral (Dashboard): verifica estado inicial Aguardando, barra de progresso e ausência de hardcoded
    fireEvent.click(screen.getByRole('button', { name: /Dashboard/i }))
    expect(screen.getByTestId('card-estado-corrida-visao-geral').textContent).toContain('Aguardando')
    expect(screen.getByTestId('resultado-ultima-execucao').textContent).toContain('Aguardando')
    expect(screen.getByTestId('ponto-resultado-ultima-execucao').className).toContain('neutro')
    expect(screen.getByTestId('data-calendario').textContent).toContain('--')
    expect(screen.getByTestId('status-banco-dados').textContent).toBe('--')
    expect(screen.getByTestId('ultima-atualizacao-sistema').textContent).toBe('--')
    expect(screen.getByTestId('barra-progresso-bateria-visao-geral').getAttribute('aria-valuenow')).toBe('100')
  })

  test('processa quadros <JSON>|<CRC>, não envia ACK pelo painel e calcula latência real apenas com timestamp do servidor', () => {
    render(<Dashboard />)
    const ws = MockWebSocket.instancias[0]

    act(() => {
      vi.advanceTimersByTime(10)
      // Exemplos da seção 7 do Contrato de Telemetria (docs/4.4.7) — sem timestamp_servidor_ms
      ws.emitirMensagem('{"v":1,"t":"evt","corrida":7,"seq":0,"tempo_ms":0,"evento":"inicio","tensao_bateria":8.21}|4886')
      ws.emitirMensagem(
        '{"v":1,"t":"tel","corrida":7,"seq":42,"tempo_ms":8400,"pos_x":2,"pos_y":1,"orientacao":"L","paredes":9,"velocidade":0.312,"tensao_bateria":7.86,"valor_flood":3,"estado":"explorando"}|2917',
      )
    })

    // Item 7: O painel web é apenas leitura e NÃO deve enviar ACK via WebSocket
    expect(ws.enviados.length).toBe(0)

    // Verifica atualização dos indicadores em tempo real
    expect(screen.getByTestId('tensao-bateria').textContent).toBe('7,86 V')
    expect(screen.getByTestId('porcentagem-bateria').textContent).toBe('73%')
    expect(screen.getByTestId('velocidade-atual').textContent).toBe('0,31 m/s')
    expect(screen.getByTestId('velocidade-media').textContent).toBe('0,31 m/s')
    expect(screen.getByTestId('texto-status-execucao').textContent).toBe('Explorando labirinto')
    expect(screen.getByTestId('orientacao-atual').textContent).toContain('Leste →')
    expect(screen.getByTestId('distancia-flood').textContent).toBe('3 células')
    expect(screen.getByTestId('posicao-atual-destaque').textContent).toBe('C2 · Leste')

    // Item 3: Sem timestamp_servidor_ms / enviado_em_ms no pacote, a latência exibe "--"
    expect(screen.getByTestId('latencia-ms').textContent).toBe('--')

    // Quando o pacote informa timestamp_servidor_ms real (inclusive > 500 ms), exibe a diferença real sem Math.min(500, ...)
    act(() => {
      const agora = Date.now()
      ws.emitirMensagem(
        gerarQuadroCrc({
          v: 1,
          t: 'tel',
          corrida: 7,
          seq: 43,
          tempo_ms: 8600,
          timestamp_servidor_ms: agora - 135,
          pos_x: 2,
          pos_y: 1,
          orientacao: 'L',
          paredes: 9,
          velocidade: 0.35,
          tensao_bateria: 7.85,
          valor_flood: 2,
          recalculos: 3,
          frequencia_hz: 5,
          limite_tempo_ms: 600000,
          estado: 'explorando',
        }),
      )
    })
    expect(screen.getByTestId('latencia-ms').textContent).toBe('135 ms')
    expect(screen.getByTestId('recalculos-rota').textContent).toBe('03')
    expect(screen.getByTestId('taxa-telemetria-hz').textContent).toContain('5 Hz')
    expect(screen.getByTestId('limite-tempo').textContent).toBe('Limite: 10:00')

    act(() => {
      const agora = Date.now()
      ws.emitirMensagem(
        gerarQuadroCrc({
          v: 1,
          t: 'tel',
          corrida: 7,
          seq: 44,
          tempo_ms: 8800,
          enviado_em_ms: agora - 640,
          pos_x: 2,
          pos_y: 1,
          orientacao: 'L',
          paredes: 9,
          velocidade: 0.35,
          tensao_bateria: 7.85,
          valor_flood: 2,
          estado: 'explorando',
        }),
      )
    })
    expect(screen.getByTestId('latencia-ms').textContent).toBe('640 ms')

    // Verifica integração real com o MazeMap.jsx (célula (2,1) observada, paredes Norte+Oeste = 9 e robô renderizado)
    const containerMapa = screen.getByTestId('espaco-mapa-labirinto')
    expect(containerMapa.querySelector('[data-celula="2,1"].maze-map__cell--observed')).toBeTruthy()
    expect(containerMapa.querySelectorAll('.maze-map__wall').length).toBe(2)
    expect(containerMapa.querySelector('[data-robo="true"]')).toBeTruthy()
  })

  test('mantém alerta de bateria crítica ativo em 7,1 V (histerese) e só desarma em >= 7,2 V (Item 2 / CT-S05)', () => {
    render(<Dashboard />)
    const ws = MockWebSocket.instancias[0]

    // Passo 1: pacote inicial com 8,4 V -> Nível normal
    act(() => {
      ws.emitirMensagem(
        gerarQuadroCrc({
          v: 1,
          t: 'tel',
          corrida: 1,
          seq: 1,
          tempo_ms: 200,
          pos_x: 0,
          pos_y: 0,
          orientacao: 'N',
          paredes: 14,
          velocidade: 0.2,
          tensao_bateria: 8.4,
          valor_flood: 5,
          estado: 'explorando',
        }),
      )
    })
    expect(screen.getByTestId('status-bateria-normal').textContent).toContain('Nível normal')

    // Passo 2: queda abaixo de 7,0 V (6,98 V) -> Dispara alerta visual de bateria crítica
    act(() => {
      ws.emitirMensagem('{"v":1,"t":"evt","corrida":7,"seq":97,"tempo_ms":19350,"evento":"bateria_critica","tensao_bateria":6.98}|8E50')
    })
    expect(screen.getByTestId('alerta-bateria-critica').textContent).toContain('Bateria crítica')

    // Passo 3: tensão sobe para 7,1 V (< 7,2 V) -> Alerta DEVE permanecer ativo pela histerese!
    act(() => {
      ws.emitirMensagem(
        gerarQuadroCrc({
          v: 1,
          t: 'tel',
          corrida: 7,
          seq: 98,
          tempo_ms: 19500,
          pos_x: 0,
          pos_y: 1,
          orientacao: 'S',
          paredes: 5,
          velocidade: 0.22,
          tensao_bateria: 7.1,
          valor_flood: 4,
          estado: 'retornando',
        }),
      )
    })
    expect(screen.getByTestId('alerta-bateria-critica').textContent).toContain('7,10 V')
    expect(screen.queryByTestId('status-bateria-normal')).toBeNull()

    // Passo 4: tensão atinge 7,2 V (>= LIMIAR_REARME_BATERIA_V) -> Alerta finalmente desarma!
    act(() => {
      ws.emitirMensagem(
        gerarQuadroCrc({
          v: 1,
          t: 'tel',
          corrida: 7,
          seq: 99,
          tempo_ms: 19700,
          pos_x: 0,
          pos_y: 1,
          orientacao: 'S',
          paredes: 5,
          velocidade: 0.25,
          tensao_bateria: 7.2,
          valor_flood: 4,
          estado: 'retornando',
        }),
      )
    })
    expect(screen.queryByTestId('alerta-bateria-critica')).toBeNull()
    expect(screen.getByTestId('status-bateria-normal').textContent).toContain('Nível normal')
    expect(screen.getByTestId('texto-status-execucao').textContent).toBe('Retornando ao início')
  })

  test('não duplica eventos em StrictMode e respeita exclusivamente os 3 labirintos oficiais (4x4, 8x4, 12x4)', () => {
    render(
      <StrictMode>
        <Dashboard />
      </StrictMode>,
    )
    const ws = MockWebSocket.instancias[0]

    act(() => {
      // Envia evento inicio com labirinto oficial 8x4 (32 células)
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'evt',
          corrida: 3,
          seq: 0,
          tempo_ms: 0,
          evento: 'inicio',
          largura: 8,
          altura: 4,
          tensao_bateria: 8.2,
        }),
      )
      // Visita a célula (5, 2) -> cobertura de 1 / 32 (3,1%), sem alterar o tamanho fixo 8x4
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          corrida: 3,
          seq: 1,
          tempo_ms: 200,
          pos_x: 5,
          pos_y: 2,
          orientacao: 'N',
          paredes: 3,
          velocidade: 0.3,
          tensao_bateria: 8.1,
          estado: 'explorando',
          largura: 9,
          altura: 9, // Dimensão não oficial deve ser ignorada
        }),
      )
    })

    expect(screen.getByTestId('card-labirinto').textContent).toContain('8 × 4')
    expect(screen.getByTestId('celulas-mapeadas-total').textContent).toBe('1 / 32')
    expect(screen.getByTestId('cobertura-percentual').textContent).toBe('3,1%')

    // Item 1: Verifica que "Célula F3 mapeada" foi registrado apenas 1 vez mesmo em StrictMode
    const eventosMapeamento = screen.getAllByText('Célula F3 mapeada')
    expect(eventosMapeamento.length).toBe(1)

    // Avança > 3 s para provocar desconexão por timeout e verifica que "Conexão perdida" não duplica em StrictMode
    act(() => {
      vi.advanceTimersByTime(3200)
    })
    const eventosOffline = screen.getAllByText('Conexão perdida — mais de 3 s sem telemetria')
    expect(eventosOffline.length).toBe(1)

    // Testa também labirinto oficial 12x4 via string "12x4"
    act(() => {
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'evt',
          corrida: 4,
          seq: 0,
          tempo_ms: 0,
          evento: 'inicio',
          labirinto: '12x4',
          tensao_bateria: 8.3,
        }),
      )
    })
    expect(screen.getByTestId('card-labirinto').textContent).toContain('12 × 4')
    expect(screen.getByTestId('celulas-mapeadas-total').textContent).toBe('0 / 48')
  })

  test('descarta pacotes corrompidos ou inválidos e mantém últimos valores na tela (CT-S03 e CT-S04)', () => {
    render(<Dashboard />)
    const ws = MockWebSocket.instancias[0]

    act(() => {
      ws.emitirMensagem(
        '{"v":1,"t":"tel","corrida":7,"seq":42,"tempo_ms":8400,"pos_x":2,"pos_y":1,"orientacao":"L","paredes":9,"velocidade":0.312,"tensao_bateria":7.86,"valor_flood":3,"estado":"explorando"}|2917',
      )
    })

    expect(screen.getByTestId('posicao-atual-destaque').textContent).toBe('C2 · Leste')

    act(() => {
      // Checksum inválido (pos_x alterado de 2 para 3 mantendo CRC 2917)
      ws.emitirMensagem(
        '{"v":1,"t":"tel","corrida":7,"seq":42,"tempo_ms":8400,"pos_x":3,"pos_y":1,"orientacao":"L","paredes":9,"velocidade":0.312,"tensao_bateria":7.86,"valor_flood":3,"estado":"explorando"}|2917',
      )
      // Outros formatos inválidos
      ws.emitirMensagem(null)
      ws.emitirMensagem('   ')
      ws.emitirMensagem('{"t":"tel"}|ZZZZ')
      ws.emitirMensagem('{json_invalido')
      ws.emitirMensagem(JSON.stringify({ v: 2, t: 'tel', tempo_ms: 100 }))
      ws.emitirMensagem(JSON.stringify({ v: 1, t: 'outro', tempo_ms: 100 }))
      ws.emitirMensagem(JSON.stringify({ v: 1, t: 'tel', tempo_ms: -1 }))
      ws.emitirMensagem(JSON.stringify({ v: 1, t: 'tel', tempo_ms: 100, pos_x: 0 }))
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          tempo_ms: 100,
          pos_x: 0,
          pos_y: 0,
          orientacao: 'N',
          paredes: 16,
          velocidade: 0.2,
          tensao_bateria: 8.0,
          estado: 'explorando',
        }),
      )
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          tempo_ms: 100,
          pos_x: -1,
          pos_y: 0,
          orientacao: 'N',
          paredes: 1,
          velocidade: 0.2,
          tensao_bateria: 8.0,
          estado: 'explorando',
        }),
      )
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          tempo_ms: 100,
          pos_x: 0,
          pos_y: 0,
          orientacao: 'X',
          paredes: 1,
          velocidade: 0.2,
          tensao_bateria: 8.0,
          estado: 'explorando',
        }),
      )
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          tempo_ms: 100,
          pos_x: 0,
          pos_y: 0,
          orientacao: 'N',
          paredes: 1,
          velocidade: 0.2,
          tensao_bateria: 8.0,
          estado: 'invalido',
        }),
      )
    })

    // Os dados válidos anteriores permanecem intactos
    expect(screen.getByTestId('posicao-atual-destaque').textContent).toBe('C2 · Leste')
    expect(screen.getByTestId('velocidade-atual').textContent).toBe('0,31 m/s')
  })

  test('detecta perda de conexão após 3 s sem pacotes e atualiza cores de Resultado na Visão Geral (CT-S06 / Item 5)', () => {
    render(<Dashboard />)
    const ws = MockWebSocket.instancias[0]

    act(() => {
      ws.emitirMensagem(
        gerarQuadroCrc({
          v: 1,
          t: 'tel',
          corrida: 1,
          seq: 1,
          tempo_ms: 200,
          pos_x: 0,
          pos_y: 0,
          orientacao: 'O',
          paredes: 11,
          velocidade: 0.4,
          tensao_bateria: 8.1,
          valor_flood: 2,
          estado: 'rapido',
        }),
      )
    })

    expect(screen.getByTestId('card-conexao').textContent).toContain('Conectado')
    expect(screen.getByTestId('texto-status-execucao').textContent).toBe('Corrida rápida')

    // Verifica na Visão Geral que "Em andamento" tem ponto âmbar
    fireEvent.click(screen.getByRole('button', { name: /Dashboard/i }))
    expect(screen.getByTestId('resultado-ultima-execucao').textContent).toContain('Em andamento')
    expect(screen.getByTestId('ponto-resultado-ultima-execucao').className).toContain('ambar')
    fireEvent.click(screen.getByRole('button', { name: /Monitoramento/i }))

    // Avança 3,2 segundos sem novos pacotes
    act(() => {
      vi.advanceTimersByTime(3200)
    })

    expect(screen.getByTestId('card-conexao').textContent).toContain('Offline')
    expect(screen.getByTestId('status-conexao-micromouse').textContent).toContain('Offline')

    // Retoma o envio com evento de reconexão e fim concluído
    act(() => {
      ws.emitirMensagem('{"v":1,"t":"evt","corrida":7,"seq":160,"tempo_ms":32010,"evento":"reconexao","offline_ms":20150,"pendentes":101}|1D4F')
      ws.emitirMensagem('{"v":1,"t":"evt","corrida":7,"seq":311,"tempo_ms":62240,"evento":"fim","resultado":"concluida","tempo_conc_s":62.24,"consumo_medio_ma":412.5}|E1BA')
    })

    expect(screen.getByTestId('card-conexao').textContent).toContain('Conectado')
    expect(screen.getByTestId('desafio-cumprido').textContent).toBe('Sim')

    // Alterna para a aba Dashboard (Visão Geral) e verifica Resultado Concluído com ponto verde
    fireEvent.click(screen.getByRole('button', { name: /Dashboard/i }))
    expect(screen.getByRole('heading', { name: 'Visão Geral' })).toBeTruthy()
    expect(screen.getByText('412,5 mA')).toBeTruthy()
    expect(screen.getByTestId('resultado-ultima-execucao').textContent).toContain('Concluído')
    expect(screen.getByTestId('ponto-resultado-ultima-execucao').className).toContain('verde')
  })

  test('padroniza pos_x/pos_y nas props do MazeMap e exibe ponto vermelho quando corrida é abandonada (Itens 5 e 8)', () => {
    render(<Dashboard />)
    const ws = MockWebSocket.instancias[0]

    act(() => {
      // Envia pacote JSON puro (como retransmitido por /ws/painel)
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          corrida: 8,
          seq: 1,
          tempo_ms: 200,
          pos_x: 1,
          pos_y: 2,
          orientacao: 'N',
          paredes: 5,
          velocidade: 0.22,
          tensao_bateria: 7.5,
          valor_flood: null,
          estado: 'explorando',
        }),
      )
      ws.emitirMensagem(
        '{"v":1,"t":"evt","corrida":8,"seq":55,"tempo_ms":11020,"evento":"fim","resultado":"abandonada","motivo":"bateria_critica","consumo_medio_ma":398.0}|0C0B',
      )
    })

    // Item 8: Verifica que roboPos e celulas priorizam pos_x e pos_y nas chaves do objeto e que o MazeMap renderizou a célula (1,2)
    const containerMapa = screen.getByTestId('espaco-mapa-labirinto')
    expect(containerMapa.getAttribute('data-celulas-count')).toBe('1')
    expect(containerMapa.getAttribute('data-robo-pos')).toBe('1,2,N')
    expect(containerMapa.getAttribute('data-robo-keys')).toBe('pos_x,pos_y')
    expect(containerMapa.getAttribute('data-celula-keys')).toBe('pos_x,pos_y')
    expect(containerMapa.querySelector('[data-celula="1,2"].maze-map__cell--observed')).toBeTruthy()

    expect(screen.getByTestId('card-estado-corrida').textContent).toContain('Abandonada')

    // Testa reconexão automática quando o WebSocket fecha
    act(() => {
      if (ws.onerror) ws.onerror()
      if (ws.onclose) ws.onclose()
      vi.advanceTimersByTime(2100)
    })
    expect(MockWebSocket.instancias.length).toBeGreaterThanOrEqual(2)

    // Item 5: Na aba Dashboard, corrida abandonada deve ter ponto vermelho e classe texto-vermelho
    fireEvent.click(screen.getByRole('button', { name: /Dashboard/i }))
    expect(screen.getByTestId('resultado-ultima-execucao').textContent).toContain('Abandonada (bateria_critica)')
    expect(screen.getByTestId('resultado-ultima-execucao').className).toContain('texto-vermelho')
    expect(screen.getByTestId('ponto-resultado-ultima-execucao').className).toContain('vermelho')

    // Navega pelas demais abas do menu lateral e testa os botões de retorno
    fireEvent.click(screen.getByRole('button', { name: /Ver detalhes/i }))
    expect(screen.getByRole('heading', { name: 'Monitoramento em Tempo Real' })).toBeTruthy()

    fireEvent.click(screen.getByRole('button', { name: /Histórico/i }))
    expect(screen.getByTestId('tela-historico')).toBeTruthy()
    fireEvent.click(screen.getByRole('button', { name: /Voltar para a Visão Geral/i }))

    fireEvent.click(screen.getByRole('button', { name: /Logs/i }))
    expect(screen.getByTestId('tela-logs')).toBeTruthy()
    fireEvent.click(screen.getByRole('button', { name: /Voltar para a Visão Geral/i }))

    fireEvent.click(screen.getByRole('button', { name: /Configurações/i }))
    expect(screen.getByTestId('tela-configuracoes')).toBeTruthy()
    fireEvent.click(screen.getByRole('button', { name: /Voltar para a Visão Geral/i }))

    fireEvent.click(screen.getByRole('button', { name: /Monitoramento/i }))
    expect(screen.getByRole('heading', { name: 'Monitoramento em Tempo Real' })).toBeTruthy()
  })
})
