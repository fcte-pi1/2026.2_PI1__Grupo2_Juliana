import { useId, useState } from 'react'
import MazeMap from './MazeMap.jsx'
import './MazeHistory.css'

const LABIRINTOS_DEMO = [
  { id: 1, nome: 'Labirinto 4 × 4', largura_celula: 4, altura_celula: 4 },
  { id: 2, nome: 'Labirinto 8 × 4', largura_celula: 8, altura_celula: 4 },
]
const CORRIDAS_DEMO = [
  {
    id: 1, id_labirinto: 1, num_tentativa: 1, modo: 'exploracao',
    status: 'abandonada', duracao_s: 23.4,
    leituras: [{ pos_x: 0, pos_y: 0, paredes: 11, orientacao: 'N', tempo_ms: 0 }],
  },
  {
    id: 2, id_labirinto: 1, num_tentativa: 2, modo: 'otimizado',
    status: 'concluida', duracao_s: 12.345,
    leituras: [
      { pos_x: 0, pos_y: 0, paredes: 11, orientacao: 'N', tempo_ms: 0 },
      { pos_x: 0, pos_y: 1, paredes: 9, orientacao: 'N', tempo_ms: 400 },
    ],
  },
]
const MODOS = { exploracao: 'Exploração', explorando: 'Exploração', otimizado: 'Otimizado' }
const STATUS = { concluida: 'Concluída', abandonada: 'Abandonada', em_andamento: 'Em andamento' }
const idIgual = (a, b) => a != null && b != null && String(a) === String(b)

function formatarTempo(corrida) {
  const valor = corrida.duracao_s ?? corrida.tempo_conc_s
  if (valor == null || valor === '') return 'Não informado'
  const segundos = Number(valor)
  return Number.isFinite(segundos) && segundos >= 0
    ? `${segundos.toLocaleString('pt-BR', { minimumFractionDigits: 3, maximumFractionDigits: 3 })} s`
    : 'Não informado'
}

/**
 * Sem props de dados, usa uma demonstração local; arrays vazios nunca ativam o mock.
 * labirintoSelecionado é o ID controlado (null significa sem seleção).
 * Sem essa prop, a seleção é local. onSelectLabirinto recebe o ID original.
 * Corridas seguem o resumo da API e podem incluir modo, celulas, trajeto,
 * roboPos e/ou leituras do detalhe. O componente não faz requisições.
 */
export default function MazeHistory({
  labirintos,
  corridas,
  labirintoSelecionado,
  onSelectLabirinto,
  loading = false,
}) {
  const id = useId()
  const demonstracao = labirintos === undefined && corridas === undefined
  const listaLabirintos = labirintos ?? (demonstracao ? LABIRINTOS_DEMO : [])
  const listaCorridas = corridas ?? (demonstracao ? CORRIDAS_DEMO : [])
  const [selecaoLocal, setSelecaoLocal] = useState(undefined)
  const [detalhe, setDetalhe] = useState(null)
  const selecao = labirintoSelecionado !== undefined
    ? labirintoSelecionado
    : selecaoLocal === undefined ? listaLabirintos[0]?.id : selecaoLocal
  const labirinto = listaLabirintos.find((item) => idIgual(item.id, selecao))
  const tentativas = listaCorridas
    .filter((corrida) => labirinto && idIgual(corrida.id_labirinto, labirinto.id))
    .sort((a, b) => b.num_tentativa - a.num_tentativa)
  const corridaDetalhada = idIgual(detalhe?.labirintoId, labirinto?.id)
    ? tentativas.find((corrida) => idIgual(corrida.id, detalhe?.corridaId))
    : null
  const leituras = [...(corridaDetalhada?.leituras ?? [])].sort((a, b) => a.tempo_ms - b.tempo_ms)
  const temMapa = corridaDetalhada && (
    corridaDetalhada.celulas != null || corridaDetalhada.trajeto != null || leituras.length > 0
  )

  function selecionar(event) {
    const proximo = listaLabirintos.find((item) => String(item.id) === event.target.value)
    setSelecaoLocal(proximo?.id ?? null)
    setDetalhe(null)
    onSelectLabirinto?.(proximo?.id ?? null)
  }

  return (
    <section className="maze-history" aria-labelledby={`${id}-titulo`} aria-busy={loading}>
      <header className="maze-history__header">
        <div>
          <h2 id={`${id}-titulo`}>Histórico por labirinto</h2>
          <p>Consulte as tentativas e o percurso de cada corrida.</p>
          {demonstracao && <small>Dados de demonstração</small>}
        </div>
        <div className="maze-history__filter">
          <label htmlFor={`${id}-labirinto`}>Labirinto</label>
          <select id={`${id}-labirinto`} value={labirinto?.id ?? ''} onChange={selecionar}
            disabled={loading || listaLabirintos.length === 0}>
            <option value="">Selecione um labirinto</option>
            {listaLabirintos.map((item) => <option key={item.id} value={item.id}>{item.nome}</option>)}
          </select>
        </div>
      </header>

      {loading ? <p className="maze-history__empty" role="status">Carregando histórico…</p>
        : !labirinto ? (
          <p className="maze-history__empty" role="status">
            {listaLabirintos.length === 0 ? 'Nenhum labirinto disponível.' : 'Selecione um labirinto para visualizar o histórico.'}
          </p>
        ) : tentativas.length === 0 ? (
          <div className="maze-history__empty" role="status">
            <strong>Labirinto sem registros</strong>
            <p>Nenhuma corrida registrada para este labirinto</p>
          </div>
        ) : (
          <>
            <div className="maze-history__table-wrapper">
              <table className="maze-history__table">
                <caption>Tentativas de {labirinto.nome}</caption>
                <thead><tr>
                  <th scope="col">Nº da tentativa</th><th scope="col">Modo</th>
                  <th scope="col">Status</th><th scope="col">Tempo</th><th scope="col">Detalhes</th>
                </tr></thead>
                <tbody>{tentativas.map((corrida) => (
                  <tr key={corrida.id}>
                    <th scope="row">{corrida.num_tentativa}</th>
                    <td>{MODOS[corrida.modo] ?? 'Não informado'}</td>
                    <td><span className={`maze-history__status maze-history__status--${corrida.status}`}>
                      {STATUS[corrida.status] ?? 'Não informado'}
                    </span></td>
                    <td>{formatarTempo(corrida)}</td>
                    <td><button type="button" aria-expanded={corridaDetalhada?.id === corrida.id}
                      aria-controls={`${id}-detalhes`}
                      aria-label={`Ver detalhes da tentativa ${corrida.num_tentativa}`}
                      onClick={() => setDetalhe({ labirintoId: labirinto.id, corridaId: corrida.id })}>
                      Ver detalhes
                    </button></td>
                  </tr>
                ))}</tbody>
              </table>
            </div>
            {corridaDetalhada && (
              <section id={`${id}-detalhes`} className="maze-history__details" aria-labelledby={`${id}-tentativa`}>
                <h3 id={`${id}-tentativa`}>Detalhes da tentativa {corridaDetalhada.num_tentativa}</h3>
                {temMapa ? <MazeMap
                  largura={labirinto.largura_celula ?? labirinto.largura ?? 4}
                  altura={labirinto.altura_celula ?? labirinto.altura ?? 4}
                  celulas={corridaDetalhada.celulas ?? leituras}
                  trajeto={corridaDetalhada.trajeto ?? leituras}
                  roboPos={corridaDetalhada.roboPos ?? leituras.at(-1) ?? null}
                  ultimaCelula={corridaDetalhada.ultimaCelula}
                  pontoSaida={corridaDetalhada.pontoSaida}
                /> : <p role="status">Mapa e trajeto indisponíveis para esta tentativa.</p>}
                <button type="button" onClick={() => setDetalhe(null)}>Fechar detalhes</button>
              </section>
            )}
          </>
        )}
    </section>
  )
}
