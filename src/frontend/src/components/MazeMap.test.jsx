import { StrictMode } from 'react'
import { cleanup, render, screen } from '@testing-library/react'
import { afterEach, describe, expect, test } from 'vitest'
import MazeMap from './MazeMap.jsx'

afterEach(cleanup)

describe('MazeMap', () => {
  test.each([4, 8, 12])('renderiza %ix4 com objetivo no canto oposto', (largura) => {
    const { container } = render(<MazeMap largura={largura} altura={4} />)
    expect(screen.getByRole('img').getAttribute('viewBox')).toBe(`0 0 ${largura * 40 + 40} 200`)
    expect(container.querySelectorAll('[data-celula]').length).toBe(largura * 4)
    const objetivo = container.querySelector(`[data-objetivo="${largura - 1},3"]`)
    expect(objetivo.getAttribute('x')).toBe(String((largura - 1) * 40 + 5))
    expect(objetivo.getAttribute('y')).toBe('5')
    expect(screen.getByRole('img').textContent).toContain(`Objetivo em (${largura - 1}, 3)`)
  })

  test.each([
    [0, 4], [-1, 4], [4.5, 4], [NaN, 4], [Infinity, 4], ['4', 4], [null, 4],
    [5, 4], [4, 0], [4, -1], [4, 4.5], [4, NaN], [4, Infinity], [4, '4'],
    [4, null], [4, 8], [4, 12], [12, 8],
  ])('rejeita dimensões fora do TAP: %sx%s', (largura, altura) => {
    render(<MazeMap largura={largura} altura={altura} />)
    expect(screen.getByRole('status').textContent).toContain('4×4, 8×4 ou 12×4')
    expect(screen.queryByRole('img')).toBeNull()
  })

  test('ignora células fora do mapa, coordenadas inválidas e máscaras inválidas', () => {
    const { container } = render(<MazeMap celulas={[
      { pos_x: 0, pos_y: 0, paredes: 15 },
      { pos_x: 4, pos_y: 0, paredes: 15 },
      { pos_x: 0, pos_y: 4, paredes: 15 },
      { pos_x: -1, pos_y: 0, paredes: 15 },
      { pos_x: 0, pos_y: -1, paredes: 15 },
      { pos_x: 0.5, pos_y: 0, paredes: 15 },
      { pos_x: 0, pos_y: 0.5, paredes: 15 },
      { pos_x: 1, pos_y: 0, paredes: -1 },
      { pos_x: 1, pos_y: 1, paredes: 16 },
      { pos_x: 1, pos_y: 2, paredes: 1.5 },
      { pos_x: 1, pos_y: 3, paredes: '1' },
      null,
    ]} />)
    expect(screen.getByRole('img').textContent).toContain('1 de 16 células observadas')
    expect(container.querySelectorAll('.maze-map__cell--observed').length).toBe(1)
    expect([...container.querySelectorAll('[data-parede]')].map((p) => p.dataset.parede))
      .toEqual(['Norte', 'Leste', 'Sul', 'Oeste'])
    expect(container.querySelector('[data-celula="0,0"]').getAttribute('y')).toBe('120')
  })

  test('aceita células indexadas por coordenada e mantém a leitura mais recente', () => {
    const { container, rerender } = render(<MazeMap celulas={{
      '0,0': 1, '1,2': { paredes: 2 }, '2,3': { pos_x: 2, pos_y: 3, paredes: 0 },
    }} />)
    expect(screen.getByRole('img').textContent).toContain('3 de 16 células observadas')
    expect(container.querySelectorAll('[data-parede]').length).toBe(2)
    rerender(<MazeMap celulas={[
      { pos_x: 0, pos_y: 0, paredes: 15 }, { pos_x: 0, pos_y: 0, paredes: 0 },
    ]} />)
    expect(screen.getByRole('img').textContent).toContain('1 de 16 células observadas')
    expect(container.querySelector('[data-parede]')).toBeNull()
    rerender(<MazeMap celulas={null} />)
    expect(screen.getByRole('img').textContent).toContain('0 de 16 células observadas')
  })

  test.each([
    ['N', 0, 'Norte'], ['L', 90, 'Leste'], ['S', 180, 'Sul'], ['O', 270, 'Oeste'],
  ])('posiciona o robô com orientação %s', (orientacao, angulo, nome) => {
    const { container } = render(<MazeMap roboPos={{ pos_x: 2, pos_y: 1, orientacao }} />)
    expect(container.querySelector('[data-robo]').getAttribute('transform'))
      .toBe(`translate(100 100) rotate(${angulo})`)
    expect(screen.getByRole('img').textContent).toContain(`Robô em (2, 1), voltado para ${nome}`)
  })

  test.each([
    null, { pos_x: 4, pos_y: 0, orientacao: 'N' }, { pos_x: 0, pos_y: 4, orientacao: 'N' },
    { pos_x: 0, pos_y: 0, orientacao: '?' }, { x: 1, y: 1, orientacao: 'N' },
  ])('omite o robô com posição ou orientação inválida: %j', (roboPos) => {
    const { container } = render(<MazeMap roboPos={roboPos} />)
    expect(container.querySelector('[data-robo]')).toBeNull()
    expect(screen.getByRole('img').textContent).toContain('Sem posição válida do robô')
  })

  test('usa apenas pos_x/pos_y nos objetos de telemetria', () => {
    const { container } = render(<MazeMap
      celulas={[{ x: 0, y: 0, paredes: 15 }]}
      roboPos={{ pos_x: 2, pos_y: 1, x: 0, y: 0, orientacao: 'N' }}
    />)
    expect(container.querySelector('.maze-map__cell--observed')).toBeNull()
    expect(container.querySelector('[data-robo]').getAttribute('transform')).toContain('translate(100 100)')
  })

  test('acumula posições sem duplicar pacotes, preserva revisitas e reinicia com o mapa', () => {
    const celulas = [{ pos_x: 0, pos_y: 0, paredes: 0 }]
    const props = { celulas, roboPos: { pos_x: 0, pos_y: 0, orientacao: 'N' } }
    const { container, rerender } = render(<StrictMode><MazeMap {...props} /></StrictMode>)
    const atualizar = (pos_x, pos_y, extras = {}) => rerender(
      <StrictMode><MazeMap {...props} roboPos={{ pos_x, pos_y, orientacao: 'N' }} {...extras} /></StrictMode>,
    )
    const caminho = () => container.querySelector('[data-trajeto]')?.getAttribute('d')
    expect(caminho()).toBe('M 20 140')
    atualizar(0, 0)
    expect(caminho()).toBe('M 20 140')
    atualizar(1, 0)
    atualizar(0, 0)
    expect(caminho()).toBe('M 20 140 L 60 140 L 20 140')
    atualizar(12, 0)
    atualizar(13, 0)
    atualizar(0, 1)
    expect(caminho()).toBe('M 20 140 L 60 140 L 20 140 M 20 100')
    atualizar(0, 1, { celulas: [] })
    expect(caminho()).toBeUndefined()
    atualizar(0, 1, { celulas: [] })
    atualizar(0, 0)
    expect(caminho()).toBe('M 20 140')
    atualizar(7, 3, { largura: 8 })
    expect(caminho()).toBe('M 300 20')
    atualizar(7, 3, { largura: 5 })
    expect(screen.getByRole('status')).toBeTruthy()
    atualizar(7, 3, { largura: 8 })
    expect(caminho()).toBe('M 300 20')
    expect(screen.getByText('Trajeto')).toBeTruthy()
    expect(screen.getByText('Objetivo')).toBeTruthy()
  })
})
