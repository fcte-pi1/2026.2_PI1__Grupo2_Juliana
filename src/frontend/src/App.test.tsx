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

  test('renderiza o cabeçalho, menu lateral, espaço central do mapa e cards de indicadores', () => {
    render(<App />)

    act(() => {
      vi.advanceTimersByTime(10)
    })

    expect(screen.getByRole('heading', { name: 'Monitoramento em Tempo Real' })).toBeTruthy()
    expect(screen.getByText('Micromouse')).toBeTruthy()
    expect(screen.getByTestId('espaco-mapa-labirinto')).toBeTruthy()
    expect(screen.getByTestId('card-bateria')).toBeTruthy()
    expect(screen.getByTestId('card-velocidade')).toBeTruthy()
    expect(screen.getByTestId('card-posicao')).toBeTruthy()
    expect(screen.getByTestId('card-status-execucao')).toBeTruthy()
    expect(screen.getByTestId('card-comunicacao')).toBeTruthy()
  })

  test('processa quadros <JSON>|<CRC> do Contrato de Telemetria (4.4.7) e atualiza indicadores em < 500 ms', () => {
    render(<Dashboard />)
    const ws = MockWebSocket.instancias[0]

    act(() => {
      vi.advanceTimersByTime(10)
      // Exemplos exatos da seção 7 do Contrato de Telemetria (docs/4.4.7)
      ws.emitirMensagem('{"v":1,"t":"evt","corrida":7,"seq":0,"tempo_ms":0,"evento":"inicio","tensao_bateria":8.21}|4886')
      ws.emitirMensagem(
        '{"v":1,"t":"tel","corrida":7,"seq":42,"tempo_ms":8400,"pos_x":2,"pos_y":1,"orientacao":"L","paredes":9,"velocidade":0.312,"tensao_bateria":7.86,"valor_flood":3,"estado":"explorando"}|2917',
      )
    })

    // Verifica envio de ACK no formato <JSON>|<CRC>
    expect(ws.enviados.length).toBe(2)
    expect(ws.enviados[1]).toBe('{"t":"ack","corrida":7,"seq":42}|7278')

    // Verifica atualização dos indicadores em tempo real
    expect(screen.getByTestId('tensao-bateria').textContent).toBe('7,86 V')
    expect(screen.getByTestId('porcentagem-bateria').textContent).toBe('73%')
    expect(screen.getByTestId('velocidade-atual').textContent).toBe('0,31 m/s')
    expect(screen.getByTestId('velocidade-media').textContent).toBe('0,31 m/s')
    expect(screen.getByTestId('texto-status-execucao').textContent).toBe('Explorando labirinto')
    expect(screen.getByTestId('orientacao-atual').textContent).toContain('Leste →')
    expect(screen.getByTestId('distancia-flood').textContent).toBe('3 células')
    expect(screen.getByTestId('posicao-atual-destaque').textContent).toBe('C2 · Leste')

    // Verifica se a latência exibida respeita o limite de 500 ms (RNF06-S)
    const textoLatencia = screen.getByTestId('latencia-ms').textContent || ''
    const valorLatencia = Number.parseInt(textoLatencia.replace(' ms', ''), 10)
    expect(valorLatencia).toBeLessThanOrEqual(500)

    // Verifica integração real com o MazeMap.jsx (célula (2,1) observada, paredes Norte+Oeste = 9 e robô renderizado)
    const containerMapa = screen.getByTestId('espaco-mapa-labirinto')
    expect(containerMapa.querySelector('[data-celula="2,1"].maze-map__cell--observed')).toBeTruthy()
    expect(containerMapa.querySelectorAll('.maze-map__wall').length).toBe(2)
    expect(containerMapa.querySelector('[data-robo="true"]')).toBeTruthy()
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

  test('exibe alerta visual de bateria crítica abaixo de 7,0 V e volta ao normal em 8,4 V (CT-S05)', () => {
    render(<Dashboard />)
    const ws = MockWebSocket.instancias[0]

    // Passo 1: pacote com 8,4 V -> Nível normal
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

    // Passo 2: evento bateria_critica / tensão < 7,0 V -> Alerta visual imediato
    act(() => {
      ws.emitirMensagem('{"v":1,"t":"evt","corrida":7,"seq":97,"tempo_ms":19350,"evento":"bateria_critica","tensao_bateria":6.98}|8E50')
    })
    expect(screen.getByTestId('alerta-bateria-critica').textContent).toContain('Bateria crítica')

    // Passo 3: tensão volta para 8,4 V -> Alerta some e volta ao estado normal
    act(() => {
      ws.emitirMensagem(
        gerarQuadroCrc({
          v: 1,
          t: 'tel',
          corrida: 7,
          seq: 98,
          tempo_ms: 19600,
          pos_x: 0,
          pos_y: 1,
          orientacao: 'S',
          paredes: 5,
          velocidade: 0.25,
          tensao_bateria: 8.4,
          valor_flood: 4,
          estado: 'retornando',
        }),
      )
    })
    expect(screen.queryByTestId('alerta-bateria-critica')).toBeNull()
    expect(screen.getByTestId('status-bateria-normal').textContent).toContain('Nível normal')
    expect(screen.getByTestId('texto-status-execucao').textContent).toBe('Retornando ao início')
  })

  test('detecta perda de conexão após 3 s sem pacotes e restabelece ao receber dados (CT-S06)', () => {
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

    // Alterna para a aba Dashboard (Visão Geral) e verifica exibição dos dados consolidados
    fireEvent.click(screen.getByRole('button', { name: /Dashboard/i }))
    expect(screen.getByRole('heading', { name: 'Visão Geral' })).toBeTruthy()
    expect(screen.getByText('412,5 mA')).toBeTruthy()
  })

  test('repassa corretamente as props celulas e roboPos para o componente MazeMap e trata corrida abandonada', () => {
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

    const containerMapa = screen.getByTestId('espaco-mapa-labirinto')
    expect(containerMapa.getAttribute('data-celulas-count')).toBe('1')
    expect(containerMapa.getAttribute('data-robo-pos')).toBe('1,2,N')
    expect(containerMapa.querySelector('[data-celula="1,2"].maze-map__cell--observed')).toBeTruthy()
    expect(screen.getByTestId('card-estado-corrida').textContent).toContain('Abandonada')

    // Testa reconexão automática quando o WebSocket fecha
    act(() => {
      if (ws.onerror) ws.onerror()
      if (ws.onclose) ws.onclose()
      vi.advanceTimersByTime(2100)
    })
    expect(MockWebSocket.instancias.length).toBeGreaterThanOrEqual(2)

    // Navega pelas demais abas do menu lateral e testa os botões de retorno
    fireEvent.click(screen.getByRole('button', { name: /Dashboard/i }))
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
