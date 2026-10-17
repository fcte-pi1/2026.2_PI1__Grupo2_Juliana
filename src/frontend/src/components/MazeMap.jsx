import { useId } from 'react'
import './MazeMap.css'

const TAMANHO_CELULA = 40
const MARGEM = 20
const DIRECOES = {
  N: { angulo: 0, nome: 'Norte' },
  L: { angulo: 90, nome: 'Leste' },
  S: { angulo: 180, nome: 'Sul' },
  O: { angulo: 270, nome: 'Oeste' },
}
const PAREDES = [
  { bit: 1, nome: 'Norte', x1: 0, y1: 0, x2: 40, y2: 0 },
  { bit: 2, nome: 'Leste', x1: 40, y1: 0, x2: 40, y2: 40 },
  { bit: 4, nome: 'Sul', x1: 0, y1: 40, x2: 40, y2: 40 },
  { bit: 8, nome: 'Oeste', x1: 0, y1: 0, x2: 0, y2: 40 },
]

export default function MazeMap({
  largura = 4,
  altura = 4,
  celulas = [],
  roboPos = null,
  trajeto = [],
  className = '',
  style,
}) {
  const id = useId()

  if (![4, 8, 12].includes(largura) || altura !== 4) {
    return <p role="status">Informe um tamanho previsto no TAP: 4×4, 8×4 ou 12×4 (largura × altura).</p>
  }

  const dentroDoMapa = (x, y) => (
    Number.isInteger(x) && Number.isInteger(y)
    && x >= 0 && x < largura && y >= 0 && y < altura
  )
  const mapa = new Map()
  const entradas = Array.isArray(celulas)
    ? celulas.map((celula) => [null, celula])
    : Object.entries(celulas ?? {})

  for (const [chave, celula] of entradas) {
    const coordenadas = chave === null ? [] : chave.split(',').map(Number)
    const x = celula?.pos_x ?? coordenadas[0]
    const y = celula?.pos_y ?? coordenadas[1]
    const paredes = typeof celula === 'number' ? celula : celula?.paredes

    if (dentroDoMapa(x, y) && Number.isInteger(paredes) && paredes >= 0 && paredes <= 15) {
      mapa.set(`${x},${y}`, paredes)
    }
  }

  const roboX = roboPos?.pos_x
  const roboY = roboPos?.pos_y
  const posicaoValida = dentroDoMapa(roboX, roboY)
  const direcao = DIRECOES[roboPos?.orientacao]
  const mostrarRobo = posicaoValida && Boolean(direcao)
  // A Dashboard mantém o histórico; posições inválidas apenas interrompem a linha.
  const caminho = trajeto.map((ponto, indice) => {
    if (!dentroDoMapa(ponto?.pos_x, ponto?.pos_y)) return ''
    const anterior = trajeto[indice - 1]
    const comando = dentroDoMapa(anterior?.pos_x, anterior?.pos_y) ? 'L' : 'M'
    return `${comando} ${(ponto.pos_x + 0.5) * TAMANHO_CELULA} ${(altura - ponto.pos_y - 0.5) * TAMANHO_CELULA}`
  }).filter(Boolean).join(' ')
  const posicao = mostrarRobo ? `Robô em (${roboX}, ${roboY}), voltado para ${direcao.nome}.` : 'Sem posição válida do robô.'
  const larguraMapa = largura * TAMANHO_CELULA
  const alturaMapa = altura * TAMANHO_CELULA
  const linhas = Array.from({ length: altura }, (_, linha) => altura - 1 - linha)
  const colunas = Array.from({ length: largura }, (_, x) => x)

  return (
    <figure className={`maze-map ${className}`.trim()} style={style}>
      <svg
        className="maze-map__canvas"
        viewBox={`0 0 ${larguraMapa + MARGEM * 2} ${alturaMapa + MARGEM * 2}`}
        width={larguraMapa + MARGEM * 2}
        height={alturaMapa + MARGEM * 2}
        role="img"
        aria-labelledby={`${id}-titulo ${id}-descricao`}
      >
        <title id={`${id}-titulo`}>Labirinto de {largura} colunas por {altura} linhas</title>
        <desc id={`${id}-descricao`}>
          Origem (0, 0) no canto inferior esquerdo. Norte para cima. {mapa.size} de {largura * altura} células observadas. Objetivo em ({largura - 1}, {altura - 1}). {posicao}
        </desc>

        {colunas.map((x) => (
          <text key={`x-${x}`} className="maze-map__axis" x={MARGEM + (x + 0.5) * TAMANHO_CELULA} y={10}>
            {x}
          </text>
        ))}
        {linhas.map((y, linha) => (
          <text key={`y-${y}`} className="maze-map__axis" x={8} y={MARGEM + (linha + 0.5) * TAMANHO_CELULA}>
            {y}
          </text>
        ))}

        {/* Desenha todas as bases antes das paredes para não encobrir limites compartilhados. */}
        <g transform={`translate(${MARGEM} ${MARGEM})`}>
          {linhas.flatMap((y, linha) => colunas.map((x) => (
            <rect
              key={`${x},${y}`}
              data-celula={`${x},${y}`}
              className={`maze-map__cell${mapa.has(`${x},${y}`) ? ' maze-map__cell--observed' : ''}`}
              x={x * TAMANHO_CELULA}
              y={linha * TAMANHO_CELULA}
              width={TAMANHO_CELULA}
              height={TAMANHO_CELULA}
            >
              <title>Célula ({x}, {y}): {mapa.has(`${x},${y}`) ? `máscara ${mapa.get(`${x},${y}`)}` : 'não observada'}</title>
            </rect>
          )))}

          <rect
            className="maze-map__goal"
            data-objetivo={`${largura - 1},${altura - 1}`}
            x={(largura - 1) * TAMANHO_CELULA + 5}
            y={5}
            width={TAMANHO_CELULA - 10}
            height={TAMANHO_CELULA - 10}
            rx={4}
          >
            <title>Objetivo ({largura - 1}, {altura - 1})</title>
          </rect>

          {caminho && (
            <path className="maze-map__trail" data-trajeto="true" d={caminho}>
              <title>Trajeto percorrido pelo robô</title>
            </path>
          )}

          {linhas.flatMap((y, linha) => colunas.map((x) => (
            <g key={`${x},${y}`} transform={`translate(${x * TAMANHO_CELULA} ${linha * TAMANHO_CELULA})`}>
              {PAREDES.filter(({ bit }) => ((mapa.get(`${x},${y}`) ?? 0) & bit) !== 0).map(({ bit, nome, ...pontos }) => (
                <line key={bit} className="maze-map__wall" data-parede={nome} {...pontos} />
              ))}
            </g>
          )))}

          {mostrarRobo && (
            <g
              className="maze-map__robot"
              data-robo="true"
              transform={`translate(${(roboX + 0.5) * TAMANHO_CELULA} ${(altura - roboY - 0.5) * TAMANHO_CELULA}) rotate(${direcao.angulo})`}
            >
              <title>{posicao}</title>
              <circle r={12} />
              <path d="M 0 -9 L 7 7 L 0 3 L -7 7 Z" />
            </g>
          )}
        </g>
      </svg>

      <figcaption className="maze-map__caption">
        <span>N ↑ · Origem (0, 0) no canto inferior esquerdo</span>
        <span>{posicao}</span>
        <span className="maze-map__legend">
          <span><i className="maze-map__key maze-map__key--observed" aria-hidden="true" />Observada</span>
          <span><i className="maze-map__key" aria-hidden="true" />Não observada</span>
          <span><i className="maze-map__key maze-map__key--wall" aria-hidden="true" />Parede confirmada</span>
          <span><i className="maze-map__key maze-map__key--trail" aria-hidden="true" />Trajeto</span>
          <span><i className="maze-map__key maze-map__key--goal" aria-hidden="true" />Objetivo</span>
        </span>
      </figcaption>
    </figure>
  )
}
