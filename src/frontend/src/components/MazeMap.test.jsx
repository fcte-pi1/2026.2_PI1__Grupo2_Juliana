import { StrictMode } from 'react'
import { cleanup, render, screen } from '@testing-library/react'
import { afterEach, describe, expect, test } from 'vitest'
import MazeMap from './MazeMap.jsx'

afterEach(cleanup)

describe('MazeMap', () => {
  test.each([4, 8, 12])('renderiza %ix4 sem objetivo fixo pré-demarcado', (largura) => {
    const { container } = render(<MazeMap largura={largura} altura={4} />)
    expect(screen.getByRole('img').getAttribute('viewBox')).toBe(`0 0 ${largura * 40 + 40} 200`)
    expect(container.querySelectorAll('[data-celula]').length).toBe(largura * 4)
    expect(container.querySelector('[data-objetivo]')).toBeNull()
    expect(container.querySelector('[data-ultima-celula-marcador]')).toBeNull()
    expect(screen.getByRole('img').textContent).not.toContain('Objetivo em')
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

  test('desenha apenas o trajeto recebido, preservando ordem e revisitas sem alterar a prop', () => {
    const trajeto = Object.freeze([
      Object.freeze({ pos_x: 0, pos_y: 0 }),
      Object.freeze({ pos_x: 1, pos_y: 0 }),
      Object.freeze({ pos_x: 0, pos_y: 0 }),
      Object.freeze({ pos_x: 0, pos_y: 1 }),
    ])
    const { container, rerender } = render(<MazeMap trajeto={trajeto} />)
    const caminho = () => container.querySelector('[data-trajeto]')?.getAttribute('d')
    expect(caminho()).toBe('M 20 140 L 60 140 L 20 140 L 20 100')

    // Mudanças na posição do robô e nas células não acrescentam nem removem pontos.
    rerender(<MazeMap trajeto={trajeto}
      roboPos={{ pos_x: 2, pos_y: 2, orientacao: 'N' }}
      celulas={{ '2,2': { pos_x: 2, pos_y: 2, paredes: 9 } }}
    />)
    expect(caminho()).toBe('M 20 140 L 60 140 L 20 140 L 20 100')
    rerender(<MazeMap trajeto={trajeto} celulas={{}} />)
    expect(caminho()).toBe('M 20 140 L 60 140 L 20 140 L 20 100')

    rerender(<MazeMap trajeto={[{ pos_x: 3, pos_y: 3 }]} />)
    expect(caminho()).toBe('M 140 20')
    rerender(<MazeMap trajeto={[]} />)
    expect(caminho()).toBeUndefined()
    expect(screen.getByText('Trajeto')).toBeTruthy()
    expect(screen.getByText('Saída (última célula)')).toBeTruthy()
  })

  test('destaca a penúltima coordenada como última célula interna quando o robô sai do labirinto', () => {
    const { container, rerender } = render(
      <MazeMap
        largura={4}
        altura={4}
        trajeto={[
          { pos_x: 0, pos_y: 0 },
          { pos_x: 3, pos_y: 3 },
          { pos_x: 4, pos_y: 3 },
        ]}
        roboPos={{ pos_x: 4, pos_y: 3, orientacao: 'L' }}
      />,
    )

    const marcador = container.querySelector('[data-ultima-celula-marcador="3,3"]')
    expect(marcador).toBeTruthy()
    expect(marcador.getAttribute('data-saida')).toBe('4,3')
    expect(marcador.getAttribute('x')).toBe(String(3 * 40 + 5))
    expect(marcador.getAttribute('y')).toBe('5')
    expect(container.querySelector('[data-celula="3,3"].maze-map__cell--exit')).toBeTruthy()
    expect(container.querySelector('[data-celula="3,3"]').getAttribute('data-ultima-celula')).toBe('true')
    expect(screen.getByTestId('status-saida-mapa').textContent).toContain('Saída em (4, 3)')
    expect(screen.getByTestId('status-saida-mapa').textContent).toContain('Última célula (3, 3)')

    // Também funciona quando a coordenada externa está apenas em roboPos e o último ponto do trajeto é interno
    rerender(
      <MazeMap
        largura={4}
        altura={4}
        trajeto={[{ pos_x: 0, pos_y: 0 }, { pos_x: 0, pos_y: 1 }]}
        roboPos={{ pos_x: -1, pos_y: 1, orientacao: 'O' }}
      />,
    )
    expect(container.querySelector('[data-ultima-celula-marcador="0,1"]')).toBeTruthy()
    expect(screen.getByTestId('status-saida-mapa').textContent).toContain('Saída em (-1, 1)')
  })

  test('restaura todo o trajeto recebido ao remontar o componente em StrictMode', () => {
    const trajeto = [{ pos_x: 0, pos_y: 0 }, { pos_x: 0, pos_y: 1 }, { pos_x: 1, pos_y: 1 }]
    const montar = () => render(<StrictMode><MazeMap trajeto={trajeto} /></StrictMode>)
    const inicial = montar()
    const caminho = inicial.container.querySelector('[data-trajeto]').getAttribute('d')
    expect(caminho).toBe('M 20 140 L 20 100 L 60 100')
    inicial.unmount()
    const retorno = montar()
    expect(retorno.container.querySelector('[data-trajeto]').getAttribute('d')).toBe(caminho)
  })

  test('interrompe a linha em pontos inválidos ou fora dos limites do labirinto', () => {
    const { container } = render(<MazeMap largura={12} trajeto={[
      { pos_x: 0, pos_y: 0 }, { pos_x: 1, pos_y: 0 },
      { pos_x: 12, pos_y: 0 }, { pos_x: 11, pos_y: 3 },
      { pos_x: 0, pos_y: 4 }, { pos_x: 0, pos_y: 1 },
      { pos_x: -1, pos_y: 0 }, { pos_x: 0.5, pos_y: 0 },
      { pos_x: 0, pos_y: NaN }, { x: 1, y: 1 }, null,
      { pos_x: 0, pos_y: 2 },
    ]} />)
    expect(container.querySelector('[data-trajeto]').getAttribute('d'))
      .toBe('M 20 140 L 60 140 M 460 20 M 20 100 M 20 60')
  })

  test('não cria trajeto a partir do robô quando a prop é omitida', () => {
    const { container } = render(<MazeMap
      roboPos={{ pos_x: 1, pos_y: 1, orientacao: 'N' }}
      celulas={{ '1,1': { pos_x: 1, pos_y: 1, paredes: 9 } }}
    />)
    expect(container.querySelector('[data-robo]')).toBeTruthy()
    expect(container.querySelector('[data-trajeto]')).toBeNull()
  })
})
