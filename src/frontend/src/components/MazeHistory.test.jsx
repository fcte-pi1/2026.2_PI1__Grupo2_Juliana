import { useState } from 'react'
import { cleanup, fireEvent, render, screen, within } from '@testing-library/react'
import { afterEach, describe, expect, test, vi } from 'vitest'
import MazeHistory from './MazeHistory.jsx'

afterEach(cleanup)

const labirintos = [
  { id: 1, nome: 'Pequeno', largura_celula: 4, altura_celula: 4 },
  { id: 2, nome: 'Médio', largura_celula: 8, altura_celula: 4 },
  { id: 3, nome: 'Grande', largura_celula: 12, altura_celula: 4 },
]
const corridas = [
  {
    id: 10, id_labirinto: 1, num_tentativa: 1, modo: 'exploracao',
    status: 'abandonada', duracao_s: 1.4, tempo_conc_s: null,
    leituras: [
      { pos_x: 0, pos_y: 1, paredes: 9, orientacao: 'N', tempo_ms: 1400 },
      { pos_x: 0, pos_y: 0, paredes: 11, orientacao: 'N', tempo_ms: 0 },
    ],
  },
  {
    id: 11, id_labirinto: 1, num_tentativa: 2, modo: 'otimizado',
    status: 'concluida', duracao_s: 12.345,
    celulas: { '1,0': 1 }, trajeto: [{ pos_x: 1, pos_y: 0 }],
    roboPos: { pos_x: 1, pos_y: 0, orientacao: 'L' },
  },
  { id: 20, id_labirinto: 2, num_tentativa: 7, modo: 'otimizado', status: 'concluida', tempo_conc_s: 0 },
]

describe('MazeHistory', () => {
  test('funciona isoladamente com demonstração e labirinto sem registros', () => {
    render(<MazeHistory />)
    expect(screen.getByText('Dados de demonstração')).toBeTruthy()
    expect(screen.getByRole('table')).toBeTruthy()
    fireEvent.change(screen.getByRole('combobox'), { target: { value: '2' } })
    expect(screen.getByRole('status').textContent).toContain('Nenhuma corrida registrada para este labirinto')
  })

  test('filtra por labirinto e mostra número, modo, status e duração das tentativas', () => {
    const ordemOriginal = corridas.map((corrida) => corrida.id)
    render(<MazeHistory labirintos={labirintos} corridas={corridas} />)
    const linhas = screen.getAllByRole('row')
    expect(linhas).toHaveLength(3)
    expect(within(linhas[1]).getByRole('rowheader').textContent).toBe('2')
    expect(linhas[1].textContent).toContain('OtimizadoConcluída12,345 s')
    expect(linhas[2].textContent).toContain('ExploraçãoAbandonada1,400 s')
    expect(screen.queryByRole('button', { name: 'Ver detalhes da tentativa 7' })).toBeNull()
    expect(corridas.map((corrida) => corrida.id)).toEqual(ordemOriginal)
  })

  test('troca a seleção local e informa o ID original ao callback', () => {
    const onSelectLabirinto = vi.fn()
    render(<MazeHistory labirintos={labirintos} corridas={corridas} onSelectLabirinto={onSelectLabirinto} />)
    fireEvent.change(screen.getByRole('combobox'), { target: { value: '2' } })
    expect(onSelectLabirinto).toHaveBeenCalledWith(2)
    expect(screen.getByRole('table').textContent).toContain('Tentativas de Médio')
    expect(screen.getByText('0,000 s')).toBeTruthy()
    expect(screen.queryByText('Abandonada')).toBeNull()
  })

  test('respeita seleção controlada e mudanças de props, inclusive IDs em string', () => {
    const onSelectLabirinto = vi.fn()
    const { rerender } = render(<MazeHistory labirintos={labirintos} corridas={corridas}
      labirintoSelecionado="1" onSelectLabirinto={onSelectLabirinto} />)
    fireEvent.change(screen.getByRole('combobox'), { target: { value: '2' } })
    expect(onSelectLabirinto).toHaveBeenCalledWith(2)
    expect(screen.getByRole('combobox').value).toBe('1')
    rerender(<MazeHistory labirintos={labirintos} corridas={corridas} labirintoSelecionado="2" />)
    expect(screen.getByRole('combobox').value).toBe('2')
    expect(screen.getAllByRole('row')).toHaveLength(2)
  })

  test('integra seleção controlada com o estado do componente pai', () => {
    function HistoricoControlado() {
      const [selecao, setSelecao] = useState(1)
      return <MazeHistory labirintos={labirintos} corridas={corridas}
        labirintoSelecionado={selecao} onSelectLabirinto={setSelecao} />
    }
    render(<HistoricoControlado />)
    fireEvent.change(screen.getByRole('combobox'), { target: { value: '3' } })
    expect(screen.getByText('Labirinto sem registros')).toBeTruthy()
    expect(screen.getByText('Nenhuma corrida registrada para este labirinto')).toBeTruthy()
    expect(screen.queryByRole('table')).toBeNull()
  })

  test('array vazio de corridas exibe o estado obrigatório sem ativar mock', () => {
    render(<MazeHistory labirintos={labirintos} corridas={[]} labirintoSelecionado={1} />)
    expect(screen.getByRole('status').textContent).toContain('Labirinto sem registros')
    expect(screen.getByText('Nenhuma corrida registrada para este labirinto')).toBeTruthy()
    expect(screen.queryByText('Dados de demonstração')).toBeNull()
  })

  test('distingue carregamento, ausência de labirintos e ausência de seleção', () => {
    const { rerender } = render(<MazeHistory labirintos={labirintos} corridas={[]} loading />)
    expect(screen.getByRole('status').textContent).toBe('Carregando histórico…')
    expect(screen.getByRole('combobox').disabled).toBe(true)
    expect(screen.queryByText('Labirinto sem registros')).toBeNull()
    rerender(<MazeHistory labirintos={[]} corridas={[]} />)
    expect(screen.getByRole('status').textContent).toBe('Nenhum labirinto disponível.')
    rerender(<MazeHistory labirintos={labirintos} corridas={corridas} labirintoSelecionado={null} />)
    expect(screen.getByRole('status').textContent).toContain('Selecione um labirinto')
    expect(screen.queryByRole('table')).toBeNull()
  })

  test('abre detalhes no MazeMap com telemetria em ordem e permite fechar', () => {
    const { container } = render(<MazeHistory labirintos={labirintos} corridas={corridas} />)
    const botao = screen.getByRole('button', { name: 'Ver detalhes da tentativa 1' })
    fireEvent.click(botao)
    expect(botao.getAttribute('aria-expanded')).toBe('true')
    expect(screen.getByRole('img').textContent).toContain('2 de 16 células observadas')
    expect(screen.getByRole('img').textContent).toContain('Robô em (0, 1)')
    expect(container.querySelector('[data-trajeto]').getAttribute('d')).toBe('M 20 140 L 20 100')
    expect(corridas[0].leituras[0].tempo_ms).toBe(1400)
    fireEvent.click(screen.getByRole('button', { name: 'Fechar detalhes' }))
    expect(screen.queryByRole('img')).toBeNull()
    expect(botao.getAttribute('aria-expanded')).toBe('false')
  })

  test('usa células, trajeto e posição específicos da tentativa selecionada', () => {
    const { container } = render(<MazeHistory labirintos={labirintos} corridas={corridas} />)
    fireEvent.click(screen.getByRole('button', { name: 'Ver detalhes da tentativa 2' }))
    expect(screen.getByRole('img').textContent).toContain('1 de 16 células observadas')
    expect(screen.getByRole('img').textContent).toContain('Robô em (1, 0), voltado para Leste')
    expect(container.querySelector('[data-trajeto]').getAttribute('d')).toBe('M 60 140')
    fireEvent.change(screen.getByRole('combobox'), { target: { value: '3' } })
    expect(screen.queryByRole('img')).toBeNull()
    expect(screen.getByText('Labirinto sem registros')).toBeTruthy()
  })

  test('não exibe mapa fictício quando o resumo não contém detalhes', () => {
    render(<MazeHistory labirintos={labirintos} corridas={corridas} labirintoSelecionado={2} />)
    fireEvent.click(screen.getByRole('button', { name: 'Ver detalhes da tentativa 7' }))
    expect(screen.getByRole('status').textContent).toContain('Mapa e trajeto indisponíveis')
    expect(screen.queryByRole('img')).toBeNull()
  })

  test('atualiza detalhes recebidos por props e respeita dimensões do labirinto', () => {
    const props = { labirintos, corridas, labirintoSelecionado: 2 }
    const { rerender } = render(<MazeHistory {...props} />)
    fireEvent.click(screen.getByRole('button', { name: 'Ver detalhes da tentativa 7' }))
    rerender(<MazeHistory {...props} corridas={corridas.map((corrida) => corrida.id === 20
      ? { ...corrida, trajeto: [{ pos_x: 0, pos_y: 0 }] } : corrida)} />)
    expect(screen.getByRole('img').textContent).toContain('Labirinto de 8 colunas por 4 linhas')
    rerender(<MazeHistory {...props} corridas={[]} />)
    expect(screen.queryByRole('img')).toBeNull()
    expect(screen.getByText('Nenhuma corrida registrada para este labirinto')).toBeTruthy()
  })

  test('mantém campos ausentes explícitos sem inventar modo, status ou tempo', () => {
    render(<MazeHistory labirintos={labirintos} corridas={[
      { id: 30, id_labirinto: 1, num_tentativa: 3, status: 'em_andamento' },
    ]} />)
    expect(screen.getByText('Em andamento')).toBeTruthy()
    expect(screen.getAllByText('Não informado')).toHaveLength(2)
  })
})
