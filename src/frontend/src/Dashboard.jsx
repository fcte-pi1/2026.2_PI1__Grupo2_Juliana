import { useEffect, useRef, useState } from 'react'
import MazeMap from './components/MazeMap.jsx'

const LIMIAR_BATERIA_CRITICA_V = 7.0
const LIMIAR_REARME_BATERIA_V = 7.2
const TENSAO_MIN_V = 6.4
const TENSAO_MAX_V = 8.4
const TIMEOUT_OFFLINE_MS = 3000

const LABIRINTOS_OFICIAIS = [
  { largura: 4, altura: 4 },
  { largura: 8, altura: 4 },
  { largura: 12, altura: 4 },
]

const NOMES_ORIENTACAO = {
  N: { nome: 'Norte', seta: '↑' },
  S: { nome: 'Sul', seta: '↓' },
  L: { nome: 'Leste', seta: '→' },
  O: { nome: 'Oeste', seta: '←' },
}

const TEXTOS_ESTADO_EXECUCAO = {
  explorando: {
    rotulo: 'Explorando labirinto',
    resumo: 'Explorando',
    cor: 'ambar',
  },
  retornando: {
    rotulo: 'Retornando ao início',
    resumo: 'Retornando',
    cor: 'azul',
  },
  rapido: {
    rotulo: 'Corrida rápida',
    resumo: 'Rápido',
    cor: 'verde',
  },
  parado: {
    rotulo: 'Parado',
    resumo: 'Parado',
    cor: 'neutro',
  },
}

const CHAVE_STORAGE_LABIRINTO = 'micromouse_tipo_labirinto'

/**
 * Verifica se o par (largura, altura) corresponde a um dos 3 labirintos oficiais: 4x4, 8x4 ou 12x4.
 */
function ehLabirintoOficial(largura, altura) {
  return LABIRINTOS_OFICIAIS.some((dim) => dim.largura === largura && dim.altura === altura)
}

/**
 * Carrega as dimensões do labirinto salvas no localStorage ('4x4', '8x4' ou '12x4').
 */
function lerDimensoesSalvas() {
  const valorSalvo = window.localStorage.getItem(CHAVE_STORAGE_LABIRINTO)
  if (!valorSalvo) {
    return null
  }
  const partes = valorSalvo
    .toLowerCase()
    .split('x')
    .map((p) => Number.parseInt(p.trim(), 10))
  if (partes.length === 2 && ehLabirintoOficial(partes[0], partes[1])) {
    return { largura: partes[0], altura: partes[1] }
  }
  return null
}

/**
 * Extrai dimensões do labirinto exclusivamente dos campos largura_celula e altura_celula
 * de qualquer pacote/evento caso correspondam a um dos 3 tamanhos oficiais (4x4, 8x4, 12x4).
 */
function resolverDimensoesLabirinto(dados, dimensoesAtuais) {
  if (
    typeof dados.largura_celula === 'number' &&
    typeof dados.altura_celula === 'number' &&
    ehLabirintoOficial(dados.largura_celula, dados.altura_celula)
  ) {
    return { largura: dados.largura_celula, altura: dados.altura_celula }
  }
  return dimensoesAtuais
}

/**
 * Calcula o checksum CRC-16/CCITT-FALSE (polinômio 0x1021, inicial 0xFFFF,
 * sem reflexão, XOR final 0) sobre os bytes UTF-8 do texto JSON,
 * exatamente conforme a seção 4 do Contrato de Telemetria (4.4.7) e simulador.py.
 */
function calcularCrc16Ccitt(texto) {
  const bytes = new TextEncoder().encode(texto)
  let crc = 0xffff
  for (let i = 0; i < bytes.length; i += 1) {
    crc ^= bytes[i] << 8
    for (let b = 0; b < 8; b += 1) {
      crc = (crc & 0x8000) !== 0 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff
    }
  }
  return crc.toString(16).toUpperCase().padStart(4, '0')
}

/**
 * Interpreta e valida um frame recebido via WebSocket.
 * Aceita tanto o formato bruto <JSON>|<CRC> (do simulador.py / robô)
 * quanto JSON puro (retransmitido pelo endpoint /ws/painel do backend).
 */
function interpretarFrameTelemetria(textoBruto) {
  if (typeof textoBruto !== 'string') {
    return { valido: false, motivo: 'formato invalido' }
  }

  const texto = textoBruto.trim()
  if (!texto) {
    return { valido: false, motivo: 'formato invalido' }
  }

  let trechoJson = texto

  const indiceSeparador = texto.lastIndexOf('|')
  if (indiceSeparador !== -1) {
    const sufixoCrc = texto.slice(indiceSeparador + 1)
    if (!/^[0-9A-Fa-f]{4}$/.test(sufixoCrc)) {
      return { valido: false, motivo: 'formato invalido' }
    }
    trechoJson = texto.slice(0, indiceSeparador)
    const crcCalculado = calcularCrc16Ccitt(trechoJson)
    if (crcCalculado !== sufixoCrc.toUpperCase()) {
      return {
        valido: false,
        motivo: `checksum invalido (recebido ${sufixoCrc.toUpperCase()}, calculado ${crcCalculado})`,
      }
    }
  }

  let dados
  try {
    dados = JSON.parse(trechoJson)
  } catch {
    return { valido: false, motivo: 'json invalido' }
  }

  if (!dados || typeof dados !== 'object') {
    return { valido: false, motivo: 'json invalido' }
  }

  if (dados.v !== undefined && dados.v !== 1) {
    return { valido: false, motivo: 'versao nao suportada' }
  }

  if (dados.t !== 'tel' && dados.t !== 'evt') {
    return { valido: false, motivo: 'tipo de mensagem desconhecido' }
  }

  if (typeof dados.tempo_ms !== 'number' || dados.tempo_ms < 0) {
    return { valido: false, motivo: 'campo invalido: tempo_ms' }
  }

  if (dados.t === 'tel') {
    const camposNumericos = ['pos_x', 'pos_y', 'paredes', 'velocidade', 'tensao_bateria']
    for (const campo of camposNumericos) {
      if (typeof dados[campo] !== 'number' || Number.isNaN(dados[campo])) {
        return { valido: false, motivo: `campo ausente ou invalido: ${campo}` }
      }
    }
    if (dados.paredes < 0 || dados.paredes > 15 || !Number.isInteger(dados.paredes)) {
      return { valido: false, motivo: `fora do intervalo: paredes=${dados.paredes}` }
    }
    if (dados.pos_x < 0 || dados.pos_y < 0) {
      return { valido: false, motivo: 'fora do intervalo: coordenadas negativas' }
    }
    if (!NOMES_ORIENTACAO[dados.orientacao]) {
      return { valido: false, motivo: `orientacao invalida: ${dados.orientacao}` }
    }
    if (!TEXTOS_ESTADO_EXECUCAO[dados.estado]) {
      return { valido: false, motivo: `estado invalido: ${dados.estado}` }
    }
  }

  return { valido: true, dados }
}

/**
 * Converte a tensão da bateria LiPo 2S (6,4 V a 8,4 V) em porcentagem (0% a 100%).
 */
function calcularPorcentagemBateria(tensaoV) {
  if (typeof tensaoV !== 'number' || Number.isNaN(tensaoV)) {
    return 0
  }
  const proporcao = ((tensaoV - TENSAO_MIN_V) / (TENSAO_MAX_V - TENSAO_MIN_V)) * 100
  return Math.max(0, Math.min(100, Math.round(proporcao)))
}

/**
 * Formata milissegundos em MM:SS.
 */
function formatarCronometro(tempoMs) {
  const totalSegundos = Math.max(0, Math.floor((tempoMs || 0) / 1000))
  const minutos = String(Math.floor(totalSegundos / 60)).padStart(2, '0')
  const segundos = String(totalSegundos % 60).padStart(2, '0')
  return `${minutos}:${segundos}`
}

/**
 * Converte coordenadas (pos_x, pos_y) em código de célula alfanumérico (ex.: A1, B5).
 */
function formatarCodigoCelula(posX, posY) {
  const letraColuna = String.fromCharCode(65 + (posX % 26))
  return `${letraColuna}${posY + 1}`
}

/**
 * Formata número decimal no padrão brasileiro com vírgula.
 */
function formatarDecimalBr(valor, casas = 2) {
  if (typeof valor !== 'number' || Number.isNaN(valor)) {
    return (0).toFixed(casas).replace('.', ',')
  }
  return valor.toFixed(casas).replace('.', ',')
}

/**
 * Formata o horário de um timestamp em HH:MM:SS.
 */
function formatarHorario(timestampMs) {
  if (!timestampMs) {
    return '--:--:--'
  }
  const data = new Date(timestampMs)
  const h = String(data.getHours()).padStart(2, '0')
  const m = String(data.getMinutes()).padStart(2, '0')
  const s = String(data.getSeconds()).padStart(2, '0')
  return `${h}:${m}:${s}`
}

export default function Dashboard({
  wsUrl = import.meta.env?.VITE_WS_URL || 'ws://localhost:8000/ws/painel',
}) {
  const [abaAtiva, setAbaAtiva] = useState('monitoramento')
  const [conectadoWs, setConectadoWs] = useState(false)
  const [roboOnline, setRoboOnline] = useState(false)

  const [corridaAtual, setCorridaAtual] = useState(1)
  const [statusCorrida, setStatusCorrida] = useState('aguardando') // aguardando | em_execucao | concluida | abandonada
  const [motivoFim, setMotivoFim] = useState(null)
  const [tempoMs, setTempoMs] = useState(0)
  const [tempoConclusaoS, setTempoConclusaoS] = useState(null)
  const [consumoMedioMa, setConsumoMedioMa] = useState(null)

  const [roboPos, setRoboPos] = useState({
    pos_x: 0,
    pos_y: 0,
    orientacao: 'N',
  })
  const [mapaCelulas, setMapaCelulas] = useState({})
  const [trajeto, setTrajeto] = useState([])
  const [dimensoesLabirinto, setDimensoesLabirinto] = useState(lerDimensoesSalvas)

  function selecionarTipoLabirinto(largura, altura) {
    setDimensoesLabirinto({ largura, altura })
    window.localStorage.setItem(CHAVE_STORAGE_LABIRINTO, `${largura}x${altura}`)
  }

  const [tensaoBateria, setTensaoBateria] = useState(8.4)
  const [tensaoInicial, setTensaoInicial] = useState(null)
  const [bateriaCriticaAtiva, setBateriaCriticaAtiva] = useState(false)

  const [velocidadeAtual, setVelocidadeAtual] = useState(0)
  const [somaVelocidades, setSomaVelocidades] = useState(0)
  const [qtdLeiturasTel, setQtdLeiturasTel] = useState(0)

  const [valorFlood, setValorFlood] = useState(null)
  const [estadoExecucao, setEstadoExecucao] = useState('parado')

  const [ultimoPacoteEm, setUltimoPacoteEm] = useState(null)
  const [segundosDesdeUltimoPacote, setSegundosDesdeUltimoPacote] = useState(0)
  const [eventosRecentes, setEventosRecentes] = useState([
    {
      id: 'evt-inicial',
      horario: '--:--:--',
      titulo: 'Painel pronto — aguardando telemetria',
      tipo: 'info',
    },
  ])

  const ultimoPacoteRef = useRef(null)
  const roboOnlineRef = useRef(false)
  const celulasVistasRef = useRef(new Set())

  // Monitora o tempo desde o último pacote para marcar Offline após 3 s sem mensagens (CT-S06)
  useEffect(() => {
    const intervalo = setInterval(() => {
      if (!ultimoPacoteRef.current) {
        return
      }
      const agora = Date.now()
      const decorridoMs = agora - ultimoPacoteRef.current
      setSegundosDesdeUltimoPacote(Number((decorridoMs / 1000).toFixed(1)))

      if (decorridoMs >= TIMEOUT_OFFLINE_MS && roboOnlineRef.current) {
        roboOnlineRef.current = false
        setRoboOnline(false)
        setEventosRecentes((anteriores) => [
          {
            id: `offline-${agora}`,
            horario: formatarHorario(agora),
            titulo: 'Conexão perdida — mais de 3 s sem telemetria',
            tipo: 'erro',
          },
          ...anteriores.slice(0, 5),
        ])
      }
    }, 200)

    return () => clearInterval(intervalo)
  }, [])

  // Conexão WebSocket para recepção de pacotes em tempo real (< 500 ms)
  useEffect(() => {
    let cancelado = false
    let socket = null
    let timerReconexao = null

    function registrarEvento(titulo, tipo = 'info', instanteMs = Date.now()) {
      setEventosRecentes((anteriores) => [
        {
          id: `${instanteMs}-${Math.random().toString(36).slice(2, 7)}`,
          horario: formatarHorario(instanteMs),
          titulo,
          tipo,
        },
        ...anteriores.slice(0, 5),
      ])
    }

    function processarMensagem(eventoWs) {
      const recebidoEm = Date.now()
      const resultado = interpretarFrameTelemetria(eventoWs.data)
      if (!resultado.valido) {
        return
      }

      const { dados } = resultado

      // Atualiza marcadores de conexão (painel é apenas leitura; não envia ACK)
      ultimoPacoteRef.current = recebidoEm
      roboOnlineRef.current = true
      setUltimoPacoteEm(recebidoEm)
      setSegundosDesdeUltimoPacote(0)
      setRoboOnline(true)

      if (typeof dados.corrida === 'number') {
        setCorridaAtual(dados.corrida)
      }

      // Lê largura_celula e altura_celula (ou fallback largura/altura) em todo pacote válido
      setDimensoesLabirinto((atual) => resolverDimensoesLabirinto(dados, atual))

      setTempoMs(dados.tempo_ms)

      if (typeof dados.tensao_bateria === 'number') {
        setTensaoBateria(dados.tensao_bateria)
        setTensaoInicial((anterior) => (anterior === null ? dados.tensao_bateria : anterior))
        if (dados.tensao_bateria < LIMIAR_BATERIA_CRITICA_V) {
          setBateriaCriticaAtiva(true)
        } else if (dados.tensao_bateria >= LIMIAR_REARME_BATERIA_V) {
          setBateriaCriticaAtiva(false)
        }
      }

      if (dados.t === 'tel') {
        setStatusCorrida((atual) => (atual === 'aguardando' ? 'em_execucao' : atual))
        setEstadoExecucao(dados.estado)
        setVelocidadeAtual(dados.velocidade)
        setSomaVelocidades((soma) => soma + dados.velocidade)
        setQtdLeiturasTel((qtd) => qtd + 1)
        setValorFlood(dados.valor_flood ?? null)

        const novaPos = {
          pos_x: dados.pos_x,
          pos_y: dados.pos_y,
          orientacao: dados.orientacao,
        }
        setRoboPos(novaPos)

        setTrajeto((trajetoAnterior) => {
          const ultimoPonto = trajetoAnterior[trajetoAnterior.length - 1]
          if (
            ultimoPonto &&
            ultimoPonto.pos_x === dados.pos_x &&
            ultimoPonto.pos_y === dados.pos_y
          ) {
            return trajetoAnterior
          }
          return [...trajetoAnterior, { pos_x: dados.pos_x, pos_y: dados.pos_y }]
        })

        const chaveCelula = `${dados.pos_x},${dados.pos_y}`
        const codigo = formatarCodigoCelula(dados.pos_x, dados.pos_y)
        const jaVista = celulasVistasRef.current.has(chaveCelula)
        celulasVistasRef.current.add(chaveCelula)

        if (!jaVista) {
          registrarEvento(`Célula ${codigo} mapeada`, 'sucesso', recebidoEm)
        }

        setMapaCelulas((mapaAnterior) => ({
          ...mapaAnterior,
          [chaveCelula]: {
            pos_x: dados.pos_x,
            pos_y: dados.pos_y,
            paredes: dados.paredes,
            valor_flood: dados.valor_flood ?? null,
            orientacao: dados.orientacao,
            visitada: true,
          },
        }))
      } else if (dados.t === 'evt') {
        if (dados.evento === 'inicio') {
          celulasVistasRef.current.clear()
          setStatusCorrida('em_execucao')
          setMotivoFim(null)
          setTempoConclusaoS(null)
          setConsumoMedioMa(null)
          setMapaCelulas({})
          setTrajeto([])
          setSomaVelocidades(0)
          setQtdLeiturasTel(0)
          setEstadoExecucao('explorando')
          if (typeof dados.tensao_bateria === 'number') {
            setTensaoInicial(dados.tensao_bateria)
          }
          registrarEvento('Corrida iniciada', 'info', recebidoEm)
        } else if (dados.evento === 'bateria_critica') {
          setBateriaCriticaAtiva(true)
          registrarEvento(
            `Bateria crítica detectada (${formatarDecimalBr(dados.tensao_bateria, 2)} V)`,
            'aviso',
            recebidoEm,
          )
        } else if (dados.evento === 'reconexao') {
          registrarEvento(
            `Reconexão restabelecida (${dados.pendentes ?? 0} pacotes pendentes)`,
            'aviso',
            recebidoEm,
          )
        } else if (dados.evento === 'fim') {
          const concluida = dados.resultado === 'concluida'
          setStatusCorrida(concluida ? 'concluida' : 'abandonada')
          setMotivoFim(dados.motivo || null)
          setEstadoExecucao('parado')
          setVelocidadeAtual(0)
          if (typeof dados.tempo_conc_s === 'number') {
            setTempoConclusaoS(dados.tempo_conc_s)
          }
          if (typeof dados.consumo_medio_ma === 'number') {
            setConsumoMedioMa(dados.consumo_medio_ma)
          }
          registrarEvento(
            concluida
              ? `Corrida concluída em ${formatarDecimalBr(dados.tempo_conc_s ?? dados.tempo_ms / 1000, 2)} s`
              : `Corrida abandonada (${dados.motivo || 'interrompida'})`,
            concluida ? 'sucesso' : 'aviso',
            recebidoEm,
          )
        }
      }
    }

    function conectar() {
      if (cancelado || typeof WebSocket === 'undefined') {
        return
      }
      try {
        socket = new WebSocket(wsUrl)
        socket.onopen = () => {
          if (!cancelado) {
            setConectadoWs(true)
          }
        }
        socket.onmessage = processarMensagem
        socket.onerror = () => {
          if (!cancelado) {
            setConectadoWs(false)
          }
        }
        socket.onclose = () => {
          if (!cancelado) {
            setConectadoWs(false)
            timerReconexao = setTimeout(conectar, 2000)
          }
        }
      } catch {
        setConectadoWs(false)
      }
    }

    conectar()

    return () => {
      cancelado = true
      if (timerReconexao) {
        clearTimeout(timerReconexao)
      }
      if (socket) {
        try {
          socket.close()
        } catch {
          // Ignora erro no fechamento
        }
      }
    }
  }, [wsUrl])

  // Cálculos derivados para exibição nos cards
  const porcentagemBateria = calcularPorcentagemBateria(tensaoBateria)
  const porcentagemInicial = tensaoInicial !== null ? calcularPorcentagemBateria(tensaoInicial) : porcentagemBateria
  const variacaoBateria = porcentagemBateria - porcentagemInicial
  const alertaBateriaCritica = bateriaCriticaAtiva || tensaoBateria < LIMIAR_BATERIA_CRITICA_V

  const velocidadeMedia = qtdLeiturasTel > 0 ? somaVelocidades / qtdLeiturasTel : 0
  const qtdCelulasMapeadas = Object.keys(mapaCelulas).length
  const totalCelulasLabirinto = dimensoesLabirinto
    ? dimensoesLabirinto.largura * dimensoesLabirinto.altura
    : null
  const textoDimensoesLabirinto = dimensoesLabirinto
    ? `${dimensoesLabirinto.largura} × ${dimensoesLabirinto.altura}`
    : '--'
  const textoSubtituloMapa = dimensoesLabirinto
    ? `Leitura espacial • ${dimensoesLabirinto.altura} linhas × ${dimensoesLabirinto.largura} colunas`
    : 'Leitura espacial • aguardando dimensões'
  const textoCobertura =
    totalCelulasLabirinto && totalCelulasLabirinto > 0
      ? `${((qtdCelulasMapeadas / totalCelulasLabirinto) * 100).toFixed(1).replace('.', ',')}%`
      : '--'

  const codigoCelulaAtual = formatarCodigoCelula(roboPos.pos_x, roboPos.pos_y)
  const infoOrientacao = NOMES_ORIENTACAO[roboPos.orientacao] || NOMES_ORIENTACAO.N
  const infoEstadoExecucao = TEXTOS_ESTADO_EXECUCAO[estadoExecucao] || TEXTOS_ESTADO_EXECUCAO.parado

  const rotuloStatusCorrida =
    statusCorrida === 'em_execucao'
      ? 'Em execução'
      : statusCorrida === 'concluida'
        ? 'Concluído'
        : statusCorrida === 'abandonada'
          ? 'Abandonada'
          : 'Aguardando'

  const infoResultadoUltimaExecucao =
    statusCorrida === 'concluida'
      ? { texto: 'Concluído', classeTexto: 'texto-verde', classePonto: 'verde' }
      : statusCorrida === 'abandonada'
        ? {
            texto: `Abandonada (${motivoFim || 'parada'})`,
            classeTexto: 'texto-vermelho',
            classePonto: 'vermelho',
          }
        : statusCorrida === 'em_execucao'
          ? { texto: 'Em andamento', classeTexto: 'texto-ambar', classePonto: 'ambar' }
          : { texto: 'Aguardando', classeTexto: 'texto-secundario-forte', classePonto: 'neutro' }

  const desafioCumprido = statusCorrida === 'concluida'
  const onlineAtivo = roboOnline || (conectadoWs && ultimoPacoteEm === null)

  return (
    <div className="painel-layout">
      {/* Menu Lateral Esquerdo (Sidebar fiel ao Figma) */}
      <aside className="menu-lateral" aria-label="Menu principal">
        <div className="marca-topo">
          <div className="marca-icone" aria-hidden="true">
            <span className="marca-ponto" />
          </div>
          <span className="marca-titulo">Micromouse</span>
        </div>

        <div className="menu-secao-rotulo">NAVEGAÇÃO</div>
        <nav className="menu-navegacao">
          <button
            type="button"
            className={`menu-item ${abaAtiva === 'dashboard' ? 'ativo' : ''}`}
            onClick={() => setAbaAtiva('dashboard')}
          >
            <span className="menu-item-icone" aria-hidden="true">⊞</span>
            <span>Dashboard</span>
            {abaAtiva === 'dashboard' && <span className="menu-item-ponto" />}
          </button>

          <button
            type="button"
            className={`menu-item ${abaAtiva === 'monitoramento' ? 'ativo' : ''}`}
            onClick={() => setAbaAtiva('monitoramento')}
          >
            <span className="menu-item-icone" aria-hidden="true">⚡</span>
            <span>Monitoramento</span>
            {abaAtiva === 'monitoramento' && <span className="menu-item-ponto" />}
          </button>

          <button
            type="button"
            className={`menu-item ${abaAtiva === 'historico' ? 'ativo' : ''}`}
            onClick={() => setAbaAtiva('historico')}
          >
            <span className="menu-item-icone" aria-hidden="true">◷</span>
            <span>Histórico</span>
          </button>

          <button
            type="button"
            className={`menu-item ${abaAtiva === 'logs' ? 'ativo' : ''}`}
            onClick={() => setAbaAtiva('logs')}
          >
            <span className="menu-item-icone" aria-hidden="true">&gt;_</span>
            <span>Logs</span>
          </button>

          <button
            type="button"
            className={`menu-item ${abaAtiva === 'configuracoes' ? 'ativo' : ''}`}
            onClick={() => setAbaAtiva('configuracoes')}
          >
            <span className="menu-item-icone" aria-hidden="true">⚙</span>
            <span>Configurações</span>
          </button>
        </nav>
      </aside>

      {/* Conteúdo Principal */}
      <main className="conteudo-principal">
        {/* Cabeçalho Superior */}
        <header className="cabecalho-painel">
          <div>
            <h1 className="titulo-pagina">
              {abaAtiva === 'dashboard'
                ? 'Visão Geral'
                : abaAtiva === 'monitoramento'
                  ? 'Monitoramento em Tempo Real'
                  : abaAtiva === 'historico'
                    ? 'Histórico de Corridas'
                    : abaAtiva === 'logs'
                      ? 'Logs e Eventos'
                      : 'Configurações'}
            </h1>
            <p className="subtitulo-pagina">
              {abaAtiva === 'dashboard'
                ? 'Acompanhamento geral do Micromouse'
                : abaAtiva === 'monitoramento'
                  ? 'Acompanhamento da execução e telemetria do Micromouse'
                  : abaAtiva === 'historico'
                    ? 'Histórico de tentativas e trajetos anteriores'
                    : abaAtiva === 'logs'
                      ? 'Depuração e auditoria de eventos e erros do sistema'
                      : 'Configurações e parâmetros do sistema de telemetria'}
            </p>
          </div>

          <div className="cabecalho-acoes">
            <div className={`pilula-tempo-real ${onlineAtivo ? 'online' : 'offline'}`}>
              <span className="ponto-status" />
              <span>Tempo Real</span>
              <span className="separador-pilula">•</span>
              <span data-testid="horario-atualizacao">
                Atualizado às {formatarHorario(ultimoPacoteEm)}
              </span>
            </div>
          </div>
        </header>

        {/* =========================================================================
            ABA 1: VISÃO GERAL (DASHBOARD) - 100% FIEL AO FIGMA (dashboard.png)
           ========================================================================= */}
        {abaAtiva === 'dashboard' && (
          <>
            {/* Faixa Superior de 4 Cards da Visão Geral */}
            <section className="grade-resumo-topo" aria-label="Resumo geral do robô">
              {/* Card 1: Status do Robô */}
              <article className="card-topo" data-testid="card-status-robo">
                <div className={`icone-quadrado ${onlineAtivo ? 'verde-claro-icone' : 'vermelho'}`} aria-hidden="true">
                  🤖
                </div>
                <div className="card-topo-conteudo">
                  <span className="card-topo-rotulo">Status do Robô</span>
                  <strong className={`card-topo-valor ${onlineAtivo ? 'texto-verde-destaque' : 'texto-vermelho'}`}>
                    {onlineAtivo ? 'Conectado' : 'Offline'}
                  </strong>
                  <span className="card-topo-sub">
                    <span className={`ponto-inline ${onlineAtivo ? 'verde' : 'vermelho'}`} />{' '}
                    {onlineAtivo ? 'Comunicação ativa' : 'Conexão perdida'}
                  </span>
                </div>
              </article>

              {/* Card 2: Bateria */}
              <article className="card-topo" data-testid="card-bateria-visao-geral">
                <div className="icone-quadrado azul-claro-icone" aria-hidden="true">
                  ⚡
                </div>
                <div className="card-topo-conteudo card-conteudo-bateria-topo">
                  <span className="card-topo-rotulo">Bateria</span>
                  <strong className="card-topo-valor" data-testid="porcentagem-bateria-visao-geral">
                    {porcentagemBateria}%
                  </strong>
                  <div
                    className="barra-progresso-trilha barra-progresso-topo"
                    role="progressbar"
                    aria-label="Nível de bateria na visão geral"
                    aria-valuenow={porcentagemBateria}
                    aria-valuemin={0}
                    aria-valuemax={100}
                    data-testid="barra-progresso-bateria-visao-geral"
                  >
                    <div
                      className={`barra-progresso-preenchimento ${alertaBateriaCritica ? 'critica' : ''}`}
                      style={{ width: `${porcentagemBateria}%` }}
                    />
                  </div>
                  <span className="card-topo-sub">
                    <span className={`ponto-inline ${alertaBateriaCritica ? 'vermelho' : 'neutro'}`} />{' '}
                    {alertaBateriaCritica ? 'Bateria crítica' : 'Nível normal'}
                  </span>
                </div>
              </article>

              {/* Card 3: Estado da Corrida */}
              <article className="card-topo" data-testid="card-estado-corrida-visao-geral">
                <div className="icone-quadrado azul-claro-icone" aria-hidden="true">
                  🎯
                </div>
                <div className="card-topo-conteudo">
                  <span className="card-topo-rotulo">Estado da Corrida</span>
                  <div className="linha-valor-com-grafico">
                    <strong className="card-topo-valor">{rotuloStatusCorrida}</strong>
                    <span className="mini-barras-sinal" aria-hidden="true">
                      <span className="barra barra-1" />
                      <span className="barra barra-2" />
                      <span className="barra barra-3" />
                    </span>
                  </div>
                  <span className="card-topo-sub">
                    <span className="ponto-inline neutro" /> {infoEstadoExecucao.rotulo}
                  </span>
                </div>
              </article>

              {/* Card 4: Labirinto Atual */}
              <article className="card-topo" data-testid="card-labirinto-visao-geral">
                <div className="icone-quadrado azul-claro-icone" aria-hidden="true">
                  ⊞
                </div>
                <div className="card-topo-conteudo">
                  <span className="card-topo-rotulo">Labirinto Atual</span>
                  <strong className="card-topo-valor" data-testid="valor-labirinto-visao-geral">
                    {textoDimensoesLabirinto}
                  </strong>
                  <span className="card-topo-sub">
                    <span className="ponto-inline neutro" /> Tentativa {corridaAtual}
                  </span>
                </div>
              </article>
            </section>

            {/* Seção Central da Visão Geral (Última Execução + Status do Sistema) */}
            <section className="grade-visao-geral-principal" aria-label="Última Execução e Status do Sistema">
              {/* Painel: Última Execução */}
              <article className="card-painel card-ultima-execucao" data-testid="painel-ultima-execucao">
                <div className="card-painel-cabecalho-linha">
                  <div className="titulo-com-icone">
                    <span className="icone-bandeira" aria-hidden="true">⚐</span>
                    <h2 className="card-titulo">Última Execução</h2>
                  </div>
                  <span className="exec-tag">EXEC - {String(corridaAtual).padStart(4, '0')}</span>
                </div>

                <div className="grade-metricas-ultima-execucao">
                  <div className="metrica-bloco">
                    <span className="rotulo-caixa-alta">TIPO DO LABIRINTO</span>
                    <strong className="metrica-valor" data-testid="tipo-labirinto-ultima-execucao">
                      {textoDimensoesLabirinto}
                    </strong>
                  </div>

                  <div className="metrica-bloco">
                    <span className="rotulo-caixa-alta">TENTATIVA</span>
                    <strong className="metrica-valor">{corridaAtual}</strong>
                  </div>

                  <div className="metrica-bloco">
                    <span className="rotulo-caixa-alta">TEMPO</span>
                    <strong className="metrica-valor">
                      {tempoConclusaoS !== null
                        ? `${formatarDecimalBr(tempoConclusaoS, 2)} s`
                        : formatarCronometro(tempoMs)}
                    </strong>
                  </div>

                  <div className="metrica-bloco">
                    <span className="rotulo-caixa-alta">VELOCIDADE MÉDIA</span>
                    <strong className="metrica-valor">
                      {formatarDecimalBr(velocidadeMedia, 2)} m/s
                    </strong>
                  </div>

                  <div className="metrica-bloco">
                    <span className="rotulo-caixa-alta">CONSUMO DE BATERIA</span>
                    <strong className="metrica-valor">
                      {consumoMedioMa !== null
                        ? `${formatarDecimalBr(consumoMedioMa, 1)} mA`
                        : `${Math.abs(variacaoBateria)}%`}
                    </strong>
                  </div>

                  <div className="metrica-bloco">
                    <span className="rotulo-caixa-alta">RESULTADO</span>
                    <strong
                      className={`metrica-valor ${infoResultadoUltimaExecucao.classeTexto}`}
                      data-testid="resultado-ultima-execucao"
                    >
                      <span
                        className={`ponto-inline ${infoResultadoUltimaExecucao.classePonto}`}
                        data-testid="ponto-resultado-ultima-execucao"
                      />{' '}
                      {infoResultadoUltimaExecucao.texto}
                    </strong>
                  </div>
                </div>

                <div className="rodape-ultima-execucao">
                  <span className="texto-secundario">
                    {ultimoPacoteEm ? `Último registro às ${formatarHorario(ultimoPacoteEm)}` : 'Aguardando telemetria'}
                  </span>
                  <button
                    type="button"
                    className="botao-ver-detalhes"
                    onClick={() => setAbaAtiva('monitoramento')}
                  >
                    Ver detalhes <span aria-hidden="true">→</span>
                  </button>
                </div>
              </article>

              {/* Painel: Status do Sistema */}
              <article className="card-painel card-status-sistema" data-testid="painel-status-sistema">
                <div className="card-painel-cabecalho-linha">
                  <div className="titulo-com-icone">
                    <span aria-hidden="true">⊞</span>
                    <h2 className="card-titulo">Status do Sistema</h2>
                  </div>
                  <span className={`badge-sistema-status ${onlineAtivo ? 'normal' : 'alerta'}`}>
                    {onlineAtivo ? 'NORMAL' : 'OFFLINE'}
                  </span>
                </div>

                <ul className="lista-status-sistema-figma">
                  <li className="item-status-sistema">
                    <span className="descricao-item-sistema">
                      <span className={`ponto-inline ${onlineAtivo ? 'verde' : 'vermelho'}`} />
                      Conexão com o Micromouse
                    </span>
                    <strong className={onlineAtivo ? 'texto-verde' : 'texto-vermelho'}>
                      {onlineAtivo ? 'Online' : 'Offline'}
                    </strong>
                  </li>

                  <li className="item-status-sistema">
                    <span className="descricao-item-sistema">
                      <span className={`ponto-inline ${onlineAtivo ? 'verde' : 'vermelho'}`} />
                      Recebimento de telemetria
                    </span>
                    <strong className={onlineAtivo ? 'texto-verde' : 'texto-vermelho'}>
                      {onlineAtivo ? 'Ativo' : 'Inativo'}
                    </strong>
                  </li>

                  <li className="item-status-sistema">
                    <span className="descricao-item-sistema">
                      <span className="ponto-inline neutro" />
                      Banco de dados
                    </span>
                    <strong className="texto-secundario" data-testid="status-banco-dados">
                      --
                    </strong>
                  </li>

                  <li className="item-status-sistema">
                    <span className="descricao-item-sistema">
                      <span aria-hidden="true">⏱</span>
                      Última atualização
                    </span>
                    <strong className="texto-secundario" data-testid="ultima-atualizacao-sistema">
                      {ultimoPacoteEm === null
                        ? '--'
                        : segundosDesdeUltimoPacote > 0
                          ? `há ${formatarDecimalBr(segundosDesdeUltimoPacote, 1)} segundos`
                          : 'há 0,0 segundos'}
                    </strong>
                  </li>
                </ul>
              </article>
            </section>
          </>
        )}

        {/* =========================================================================
            ABA 2: MONITORAMENTO EM TEMPO REAL - 100% FIEL AO FIGMA (monitoramento.png)
           ========================================================================= */}
        {abaAtiva === 'monitoramento' && (
          <>
            {/* Faixa Superior de 4 Cards do Monitoramento */}
            <section className="grade-resumo-topo" aria-label="Resumo de status da corrida">
              {/* Card 1: Conexão */}
              <article
                className={`card-topo ${!onlineAtivo && ultimoPacoteEm !== null ? 'card-alerta-conexao' : ''}`}
                data-testid="card-conexao"
              >
                <div className={`icone-quadrado ${onlineAtivo ? 'verde' : 'vermelho'}`} aria-hidden="true">
                  📶
                </div>
                <div className="card-topo-conteudo">
                  <span className="card-topo-rotulo">CONEXÃO</span>
                  <strong className={`card-topo-valor ${onlineAtivo ? 'texto-verde' : 'texto-vermelho'}`}>
                    <span className="ponto-inline" />
                    {onlineAtivo ? 'Conectado' : 'Offline'}
                  </strong>
                  <span className="card-topo-sub">
                    {onlineAtivo ? 'Telemetria ativa' : 'Alerta: Conexão perdida'}
                  </span>
                </div>
              </article>

              {/* Card 2: Labirinto */}
              <article className="card-topo" data-testid="card-labirinto">
                <div className="icone-quadrado neutro" aria-hidden="true">
                  ⊞
                </div>
                <div className="card-topo-conteudo">
                  <span className="card-topo-rotulo">LABIRINTO</span>
                  <strong className="card-topo-valor" data-testid="valor-labirinto-monitoramento">
                    {textoDimensoesLabirinto}
                  </strong>
                  <span className="card-topo-sub">Tentativa {corridaAtual}</span>
                </div>
              </article>

              {/* Card 3: Estado da Corrida (Destacado em tom verde-água no Figma) */}
              <article className="card-topo card-topo-destaque" data-testid="card-estado-corrida">
                <div className="icone-quadrado petroleo" aria-hidden="true">
                  ⇄
                </div>
                <div className="card-topo-conteudo">
                  <span className="card-topo-rotulo">ESTADO DA CORRIDA</span>
                  <strong className="card-topo-valor texto-petroleo">{rotuloStatusCorrida}</strong>
                  <span className="card-topo-sub">{infoEstadoExecucao.resumo}</span>
                </div>
              </article>

              {/* Card 4: Tempo */}
              <article className="card-topo" data-testid="card-tempo">
                <div className="icone-quadrado neutro" aria-hidden="true">
                  ⏱
                </div>
                <div className="card-topo-conteudo">
                  <span className="card-topo-rotulo">TEMPO</span>
                  <strong className="card-topo-valor" data-testid="valor-cronometro">
                    {formatarCronometro(tempoMs)}
                  </strong>
                </div>
              </article>
            </section>

            {/* Área Central: Mapa + Coluna Direita de Indicadores */}
            <section className="grade-monitoramento-central">
              {/* Mapa do Labirinto com o componente MazeMap.jsx do José Eduardo */}
              <article className="card-painel card-mapa-labirinto" aria-label="Mapa do Labirinto">
                <div className="card-mapa-cabecalho">
                  <div>
                    <h2 className="card-titulo">Mapa do Labirinto</h2>
                    <p className="card-subtitulo" data-testid="subtitulo-mapa">
                      {textoSubtituloMapa}
                    </p>
                  </div>

                  <div
                    className="seletor-tipo-labirinto"
                    role="group"
                    aria-label="Seletor do tipo de labirinto"
                    data-testid="seletor-tipo-labirinto"
                  >
                    <span className="rotulo-seletor-labirinto">Tipo:</span>
                    {LABIRINTOS_OFICIAIS.map((opcao) => {
                      const chave = `${opcao.largura}x${opcao.altura}`
                      const ativo =
                        dimensoesLabirinto?.largura === opcao.largura &&
                        dimensoesLabirinto?.altura === opcao.altura
                      return (
                        <button
                          key={chave}
                          type="button"
                          className={`botao-seletor-labirinto ${ativo ? 'ativo' : ''}`}
                          aria-pressed={ativo}
                          data-testid={`seletor-labirinto-${chave}`}
                          onClick={() => selecionarTipoLabirinto(opcao.largura, opcao.altura)}
                        >
                          {opcao.largura} × {opcao.altura}
                        </button>
                      )
                    })}
                  </div>
                </div>

                <div className="card-mapa-corpo">
                  <div
                    className="container-maze-map"
                    data-testid="espaco-mapa-labirinto"
                    data-celulas-count={qtdCelulasMapeadas}
                    data-robo-pos={`${roboPos.pos_x},${roboPos.pos_y},${roboPos.orientacao}`}
                    data-trajeto-count={trajeto.length}
                    data-trajeto={trajeto.map((p) => `${p.pos_x},${p.pos_y}`).join(';')}
                  >
                    {dimensoesLabirinto ? (
                      <MazeMap
                        largura={dimensoesLabirinto.largura}
                        altura={dimensoesLabirinto.altura}
                        celulas={mapaCelulas}
                        roboPos={roboPos}
                        trajeto={trajeto}
                      />
                    ) : (
                      <div className="placeholder-maze-map" data-testid="aviso-escolha-labirinto">
                        <span className="placeholder-grade-icone" aria-hidden="true">
                          ⊞
                        </span>
                        <strong className="placeholder-titulo">Escolha o tipo de labirinto</strong>
                        <span className="placeholder-detalhe">
                          Selecione 4 × 4, 8 × 4 ou 12 × 4 acima para desenhar o mapa.
                        </span>
                      </div>
                    )}
                  </div>

                  {/* Detalhes ao lado do mapa */}
                  <div className="coluna-detalhes-mapa">
                    <div className="subcard-mapa">
                      <div className="subcard-posicao-topo">
                        <div className="icone-quadrado-pequeno petroleo" aria-hidden="true">
                          ⊕
                        </div>
                        <div>
                          <span className="rotulo-caixa-alta">POSIÇÃO ATUAL</span>
                          <div className="valor-posicao-destaque" data-testid="posicao-atual-destaque">
                            {codigoCelulaAtual} · {infoOrientacao.nome}
                          </div>
                        </div>
                      </div>

                      <div className="divisor-horizontal" />

                      <div className="linha-metrica-mapa">
                        <span>Células mapeadas</span>
                        <strong data-testid="celulas-mapeadas-total">
                          {totalCelulasLabirinto !== null
                            ? `${qtdCelulasMapeadas} / ${totalCelulasLabirinto}`
                            : `${qtdCelulasMapeadas} / --`}
                        </strong>
                      </div>
                      <div className="linha-metrica-mapa">
                        <span>Cobertura</span>
                        <strong className="texto-petroleo" data-testid="cobertura-percentual">
                          {textoCobertura}
                        </strong>
                      </div>
                    </div>

                    <div className="subcard-mapa">
                      <div className="subcard-titulo-icone">
                        <span className="texto-azul" aria-hidden="true">⇄</span>
                        <strong>Planejamento de rota</strong>
                      </div>
                      <div className="linha-metrica-mapa">
                        <span>Distância ao objetivo</span>
                        <strong data-testid="distancia-flood">
                          {valorFlood !== null ? `${valorFlood} células` : '--'}
                        </strong>
                      </div>
                      <div className="pilula-caminho-disponivel">
                        <span className="ponto-inline" />{' '}
                        {valorFlood !== null ? 'Menor caminho disponível' : 'Aguardando leitura de rota'}
                      </div>
                    </div>

                    <div className="subcard-leitura-mapa">
                      <span className="rotulo-caixa-alta">LEITURA DO MAPA</span>
                      <p>
                        Células cinza ainda não foram observadas. Paredes escuras representam limites
                        confirmados pelos sensores.
                      </p>
                    </div>
                  </div>
                </div>
              </article>

              {/* Painel lateral de Telemetria */}
              <aside className="coluna-telemetria" aria-label="Indicadores de Telemetria em Tempo Real">
                <div className="cabecalho-coluna-telemetria">
                  <h2 className="titulo-coluna-telemetria">
                    <span className="icone-pulso" aria-hidden="true">∿</span> Telemetria
                  </h2>
                </div>

                {/* Bateria */}
                <article
                  className={`card-indicador ${alertaBateriaCritica ? 'card-bateria-critica' : ''}`}
                  data-testid="card-bateria"
                >
                  <div className="card-indicador-topo">
                    <span className="icone-indicador verde" aria-hidden="true">🔋</span>
                    <h3 className="card-indicador-titulo">Bateria</h3>
                    <span className="tensao-badge" data-testid="tensao-bateria">
                      {formatarDecimalBr(tensaoBateria, 2)} V
                    </span>
                  </div>

                  <div className="bateria-linha-principal">
                    <strong className="bateria-porcentagem" data-testid="porcentagem-bateria">
                      {porcentagemBateria}%
                    </strong>
                    <div
                      className="barra-progresso-trilha"
                      role="progressbar"
                      aria-valuenow={porcentagemBateria}
                      aria-valuemin={0}
                      aria-valuemax={100}
                    >
                      <div
                        className={`barra-progresso-preenchimento ${alertaBateriaCritica ? 'critica' : ''}`}
                        style={{ width: `${porcentagemBateria}%` }}
                      />
                    </div>
                  </div>

                  <div className="card-indicador-rodape">
                    {alertaBateriaCritica ? (
                      <span className="alerta-critico-texto" role="alert" data-testid="alerta-bateria-critica">
                        ⚠ Bateria crítica ({formatarDecimalBr(tensaoBateria, 2)} V — rearme em 7,20 V)
                      </span>
                    ) : (
                      <span className="texto-secundario" data-testid="status-bateria-normal">
                        Nível normal ({variacaoBateria <= 0 ? `${variacaoBateria}%` : `+${variacaoBateria}%`} desde o início)
                      </span>
                    )}
                  </div>
                </article>

                {/* Velocidade */}
                <article className="card-indicador" data-testid="card-velocidade">
                  <div className="card-indicador-topo">
                    <span className="icone-indicador petroleo" aria-hidden="true">⏲</span>
                    <h3 className="card-indicador-titulo">Velocidade</h3>
                  </div>

                  <div className="linhas-duplas-indicador">
                    <div className="linha-indicador">
                      <span>Velocidade média</span>
                      <strong data-testid="velocidade-media">
                        {formatarDecimalBr(velocidadeMedia, 2)} m/s
                      </strong>
                    </div>
                    <div className="linha-indicador">
                      <span>Velocidade atual</span>
                      <strong className="texto-petroleo" data-testid="velocidade-atual">
                        {formatarDecimalBr(velocidadeAtual, 2)} m/s
                      </strong>
                    </div>
                  </div>
                </article>

                {/* Posição */}
                <article className="card-indicador" data-testid="card-posicao">
                  <div className="card-indicador-topo">
                    <span className="icone-indicador petroleo" aria-hidden="true">📍</span>
                    <h3 className="card-indicador-titulo">Posição</h3>
                  </div>

                  <div className="posicao-resumo-linha">
                    <span className="pilula-celula" data-testid="celula-atual-badge">
                      {codigoCelulaAtual} ({roboPos.pos_x}, {roboPos.pos_y})
                    </span>
                    <strong className="orientacao-label" data-testid="orientacao-atual">
                      {infoOrientacao.nome} {infoOrientacao.seta}
                    </strong>
                    <span className="texto-secundario">{qtdCelulasMapeadas} células</span>
                  </div>
                </article>

                {/* Status da Execução */}
                <article className="card-indicador" data-testid="card-status-execucao">
                  <div className="card-indicador-topo">
                    <span className="icone-indicador ambar" aria-hidden="true">⌖</span>
                    <h3 className="card-indicador-titulo">Status da Execução</h3>
                  </div>

                  <div className="status-execucao-Principal">
                    <span className={`ponto-estado ${infoEstadoExecucao.cor}`} />
                    <strong data-testid="texto-status-execucao">{infoEstadoExecucao.rotulo}</strong>
                  </div>

                  <div className="linha-indicador">
                    <span>Desafio cumprido</span>
                    <strong data-testid="desafio-cumprido">{desafioCumprido ? 'Sim' : 'Não'}</strong>
                  </div>
                </article>

                {/* Comunicação */}
                <article className="card-indicador" data-testid="card-comunicacao">
                  <div className="card-indicador-topo">
                    <span className="icone-indicador verde" aria-hidden="true">((•))</span>
                    <h3 className="card-indicador-titulo">Comunicação</h3>
                  </div>

                  <div className="comunicacao-status-linha">
                    <span className={`ponto-inline ${onlineAtivo ? 'verde' : 'vermelho'}`} />
                    <span
                      className={onlineAtivo ? 'texto-verde' : 'texto-vermelho'}
                      data-testid="status-conexao-micromouse"
                    >
                      {onlineAtivo ? 'Conexão com o robô estável' : 'Conexão com o Micromouse: Offline'}
                    </span>
                  </div>

                  <div className="comunicacao-rodape">
                    <span>
                      Último pacote{' '}
                      <strong data-testid="ultimo-pacote-tempo">
                        {ultimoPacoteEm !== null ? `há ${formatarDecimalBr(segundosDesdeUltimoPacote, 1)} s` : '--'}
                      </strong>
                    </span>
                  </div>
                </article>
              </aside>
            </section>

            {/* Rodapé: Eventos Recentes */}
            <section className="card-painel card-eventos-recentes" aria-label="Eventos Recentes">
              <div className="card-eventos-cabecalho">
                <h2 className="card-titulo-pequeno">☰ Eventos Recentes</h2>
                <span className="fluxo-eventos-status">
                  <span className="ponto-inline verde" /> Fluxo de eventos ativo
                </span>
              </div>

              <div className="lista-eventos-horizontal" data-testid="lista-eventos-recentes">
                {eventosRecentes.slice(0, 4).map((evt) => (
                  <div key={evt.id} className="item-evento-recente">
                    <div className={`icone-evento-circulo ${evt.tipo}`} aria-hidden="true">
                      {evt.tipo === 'erro' ? '!' : evt.tipo === 'aviso' ? '⚠' : evt.tipo === 'sucesso' ? '✓' : '▶'}
                    </div>
                    <div>
                      <span className={`horario-evento ${evt.tipo}`}>{evt.horario}</span>
                      <strong className="titulo-evento">{evt.titulo}</strong>
                    </div>
                  </div>
                ))}
              </div>
            </section>
          </>
        )}

        {/* =========================================================================
            ABAS AUXILIARES (Histórico, Logs e Configurações)
           ========================================================================= */}
        {abaAtiva === 'historico' && (
          <section className="card-painel card-em-desenvolvimento" data-testid="tela-historico">
            <div className="placeholder-secao-texto">
              <span className="icone-secao-grande" aria-hidden="true">◷</span>
              <h2 className="card-titulo">Histórico de Corridas</h2>
              <p className="texto-secundario">
                Módulo referente às atividades <strong>3.8.4</strong> (Consultas e Filtros por Labirinto) e{' '}
                <strong>3.8.5</strong> (Exportação de relatórios em CSV e PDF).
              </p>
              <button type="button" className="botao-voltar" onClick={() => setAbaAtiva('dashboard')}>
                Voltar para a Visão Geral
              </button>
            </div>
          </section>
        )}

        {abaAtiva === 'logs' && (
          <section className="card-painel card-em-desenvolvimento" data-testid="tela-logs">
            <div className="placeholder-secao-texto">
              <span className="icone-secao-grande" aria-hidden="true">&gt;_</span>
              <h2 className="card-titulo">Logs e Eventos</h2>
              <p className="texto-secundario">
                Módulo referente à auditoria de descartes e eventos de hardware/software (RF20-S).
              </p>
              <button type="button" className="botao-voltar" onClick={() => setAbaAtiva('dashboard')}>
                Voltar para a Visão Geral
              </button>
            </div>
          </section>
        )}

        {abaAtiva === 'configuracoes' && (
          <section className="card-painel card-em-desenvolvimento" data-testid="tela-configuracoes">
            <div className="placeholder-secao-texto">
              <span className="icone-secao-grande" aria-hidden="true">⚙</span>
              <h2 className="card-titulo">Configurações</h2>
              <p className="texto-secundario">
                Configurações da URL do WebSocket (atual: <code>{wsUrl}</code>) e parâmetros locais da aplicação.
              </p>
              <button type="button" className="botao-voltar" onClick={() => setAbaAtiva('dashboard')}>
                Voltar para a Visão Geral
              </button>
            </div>
          </section>
        )}
      </main>
    </div>
  )
}
