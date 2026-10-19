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
    window.localStorage.clear()
    vi.useFakeTimers()
    MockWebSocket.instancias = []
    vi.stubGlobal('WebSocket', MockWebSocket)
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

    fireEvent.click(screen.getByRole('button', { name: /Configurações/i }))
    expect(screen.getByTestId('tela-configuracoes')).toBeTruthy()
    fireEvent.click(screen.getByRole('button', { name: /Voltar para a Visão Geral/i }))

    fireEvent.click(screen.getByRole('button', { name: /Monitoramento/i }))
    expect(screen.getByRole('heading', { name: 'Monitoramento em Tempo Real' })).toBeTruthy()
  })
})
