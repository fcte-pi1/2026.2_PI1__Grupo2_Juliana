import { StrictMode } from 'react'
import { act, cleanup, fireEvent, render, screen, waitFor } from '@testing-library/react'
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

vi.setConfig({ testTimeout: 15000 })

describe('Painel de Corrida em Tempo Real (Issue #218)', () => {
  beforeEach(() => {
    window.localStorage.clear()
    vi.useFakeTimers()
    MockWebSocket.instancias = []
    vi.stubGlobal('WebSocket', MockWebSocket)
    vi.stubGlobal(
      'fetch',
      vi.fn(() =>
        Promise.resolve({
          ok: true,
          status: 200,
          json: () => Promise.resolve([]),
        }),
      ),
    )
  })

  afterEach(() => {
    cleanup()
    vi.useRealTimers()
    vi.unstubAllGlobals()
  })

  test('não desenha o MazeMap sem tamanho escolhido nem recebido no pacote e exibe "Escolha o tipo de labirinto"', () => {
    render(<App />)
    const ws = MockWebSocket.instancias[0]

    act(() => {
      vi.advanceTimersByTime(10)
    })

    expect(screen.getByRole('heading', { name: 'Monitoramento em Tempo Real' })).toBeTruthy()
    expect(screen.getByText('Micromouse')).toBeTruthy()
    const containerMapa = screen.getByTestId('espaco-mapa-labirinto')

    // Nenhum botão seletor está ativo inicialmente
    expect(screen.getByRole('button', { name: '4 × 4' }).getAttribute('aria-pressed')).toBe('false')
    expect(screen.getByRole('button', { name: '8 × 4' }).getAttribute('aria-pressed')).toBe('false')
    expect(screen.getByRole('button', { name: '12 × 4' }).getAttribute('aria-pressed')).toBe('false')

    // MazeMap NÃO deve ser renderizado enquanto não houver tamanho definido
    expect(screen.queryByTestId('maze-map')).toBeNull()
    expect(containerMapa.querySelector('.maze-map')).toBeNull()
    expect(screen.getByText('Escolha o tipo de labirinto')).toBeTruthy()

    // Mesmo recebendo pacote tel sem largura_celula/altura_celula (ou com largura/altura fora do contrato), continua sem desenhar o mapa
    act(() => {
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          corrida: 1,
          seq: 1,
          tempo_ms: 200,
          largura: 4,
          altura: 4,
          labirinto: '4x4',
          pos_x: 0,
          pos_y: 0,
          orientacao: 'N',
          paredes: 14,
          velocidade: 0.2,
          tensao_bateria: 8.4,
          estado: 'explorando',
        }),
      )
    })

    expect(screen.queryByTestId('maze-map')).toBeNull()
    expect(screen.getByText('Escolha o tipo de labirinto')).toBeTruthy()
    expect(screen.getByTestId('valor-labirinto-monitoramento').textContent).toBe('--')
    expect(screen.getByTestId('subtitulo-mapa').textContent).toBe('Leitura espacial • aguardando dimensões')
    expect(screen.getByTestId('celulas-mapeadas-total').textContent).toBe('1 / --')
    expect(screen.getByTestId('cobertura-percentual').textContent).toBe('--')

    // Legenda duplicada do cabeçalho e campos fora do Contrato 4.4.7 foram removidos
    expect(screen.queryByTestId('legenda-mapa')).toBeNull()
    expect(screen.queryByTestId('latencia-ms')).toBeNull()
    expect(screen.queryByTestId('limite-tempo')).toBeNull()
    expect(screen.queryByTestId('recalculos-rota')).toBeNull()
    expect(screen.queryByTestId('taxa-telemetria-hz')).toBeNull()

    // Aba Visão Geral (Dashboard): verifica "--" no labirinto e barra de progresso horizontal da bateria
    fireEvent.click(screen.getByRole('button', { name: /Dashboard/i }))
    expect(screen.queryByTestId('data-calendario')).toBeNull()
    expect(screen.getByTestId('valor-labirinto-visao-geral').textContent).toBe('--')
    expect(screen.getByTestId('tipo-labirinto-ultima-execucao').textContent).toBe('--')
    expect(screen.getByTestId('status-banco-dados').textContent).toBe('--')

    const barraBateriaTopo = screen.getByTestId('barra-progresso-bateria-visao-geral')
    expect(barraBateriaTopo.getAttribute('aria-valuenow')).toBe('100')
    expect(barraBateriaTopo.querySelector('.barra-progresso-preenchimento')).toBeTruthy()
  })

  test('seleciona "8 × 4" no seletor, grava no localStorage, desenha o mapa 8x4 e posiciona o robô em (5, 0) sem largura_celula no pacote', () => {
    const { unmount } = render(<Dashboard />)
    const ws = MockWebSocket.instancias[0]

    // Clica no botão "8 × 4" do seletor
    const botao8x4 = screen.getByRole('button', { name: '8 × 4' })
    fireEvent.click(botao8x4)

    // Verifica gravação no localStorage e destaque visual do botão ativo
    expect(window.localStorage.getItem('micromouse_tipo_labirinto')).toBe('8x4')
    expect(botao8x4.getAttribute('aria-pressed')).toBe('true')
    expect(botao8x4.className).toContain('ativo')
    expect(screen.getByRole('button', { name: '4 × 4' }).getAttribute('aria-pressed')).toBe('false')
    expect(screen.getByRole('button', { name: '12 × 4' }).getAttribute('aria-pressed')).toBe('false')

    // Envia pacote tel com pos_x: 5, pos_y: 0 (sem largura_celula no pacote)
    act(() => {
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          corrida: 1,
          seq: 1,
          tempo_ms: 200,
          pos_x: 5,
          pos_y: 0,
          orientacao: 'L',
          paredes: 5,
          velocidade: 0.28,
          tensao_bateria: 8.2,
          valor_flood: 4,
          estado: 'explorando',
        }),
      )
    })

    // Confere que o aviso sumiu, o mapa 8x4 (32 células) foi desenhado e o robô aparece na célula (5, 0)
    expect(screen.queryByText('Escolha o tipo de labirinto')).toBeNull()
    expect(screen.getByTestId('valor-labirinto-monitoramento').textContent).toBe('8 × 4')
    expect(screen.getByTestId('celulas-mapeadas-total').textContent).toBe('1 / 32')

    const containerMapa = screen.getByTestId('espaco-mapa-labirinto')
    expect(containerMapa.querySelectorAll('[data-celula]').length).toBe(32)
    expect(containerMapa.querySelector('[data-celula="5,0"].maze-map__cell--observed')).toBeTruthy()
    const elementoRobo = containerMapa.querySelector('[data-robo="true"]')
    expect(elementoRobo).toBeTruthy()
    expect(elementoRobo?.textContent).toContain('Robô em (5, 0)')

    // Simula F5 (desmonta e monta novamente) conferindo restauração do localStorage
    unmount()
    const { unmount: unmount2 } = render(<Dashboard />)
    expect(screen.getByRole('button', { name: '8 × 4' }).getAttribute('aria-pressed')).toBe('true')
    expect(screen.getByTestId('valor-labirinto-monitoramento').textContent).toBe('8 × 4')

    // Se o backend enviar largura_celula e altura_celula válidos (ex: 12x4), o valor do backend tem prioridade sobre a escolha manual
    const ws2 = MockWebSocket.instancias[MockWebSocket.instancias.length - 1]
    act(() => {
      ws2.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'evt',
          corrida: 2,
          seq: 0,
          tempo_ms: 0,
          evento: 'inicio',
          largura_celula: 12,
          altura_celula: 4,
          tensao_bateria: 8.4,
        }),
      )
    })
    expect(screen.getByRole('button', { name: '12 × 4' }).getAttribute('aria-pressed')).toBe('true')
    expect(screen.getByTestId('valor-labirinto-monitoramento').textContent).toBe('12 × 4')

    // Valor inválido no localStorage deve ser ignorado na inicialização
    unmount2()
    window.localStorage.setItem('micromouse_tipo_labirinto', '9x9')
    render(<Dashboard />)
    expect(screen.getByText('Escolha o tipo de labirinto')).toBeTruthy()
  })

  test('lê largura_celula e altura_celula em qualquer pacote (tel ou evt) e aceita apenas 4x4, 8x4 e 12x4', () => {
    render(<Dashboard />)
    const ws = MockWebSocket.instancias[0]

    act(() => {
      vi.advanceTimersByTime(10)
      // Simula abertura do painel no meio de uma corrida 8x4 recebendo pacote tel direto com largura_celula e altura_celula
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          corrida: 7,
          seq: 42,
          tempo_ms: 8400,
          largura_celula: 8,
          altura_celula: 4,
          pos_x: 2,
          pos_y: 1,
          orientacao: 'L',
          paredes: 9,
          velocidade: 0.312,
          tensao_bateria: 7.86,
          valor_flood: 3,
          estado: 'explorando',
        }),
      )
    })

    // Painel não envia ACK
    expect(ws.enviados.length).toBe(0)

    // Dimensões 8x4 atualizadas a partir do pacote tel
    expect(screen.getByTestId('valor-labirinto-monitoramento').textContent).toBe('8 × 4')
    expect(screen.getByTestId('subtitulo-mapa').textContent).toBe('Leitura espacial • 4 linhas × 8 colunas')
    expect(screen.getByTestId('celulas-mapeadas-total').textContent).toBe('1 / 32')
    expect(screen.getByTestId('cobertura-percentual').textContent).toBe('3,1%')

    // Item 2: Tamanhos invertidos ou não oficiais (4x8, 4x12, 9x9) devem ser rejeitados
    act(() => {
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          corrida: 7,
          seq: 43,
          tempo_ms: 8600,
          largura_celula: 4,
          altura_celula: 8,
          pos_x: 3,
          pos_y: 1,
          orientacao: 'L',
          paredes: 5,
          velocidade: 0.32,
          tensao_bateria: 7.85,
          valor_flood: 2,
          estado: 'explorando',
        }),
      )
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          corrida: 7,
          seq: 44,
          tempo_ms: 8800,
          largura_celula: 4,
          altura_celula: 12,
          pos_x: 4,
          pos_y: 1,
          orientacao: 'L',
          paredes: 5,
          velocidade: 0.32,
          tensao_bateria: 7.84,
          valor_flood: 1,
          estado: 'explorando',
        }),
      )
    })

    // Continua 8 × 4 (32 células), agora com 3 células mapeadas (9,4%)
    expect(screen.getByTestId('valor-labirinto-monitoramento').textContent).toBe('8 × 4')
    expect(screen.getByTestId('celulas-mapeadas-total').textContent).toBe('3 / 32')
    expect(screen.getByTestId('cobertura-percentual').textContent).toBe('9,4%')

    // Verifica integração real com o MazeMap.jsx (célula (2,1) observada e robô renderizado)
    const containerMapa = screen.getByTestId('espaco-mapa-labirinto')
    expect(containerMapa.querySelector('[data-celula="2,1"].maze-map__cell--observed')).toBeTruthy()
    expect(containerMapa.querySelector('[data-robo="true"]')).toBeTruthy()
  })

  test('acumula o trajeto sem duplicar posições consecutivas iguais e preserva ao trocar de aba (Item 3)', () => {
    render(<Dashboard />)
    const ws = MockWebSocket.instancias[0]

    act(() => {
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'evt',
          corrida: 2,
          seq: 0,
          tempo_ms: 0,
          evento: 'inicio',
          largura_celula: 4,
          altura_celula: 4,
          tensao_bateria: 8.3,
        }),
      )
      // Ponto 1: (0, 0)
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          corrida: 2,
          seq: 1,
          tempo_ms: 200,
          pos_x: 0,
          pos_y: 0,
          orientacao: 'N',
          paredes: 14,
          velocidade: 0.2,
          tensao_bateria: 8.28,
          estado: 'explorando',
        }),
      )
      // Ponto 2: (0, 1)
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          corrida: 2,
          seq: 2,
          tempo_ms: 400,
          pos_x: 0,
          pos_y: 1,
          orientacao: 'L',
          paredes: 9,
          velocidade: 0.25,
          tensao_bateria: 8.26,
          estado: 'explorando',
        }),
      )
      // Posição repetida consecutiva (0, 1) -> NÃO deve duplicar no trajeto
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          corrida: 2,
          seq: 3,
          tempo_ms: 600,
          pos_x: 0,
          pos_y: 1,
          orientacao: 'L',
          paredes: 9,
          velocidade: 0.25,
          tensao_bateria: 8.25,
          estado: 'explorando',
        }),
      )
      // Ponto 3: (1, 1)
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          corrida: 2,
          seq: 4,
          tempo_ms: 800,
          pos_x: 1,
          pos_y: 1,
          orientacao: 'S',
          paredes: 3,
          velocidade: 0.3,
          tensao_bateria: 8.24,
          estado: 'explorando',
        }),
      )
    })

    expect(screen.getByTestId('espaco-mapa-labirinto').getAttribute('data-trajeto-count')).toBe('3')
    expect(screen.getByTestId('espaco-mapa-labirinto').getAttribute('data-trajeto')).toBe('0,0;0,1;1,1')

    // Alterna para a aba Dashboard (desmonta MazeMap) e volta para Monitoramento
    fireEvent.click(screen.getByRole('button', { name: /Dashboard/i }))
    expect(screen.getByRole('heading', { name: 'Visão Geral' })).toBeTruthy()
    expect(screen.getByTestId('valor-labirinto-visao-geral').textContent).toBe('4 × 4')

    fireEvent.click(screen.getByRole('button', { name: /Monitoramento/i }))
    expect(screen.getByRole('heading', { name: 'Monitoramento em Tempo Real' })).toBeTruthy()

    // Verifica que o trajeto e as células mapeadas foram preservados após trocar de aba
    const containerAposRetorno = screen.getByTestId('espaco-mapa-labirinto')
    expect(containerAposRetorno.getAttribute('data-trajeto-count')).toBe('3')
    expect(containerAposRetorno.getAttribute('data-trajeto')).toBe('0,0;0,1;1,1')
    expect(containerAposRetorno.getAttribute('data-celulas-count')).toBe('3')

    // Novo evento "inicio" deve zerar o trajeto
    act(() => {
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'evt',
          corrida: 3,
          seq: 0,
          tempo_ms: 0,
          evento: 'inicio',
          largura_celula: 12,
          altura_celula: 4,
          tensao_bateria: 8.4,
        }),
      )
    })
    expect(screen.getByTestId('espaco-mapa-labirinto').getAttribute('data-trajeto-count')).toBe('0')
    expect(screen.getByTestId('valor-labirinto-monitoramento').textContent).toBe('12 × 4')
  })

  test('mantém alerta de bateria crítica ativo em 7,1 V (histerese) e só desarma em >= 7,2 V (CT-S05)', () => {
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
    expect(screen.getByTestId('alerta-bateria-critica').getAttribute('role')).toBe('alert')

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

    // Verifica que na aba Dashboard (Visão Geral) o card de bateria também fica destacado em estado crítico
    fireEvent.click(screen.getByRole('button', { name: /Dashboard/i }))
    expect(screen.getByTestId('card-bateria-visao-geral').className).toContain('card-bateria-critica')
    expect(screen.getByTestId('tensao-bateria-visao-geral').textContent).toBe('7,10 V')
    expect(screen.getByTestId('alerta-bateria-critica-visao-geral').textContent).toContain('7,10 V')
    fireEvent.click(screen.getByRole('button', { name: /Monitoramento/i }))

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

    fireEvent.click(screen.getByRole('button', { name: /Dashboard/i }))
    expect(screen.queryByTestId('alerta-bateria-critica-visao-geral')).toBeNull()
    expect(screen.getByTestId('status-bateria-normal-visao-geral').textContent).toContain('Nível normal')
  })

  test('não duplica eventos em StrictMode quando mapeia célula ou perde conexão', () => {
    render(
      <StrictMode>
        <Dashboard />
      </StrictMode>,
    )
    const ws = MockWebSocket.instancias[0]

    act(() => {
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          corrida: 3,
          seq: 1,
          tempo_ms: 200,
          largura_celula: 8,
          altura_celula: 4,
          pos_x: 5,
          pos_y: 2,
          orientacao: 'N',
          paredes: 3,
          velocidade: 0.3,
          tensao_bateria: 8.1,
          estado: 'explorando',
        }),
      )
    })

    // Verifica que "Célula F3 mapeada" foi registrado apenas 1 vez mesmo em StrictMode
    const eventosMapeamento = screen.getAllByText('Célula F3 mapeada')
    expect(eventosMapeamento.length).toBe(1)

    // Avança > 3 s para provocar desconexão por timeout e verifica que "Conexão perdida" não duplica em StrictMode
    act(() => {
      vi.advanceTimersByTime(3200)
    })
    const eventosOffline = screen.getAllByText('Conexão perdida — mais de 3 s sem telemetria')
    expect(eventosOffline.length).toBe(1)
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
          pos_x: -2,
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

  test('detecta perda de conexão após 3 s, trata corrida concluída/abandonada e navega entre abas (CT-S06)', () => {
    render(<Dashboard />)
    const ws = MockWebSocket.instancias[0]

    // A espera pela primeira mensagem não representa perda de conexão.
    act(() => {
      vi.advanceTimersByTime(4000)
    })
    expect(screen.queryByRole('alert')).toBeNull()

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

    // O toast só aparece ao atingir o timeout de 3 segundos.
    act(() => {
      vi.advanceTimersByTime(2800)
    })
    expect(screen.queryByRole('alert')).toBeNull()

    act(() => {
      vi.advanceTimersByTime(200)
    })

    expect(screen.getByRole('alert').textContent).toContain('Conexão com o robô perdida')

    // Mensagens inválidas não encerram o alerta.
    act(() => {
      ws.emitirMensagem('{json_invalido')
    })
    expect(screen.getByRole('alert')).toBeTruthy()

    fireEvent.click(screen.getByRole('button', { name: /Dashboard/i }))
    expect(screen.getByRole('alert')).toBeTruthy()
    expect(screen.getByTestId('connection-alert')).toBeTruthy()
    expect(screen.getByTestId('card-status-robo').className).toContain('card-alerta-conexao')
    fireEvent.click(screen.getByRole('button', { name: /Monitoramento/i }))

    expect(screen.getByTestId('card-conexao').textContent).toContain('Offline')
    expect(screen.getByTestId('status-conexao-micromouse').textContent).toContain('Offline')

    // Retoma o envio com evento de reconexão e fim concluído
    act(() => {
      ws.emitirMensagem('{"v":1,"t":"evt","corrida":7,"seq":160,"tempo_ms":32010,"evento":"reconexao","offline_ms":20150,"pendentes":101}|1D4F')
      ws.emitirMensagem('{"v":1,"t":"evt","corrida":7,"seq":311,"tempo_ms":62240,"evento":"fim","resultado":"concluida","tempo_conc_s":62.24,"consumo_medio_ma":412.5}|E1BA')
    })

    expect(screen.getByTestId('card-conexao').textContent).toContain('Conectado')
    expect(screen.getByTestId('desafio-cumprido').textContent).toBe('Sim')
    expect(screen.queryByRole('alert')).toBeNull()

    // Alterna para a aba Dashboard (Visão Geral) e verifica Resultado Concluído com ponto verde
    fireEvent.click(screen.getByRole('button', { name: /Dashboard/i }))
    expect(screen.getByRole('heading', { name: 'Visão Geral' })).toBeTruthy()
    expect(screen.getByText('412,5 mA')).toBeTruthy()
    expect(screen.getByTestId('resultado-ultima-execucao').textContent).toContain('Concluído')
    expect(screen.getByTestId('ponto-resultado-ultima-execucao').className).toContain('verde')

    // Testa corrida abandonada e reconexão automática do WebSocket
    act(() => {
      ws.emitirMensagem(
        '{"v":1,"t":"evt","corrida":8,"seq":55,"tempo_ms":11020,"evento":"fim","resultado":"abandonada","motivo":"bateria_critica","consumo_medio_ma":398.0}|0C0B',
      )
      if (ws.onerror) ws.onerror()
      if (ws.onclose) ws.onclose()
      vi.advanceTimersByTime(2100)
    })
    expect(MockWebSocket.instancias.length).toBeGreaterThanOrEqual(2)
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

    // O item "Configurações" foi removido da barra lateral de navegação
    expect(screen.queryByRole('button', { name: /Configurações/i })).toBeNull()

    fireEvent.click(screen.getByRole('button', { name: /Monitoramento/i }))
    expect(screen.getByRole('heading', { name: 'Monitoramento em Tempo Real' })).toBeTruthy()
  })

  test('trata coordenada fora da matriz (ex: (4, 3) num mapa 4x4) como saída do labirinto e destaca a penúltima célula ((3, 3))', () => {
    render(<Dashboard />)
    const ws = MockWebSocket.instancias[0]

    const percursoInterno = [
      { seq: 1, tempo_ms: 200, pos_x: 0, pos_y: 0, orientacao: 'L', paredes: 12 },
      { seq: 2, tempo_ms: 400, pos_x: 1, pos_y: 0, orientacao: 'L', paredes: 5 },
      { seq: 3, tempo_ms: 600, pos_x: 2, pos_y: 0, orientacao: 'L', paredes: 5 },
      { seq: 4, tempo_ms: 800, pos_x: 3, pos_y: 0, orientacao: 'N', paredes: 6 },
      { seq: 5, tempo_ms: 1000, pos_x: 3, pos_y: 1, orientacao: 'N', paredes: 10 },
      { seq: 6, tempo_ms: 1200, pos_x: 3, pos_y: 2, orientacao: 'N', paredes: 10 },
      { seq: 7, tempo_ms: 1400, pos_x: 3, pos_y: 3, orientacao: 'L', paredes: 9 },
    ]

    act(() => {
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'evt',
          corrida: 1,
          seq: 0,
          tempo_ms: 0,
          evento: 'inicio',
          largura_celula: 4,
          altura_celula: 4,
          tensao_bateria: 8.4,
        }),
      )

      for (const passo of percursoInterno) {
        ws.emitirMensagem(
          JSON.stringify({
            v: 1,
            t: 'tel',
            corrida: 1,
            ...passo,
            velocidade: 0.35,
            tensao_bateria: 8.2,
            valor_flood: 1,
            estado: 'explorando',
          }),
        )
      }
    })

    const containerMapa = screen.getByTestId('espaco-mapa-labirinto')
    expect(screen.getByTestId('celulas-mapeadas-total').textContent).toBe('7 / 16')
    expect(screen.getByTestId('cobertura-percentual').textContent).toBe('43,8%')
    expect(containerMapa.getAttribute('data-ultima-celula')).toBe('')
    expect(screen.queryByTestId('indicador-saida-painel')).toBeNull()

    // Envia a última posição fora da matriz (4, 3) indicando a saída do labirinto 4x4
    act(() => {
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          corrida: 1,
          seq: 8,
          tempo_ms: 1600,
          pos_x: 4,
          pos_y: 3,
          orientacao: 'L',
          paredes: 0,
          velocidade: 0.3,
          tensao_bateria: 8.18,
          valor_flood: 0,
          estado: 'rapido',
        }),
      )
    })

    // (a) A coordenada externa (4, 3) NÃO deve entrar em mapaCelulas nem distorcer a cobertura
    expect(screen.getByTestId('celulas-mapeadas-total').textContent).toBe('7 / 16')
    expect(screen.getByTestId('cobertura-percentual').textContent).toBe('43,8%')
    expect(containerMapa.getAttribute('data-celulas-count')).toBe('7')

    // (b) A penúltima coordenada (3, 3) fica marcada como última célula interna e (4, 3) como saída
    expect(containerMapa.getAttribute('data-ultima-celula')).toBe('3,3')
    expect(containerMapa.getAttribute('data-saida-labirinto')).toBe('4,3')
    expect(containerMapa.querySelector('[data-ultima-celula-marcador="3,3"]')).toBeTruthy()
    expect(containerMapa.querySelector('[data-celula="3,3"].maze-map__cell--exit')).toBeTruthy()

    expect(screen.getByTestId('posicao-atual-destaque').textContent).toBe('Saída (4, 3) · Leste')
    expect(screen.getByTestId('indicador-saida-painel').textContent).toContain('Saída do mapa em (4, 3)')
    expect(screen.getByTestId('indicador-saida-painel').textContent).toContain('D4 (3, 3)')
    expect(screen.getByTestId('celula-atual-badge').textContent).toContain('Saída (4, 3) · Última: D4 (3, 3)')
    expect(screen.getByTestId('status-saida-mapa').textContent).toContain('Saída em (4, 3)')
    expect(screen.getByText('Saída do labirinto alcançada (4, 3)')).toBeTruthy()
  })

  test('aceita saídas válidas pela borda inferior ((0, -1)) e esquerda ((-1, 0)) e rejeita saídas inválidas ((50, 3), (4, 4) e (-1, -1))', () => {
    render(<Dashboard />)
    const ws = MockWebSocket.instancias[0]

    // 1. Saídas inválidas após chegar em (3, 3): (50, 3), diagonal (4, 4) e diagonal (-1, -1) NÃO podem virar saída
    act(() => {
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'evt',
          corrida: 1,
          seq: 0,
          tempo_ms: 0,
          evento: 'inicio',
          largura_celula: 4,
          altura_celula: 4,
          tensao_bateria: 8.4,
        }),
      )
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          corrida: 1,
          seq: 1,
          tempo_ms: 200,
          pos_x: 3,
          pos_y: 3,
          orientacao: 'N',
          paredes: 9,
          velocidade: 0.3,
          tensao_bateria: 8.3,
          estado: 'explorando',
        }),
      )
      for (const invalido of [
        { pos_x: 50, pos_y: 3 },
        { pos_x: 4, pos_y: 4 },
        { pos_x: -1, pos_y: -1 },
      ]) {
        ws.emitirMensagem(
          JSON.stringify({
            v: 1,
            t: 'tel',
            corrida: 1,
            seq: 2,
            tempo_ms: 400,
            ...invalido,
            orientacao: 'N',
            paredes: 0,
            velocidade: 0.3,
            tensao_bateria: 8.3,
            estado: 'explorando',
          }),
        )
      }
    })

    const containerMapa = screen.getByTestId('espaco-mapa-labirinto')
    expect(containerMapa.getAttribute('data-trajeto')).toBe('3,3')
    expect(containerMapa.getAttribute('data-saida-labirinto')).toBe('')
    expect(containerMapa.getAttribute('data-ultima-celula')).toBe('')
    expect(containerMapa.querySelector('[data-ultima-celula-marcador]')).toBeNull()
    expect(screen.getByTestId('posicao-atual-destaque').textContent).toBe('D4 · Norte')
    expect(screen.queryByTestId('indicador-saida-painel')).toBeNull()

    // 2. Saída válida pela borda inferior: (0, 0) -> (0, -1)
    act(() => {
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'evt',
          corrida: 2,
          seq: 0,
          tempo_ms: 0,
          evento: 'inicio',
          largura_celula: 4,
          altura_celula: 4,
          tensao_bateria: 8.4,
        }),
      )
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          corrida: 2,
          seq: 1,
          tempo_ms: 200,
          pos_x: 0,
          pos_y: 0,
          orientacao: 'S',
          paredes: 11,
          velocidade: 0.3,
          tensao_bateria: 8.3,
          estado: 'explorando',
        }),
      )
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          corrida: 2,
          seq: 2,
          tempo_ms: 400,
          pos_x: 0,
          pos_y: -1,
          orientacao: 'S',
          paredes: 0,
          velocidade: 0.3,
          tensao_bateria: 8.28,
          estado: 'rapido',
        }),
      )
    })

    expect(containerMapa.getAttribute('data-ultima-celula')).toBe('0,0')
    expect(containerMapa.getAttribute('data-saida-labirinto')).toBe('0,-1')
    expect(screen.getByTestId('celulas-mapeadas-total').textContent).toBe('1 / 16')
    expect(screen.getByTestId('posicao-atual-destaque').textContent).toBe('Saída (0, -1) · Sul')

    // 3. Saída válida pela borda esquerda: (0, 0) -> (-1, 0)
    act(() => {
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'evt',
          corrida: 3,
          seq: 0,
          tempo_ms: 0,
          evento: 'inicio',
          largura_celula: 4,
          altura_celula: 4,
          tensao_bateria: 8.4,
        }),
      )
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          corrida: 3,
          seq: 1,
          tempo_ms: 200,
          pos_x: 0,
          pos_y: 0,
          orientacao: 'O',
          paredes: 7,
          velocidade: 0.3,
          tensao_bateria: 8.3,
          estado: 'explorando',
        }),
      )
      ws.emitirMensagem(
        JSON.stringify({
          v: 1,
          t: 'tel',
          corrida: 3,
          seq: 2,
          tempo_ms: 400,
          pos_x: -1,
          pos_y: 0,
          orientacao: 'O',
          paredes: 0,
          velocidade: 0.3,
          tensao_bateria: 8.28,
          estado: 'rapido',
        }),
      )
    })

    expect(containerMapa.getAttribute('data-ultima-celula')).toBe('0,0')
    expect(containerMapa.getAttribute('data-saida-labirinto')).toBe('-1,0')
    expect(screen.getByTestId('celulas-mapeadas-total').textContent).toBe('1 / 16')
    expect(screen.getByTestId('posicao-atual-destaque').textContent).toBe('Saída (-1, 0) · Oeste')
  })

  test('pacote tel com coordenada negativa (-1) sem dimensões definidas conta como sinal de vida e não dispara falso alerta de perda de conexão após 3s', () => {
    render(<Dashboard />)
    const ws = MockWebSocket.instancias[0]

    // Envia pacote inicial válido sem dimensões definidas
    act(() => {
      ws.emitirMensagem(
        JSON.stringify({
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
          tensao_bateria: 8.3,
          estado: 'explorando',
        }),
      )
    })

    // Durante 5 segundos, envia pacotes tel a cada 1 s com pos_x: -1 sem dimensões definidas
    for (let passo = 1; passo <= 5; passo += 1) {
      act(() => {
        vi.advanceTimersByTime(1000)
        ws.emitirMensagem(
          JSON.stringify({
            v: 1,
            t: 'tel',
            corrida: 1,
            seq: passo + 1,
            tempo_ms: 200 + passo * 1000,
            pos_x: -1,
            pos_y: 0,
            orientacao: 'O',
            paredes: 0,
            velocidade: 0.2,
            tensao_bateria: 8.25,
            estado: 'explorando',
          }),
        )
      })
      expect(screen.queryByTestId('connection-alert')).toBeNull()
      expect(screen.getByTestId('card-conexao').textContent).toContain('Conectado')
    }

    // A posição original permanece intacta enquanto não há dimensões definidas
    expect(screen.getByTestId('posicao-atual-destaque').textContent).toBe('A1 · Norte')

    // Ao parar de enviar pacotes por > 3 s, o alerta de perda de conexão dispara normalmente
    act(() => {
      vi.advanceTimersByTime(3200)
    })
    expect(screen.getByTestId('connection-alert')).toBeTruthy()
  })
})

describe('Telas de Consulta e Histórico de Corridas (Issue #220 - 3.8.5, 3.8.2 e 3.8.7)', () => {
  const labirintosMock = [
    { id: 1, nome: '4x4', largura_celula: 4, altura_celula: 4, total_tentativas: 2 },
    { id: 2, nome: '8x4', largura_celula: 8, altura_celula: 4, total_tentativas: 1 },
    { id: 3, nome: '12x4', largura_celula: 12, altura_celula: 4, total_tentativas: 0 },
  ]

  const corridasMock = [
    {
      id: 28,
      id_labirinto: 2,
      labirinto: '8x4',
      largura_celula: 8,
      altura_celula: 4,
      num_tentativa: 1,
      num_corrida_robo: 28,
      status: 'concluida',
      iniciado_em: '2026-09-25T16:42:00-03:00',
      tempo_conc_s: 91.0,
      duracao_s: 91.0,
      consumo_medio_ma: 412.5,
      velocidade_media: 0.39,
      total_leituras: 18,
    },
    {
      id: 27,
      id_labirinto: 1,
      labirinto: '4x4',
      largura_celula: 4,
      altura_celula: 4,
      num_tentativa: 2,
      num_corrida_robo: 27,
      status: 'concluida',
      iniciado_em: '2026-09-25T15:10:00-03:00',
      tempo_conc_s: 38.4,
      duracao_s: 38.4,
      consumo_medio_ma: 390.0,
      velocidade_media: 0.44,
      total_leituras: 7,
    },
    {
      id: 26,
      id_labirinto: 1,
      labirinto: '4x4',
      largura_celula: 4,
      altura_celula: 4,
      num_tentativa: 1,
      num_corrida_robo: 26,
      status: 'abandonada',
      iniciado_em: '2026-09-24T14:00:00-03:00',
      tempo_conc_s: null,
      duracao_s: 15.2,
      consumo_medio_ma: 350.0,
      velocidade_media: 0.28,
      total_leituras: 4,
    },
  ]

  const detalheCorrida27Mock = {
    ...corridasMock[1],
    leituras: [
      {
        tempo_ms: 0,
        pos_x: 0,
        pos_y: 0,
        orientacao: 'L',
        paredes: 12,
        velocidade: 0.4,
        tensao_bateria: 8.3,
        valor_flood: 3,
      },
      {
        tempo_ms: 200,
        pos_x: 1,
        pos_y: 0,
        orientacao: 'N',
        paredes: 6,
        velocidade: 0.45,
        tensao_bateria: 8.2,
        valor_flood: 2,
      },
      {
        tempo_ms: 400,
        pos_x: 1,
        pos_y: 1,
        orientacao: 'N',
        paredes: 9,
        velocidade: 0.47,
        tensao_bateria: 8.1,
        valor_flood: 1,
      },
    ],
  }

  const logsMock = [
    {
      id: 103,
      id_corrida: 28,
      tipo: 'erro',
      origem: 'backend',
      mensagem: 'checksum invalido (recebido 0000, calculado 2917)',
      ocorrido_em: '2026-09-26T19:42:36-03:00',
    },
    {
      id: 102,
      id_corrida: 28,
      tipo: 'aviso',
      origem: 'firmware',
      mensagem: 'desconexao: conexao WebSocket do robo encerrada',
      ocorrido_em: '2026-09-26T19:42:35-03:00',
    },
    {
      id: 101,
      id_corrida: 27,
      tipo: 'info',
      origem: 'backend',
      mensagem: 'Corrida iniciada',
      ocorrido_em: '2026-09-25T15:10:00-03:00',
    },
  ]

  beforeEach(() => {
    window.localStorage.clear()
    MockWebSocket.instancias = []
    vi.stubGlobal('WebSocket', MockWebSocket)
  })

  afterEach(() => {
    cleanup()
    vi.unstubAllGlobals()
  })

  test('carrega a Consulta Geral dos Resultados (GET /labirintos e GET /corridas), exibe cards de resumo, tentativas por labirinto e tabela completa (RF09-S, RF10-S, CT-S09, CT-S12)', async () => {
    const fetchMock = vi.fn((url: string) => {
      if (url.endsWith('/labirintos')) {
        return Promise.resolve({
          ok: true,
          status: 200,
          json: () => Promise.resolve(labirintosMock),
        })
      }
      if (url.includes('/corridas')) {
        return Promise.resolve({
          ok: true,
          status: 200,
          json: () => Promise.resolve(corridasMock),
        })
      }
      return Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve([]) })
    })
    vi.stubGlobal('fetch', fetchMock)

    render(<Dashboard apiBaseUrl="http://localhost:8000" />)

    // Navega para a aba Histórico
    fireEvent.click(screen.getByRole('button', { name: /Histórico/i }))
    expect(screen.getByRole('heading', { name: 'Histórico de Corridas' })).toBeTruthy()
    expect(screen.getByTestId('tela-historico')).toBeTruthy()

    await waitFor(() => {
      expect(screen.getByTestId('tabela-execucoes-registradas')).toBeTruthy()
    })

    // Verifica que as rotas da 3.8.2 foram chamadas
    const urlsChamadas = fetchMock.mock.calls.map((c) => c[0])
    expect(urlsChamadas).toContain('http://localhost:8000/labirintos')
    expect(urlsChamadas).toContain('http://localhost:8000/corridas')

    // Verifica cards de resumo e contagem de tentativas por labirinto (CT-S09)
    expect(screen.getByTestId('valor-total-corridas-historico').textContent).toBe('3')
    expect(screen.getByTestId('valor-corridas-concluidas').textContent).toBe('2')
    expect(screen.getByTestId('valor-corridas-abandonadas').textContent).toBe('1')
    expect(screen.getByTestId('chip-labirinto-4x4').textContent).toContain('4 × 4: 2')
    expect(screen.getByTestId('chip-labirinto-8x4').textContent).toContain('8 × 4: 1')
    expect(screen.getByTestId('chip-labirinto-12x4').textContent).toContain('12 × 4: 0')

    // Verifica contador e linhas da tabela de Consulta Geral (CT-S12)
    expect(screen.getByTestId('contador-corridas-encontradas').textContent).toContain(
      '3 corridas encontradas',
    )
    const linha28 = screen.getByTestId('linha-corrida-28')
    expect(linha28.textContent).toContain('8 × 4')
    expect(linha28.textContent).toContain('Tentativa 1')
    expect(linha28.textContent).toContain('Exploração')
    expect(linha28.textContent).toContain('01:31 (91,00 s)')
    expect(linha28.textContent).toContain('0,39 m/s')
    expect(linha28.textContent).toContain('412,5 mA')
    expect(linha28.textContent).toContain('Concluído')

    const linha27 = screen.getByTestId('linha-corrida-27')
    expect(linha27.textContent).toContain('4 × 4')
    expect(linha27.textContent).toContain('Tentativa 2')
    expect(linha27.textContent).toContain('Rápido')
    expect(linha27.textContent).toContain('00:38 (38,40 s)')
    expect(linha27.textContent).toContain('0,44 m/s')
    expect(linha27.textContent).toContain('Concluído')

    const linha26 = screen.getByTestId('linha-corrida-26')
    expect(linha26.textContent).toContain('00:15 (15,20 s)')
    expect(linha26.textContent).toContain('Não concluído')
  })

  test('filtra corridas por labirinto enviando query param id_labirinto e exibe "Labirinto sem registros" quando vazio (RF08-S, CT-S10 e CT-S11)', async () => {
    const fetchMock = vi.fn((url: string) => {
      if (url.endsWith('/labirintos')) {
        return Promise.resolve({
          ok: true,
          status: 200,
          json: () => Promise.resolve(labirintosMock),
        })
      }
      if (url.includes('/corridas?id_labirinto=1')) {
        return Promise.resolve({
          ok: true,
          status: 200,
          json: () => Promise.resolve(corridasMock.filter((c) => c.id_labirinto === 1)),
        })
      }
      if (url.includes('/corridas?id_labirinto=3')) {
        return Promise.resolve({
          ok: true,
          status: 200,
          json: () => Promise.resolve([]),
        })
      }
      return Promise.resolve({
        ok: true,
        status: 200,
        json: () => Promise.resolve(corridasMock),
      })
    })
    vi.stubGlobal('fetch', fetchMock)

    render(<Dashboard apiBaseUrl="http://localhost:8000" />)
    fireEvent.click(screen.getByRole('button', { name: /Histórico/i }))

    await waitFor(() => {
      expect(screen.getByTestId('tabela-execucoes-registradas')).toBeTruthy()
    })

    // Passo 1 (CT-S10): Filtra pelo labirinto 4 × 4 (id=1)
    const selectLabirinto = screen.getByTestId('filtro-labirinto')
    fireEvent.change(selectLabirinto, { target: { value: '1' } })

    await waitFor(() => {
      expect(screen.getByTestId('contador-corridas-encontradas').textContent).toContain(
        '2 corridas encontradas',
      )
    })
    expect(fetchMock.mock.calls.some((c) => c[0] === 'http://localhost:8000/corridas?id_labirinto=1')).toBe(
      true,
    )
    expect(screen.getByTestId('linha-corrida-27')).toBeTruthy()
    expect(screen.getByTestId('linha-corrida-26')).toBeTruthy()
    expect(screen.queryByTestId('linha-corrida-28')).toBeNull()

    // Passo 2 (CT-S11): Filtra pelo labirinto 12 × 4 (id=3), que não possui corridas registradas
    fireEvent.change(selectLabirinto, { target: { value: '3' } })

    await waitFor(() => {
      expect(screen.getByTestId('estado-labirinto-sem-registros')).toBeTruthy()
    })
    expect(fetchMock.mock.calls.some((c) => c[0] === 'http://localhost:8000/corridas?id_labirinto=3')).toBe(
      true,
    )
    expect(screen.getByTestId('contador-corridas-encontradas').textContent).toContain(
      '0 corridas encontradas',
    )
    expect(screen.getByTestId('estado-labirinto-sem-registros').textContent).toContain(
      'Labirinto sem registros',
    )
    expect(screen.getByTestId('estado-labirinto-sem-registros').textContent).toContain(
      'Nenhuma corrida registrada para este labirinto (12 × 4)',
    )

    // Limpa os filtros e volta a mostrar as 3 corridas gerais
    fireEvent.click(screen.getByTestId('botao-limpar-filtros-historico'))
    await waitFor(() => {
      expect(screen.getByTestId('contador-corridas-encontradas').textContent).toContain(
        '3 corridas encontradas',
      )
    })
  })

  test('abre Detalhes da Corrida via GET /corridas/{id} e renderiza o <MazeMap /> com o trajeto reconstruído (RF07-S, CT-S07)', async () => {
    const fetchMock = vi.fn((url: string) => {
      if (url.endsWith('/labirintos')) {
        return Promise.resolve({
          ok: true,
          status: 200,
          json: () => Promise.resolve(labirintosMock),
        })
      }
      if (url.endsWith('/corridas/27')) {
        return Promise.resolve({
          ok: true,
          status: 200,
          json: () => Promise.resolve(detalheCorrida27Mock),
        })
      }
      return Promise.resolve({
        ok: true,
        status: 200,
        json: () => Promise.resolve(corridasMock),
      })
    })
    vi.stubGlobal('fetch', fetchMock)

    render(<Dashboard apiBaseUrl="http://localhost:8000" />)
    fireEvent.click(screen.getByRole('button', { name: /Histórico/i }))

    await waitFor(() => {
      expect(screen.getByTestId('botao-detalhes-corrida-27')).toBeTruthy()
    })

    // Clica em "Ver detalhes" na corrida #27 (4x4)
    fireEvent.click(screen.getByTestId('botao-detalhes-corrida-27'))

    await waitFor(() => {
      expect(screen.getByTestId('tela-detalhes-corrida')).toBeTruthy()
    })

    expect(fetchMock.mock.calls.some((c) => c[0] === 'http://localhost:8000/corridas/27')).toBe(true)
    expect(screen.getByTestId('detalhe-id-corrida').textContent).toBe('#027')
    expect(screen.getByTestId('detalhe-labirinto').textContent).toContain('4 × 4')
    expect(screen.getByTestId('detalhe-tentativa').textContent).toBe('2')
    expect(screen.getByTestId('detalhe-resultado').textContent).toContain('Concluído')
    expect(screen.getByTestId('detalhe-tempo-conclusao').textContent).toContain('00:38 (38,40 s)')
    expect(screen.getByTestId('detalhe-velocidade-media').textContent).toBe('0,44 m/s')
    expect(screen.getByTestId('detalhe-consumo-medio').textContent).toBe('390,0 mA')

    // Verifica que o <MazeMap /> foi renderizado com as 3 células e o trajeto 0,0;1,0;1,1
    const containerMapaDetalhe = screen.getByTestId('espaco-mapa-detalhe-corrida')
    expect(containerMapaDetalhe.getAttribute('data-celulas-count')).toBe('3')
    expect(containerMapaDetalhe.getAttribute('data-trajeto-count')).toBe('3')
    expect(containerMapaDetalhe.getAttribute('data-trajeto')).toBe('0,0;1,0;1,1')
    expect(containerMapaDetalhe.querySelector('.maze-map')).toBeTruthy()
    expect(containerMapaDetalhe.querySelector('[data-celula="1,1"].maze-map__cell--observed')).toBeTruthy()

    // Volta ao histórico de corridas
    fireEvent.click(screen.getByTestId('botao-voltar-historico'))
    expect(screen.getByTestId('tabela-execucoes-registradas')).toBeTruthy()
  })

  test('trata erro de rede na consulta de histórico e permite tentar novamente', async () => {
    let falhar = true
    const fetchMock = vi.fn((url: string) => {
      if (url.endsWith('/labirintos')) {
        return Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve(labirintosMock) })
      }
      if (falhar) {
        return Promise.resolve({ ok: false, status: 500, json: () => Promise.resolve({}) })
      }
      return Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve(corridasMock) })
    })
    vi.stubGlobal('fetch', fetchMock)

    render(<Dashboard apiBaseUrl="http://localhost:8000" />)
    fireEvent.click(screen.getByRole('button', { name: /Histórico/i }))

    await waitFor(() => {
      expect(screen.getByTestId('erro-historico')).toBeTruthy()
    })

    falhar = false
    fireEvent.click(screen.getByRole('button', { name: /Tentar novamente/i }))

    await waitFor(() => {
      expect(screen.getByTestId('tabela-execucoes-registradas')).toBeTruthy()
    })
  })

  test('integra a aba Logs e Eventos com GET /logs, filtros de tipo/origem/busca e painel Detalhes do evento (3.8.7, RF20-S, CT-S03 e CT-S14)', async () => {
    const fetchMock = vi.fn((url: string) => {
      if (url.includes('/logs?tipo=erro')) {
        return Promise.resolve({
          ok: true,
          status: 200,
          json: () => Promise.resolve(logsMock.filter((l) => l.tipo === 'erro')),
        })
      }
      if (url.includes('/logs')) {
        return Promise.resolve({
          ok: true,
          status: 200,
          json: () => Promise.resolve(logsMock),
        })
      }
      return Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve([]) })
    })
    vi.stubGlobal('fetch', fetchMock)

    render(<Dashboard apiBaseUrl="http://localhost:8000" />)
    fireEvent.click(screen.getByRole('button', { name: /Logs/i }))

    await waitFor(() => {
      expect(screen.getByTestId('tabela-registro-eventos')).toBeTruthy()
    })

    expect(screen.getByTestId('valor-logs-total').textContent).toBe('3')
    expect(screen.getByTestId('valor-logs-avisos').textContent).toBe('1')
    expect(screen.getByTestId('valor-logs-erros').textContent).toBe('1')

    // Filtra por Tipo de evento = "Erro" (CT-S03)
    fireEvent.change(screen.getByTestId('filtro-tipo-log'), { target: { value: 'erro' } })

    await waitFor(() => {
      expect(screen.getByTestId('contador-logs-encontrados').textContent).toContain(
        '1 eventos encontrados',
      )
    })
    expect(fetchMock.mock.calls.some((c) => c[0] === 'http://localhost:8000/logs?tipo=erro')).toBe(
      true,
    )

    // Clica no evento de descarte para abrir o painel "Detalhes do evento" e confere o campo "Motivo"
    fireEvent.click(screen.getByTestId('botao-detalhe-log-103'))
    const painelDetalhes = screen.getByTestId('painel-detalhes-evento')
    expect(painelDetalhes).toBeTruthy()
    expect(painelDetalhes.textContent).toContain('Descartado')
    expect(screen.getByTestId('detalhe-evento-motivo').textContent).toContain(
      'checksum invalido (recebido 0000, calculado 2917)',
    )

    // Limpa os filtros de logs (CT-S14)
    fireEvent.click(screen.getByTestId('botao-limpar-filtros-logs'))
    await waitFor(() => {
      expect(screen.getByTestId('contador-logs-encontrados').textContent).toContain(
        '3 eventos encontrados',
      )
    })
  })
})

