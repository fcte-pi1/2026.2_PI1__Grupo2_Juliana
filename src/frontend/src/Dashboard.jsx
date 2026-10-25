import { useEffect, useRef, useState } from 'react'
import MazeMap from './components/MazeMap.jsx'
import ConnectionAlert from './components/ConnectionAlert.jsx'

const LIMIAR_BATERIA_CRITICA_V = 7.0
const LIMIAR_REARME_BATERIA_V = 7.2
const TEXTO_REARME_BATERIA = LIMIAR_REARME_BATERIA_V.toFixed(2).replace('.', ',')
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
 * Verifica se o ponto (pos_x, pos_y) está dentro dos limites internos da matriz (0..largura-1, 0..altura-1).
 */
function ehCelulaInterna(ponto, dimensoes) {
  return Boolean(
    dimensoes &&
      ponto &&
      Number.isInteger(ponto.pos_x) &&
      Number.isInteger(ponto.pos_y) &&
      ponto.pos_x >= 0 &&
      ponto.pos_x < dimensoes.largura &&
      ponto.pos_y >= 0 &&
      ponto.pos_y < dimensoes.altura,
  )
}

/**
 * Valida se a coordenada (pos_x, pos_y) de um pacote tel é permitida pelas dimensões atuais do labirinto.
 * Contrato v1.3 (Labirinto Fechado): aceita estritamente 0 <= pos_x < largura e 0 <= pos_y < altura.
 */
function ehPosicaoPermitida(ponto, dimensoes) {
  if (!ponto || !Number.isInteger(ponto.pos_x) || !Number.isInteger(ponto.pos_y)) {
    return false
  }
  if (ponto.pos_x < 0 || ponto.pos_y < 0) {
    return false
  }
  if (!dimensoes) {
    return true
  }
  return ehCelulaInterna(ponto, dimensoes)
}

/**
 * Centraliza o cálculo da saída/objetivo do labirinto fechado (Contrato v1.3).
 * Quando a corrida está concluída e há dimensões definidas, o objetivo alcançado
 * é sempre o canto diagonal oposto à largada: (largura - 1, altura - 1).
 */
function calcularSaidaLabirinto(dimensoes, concluida = false) {
  if (concluida && dimensoes) {
    const objetivo = {
      pos_x: dimensoes.largura - 1,
      pos_y: dimensoes.altura - 1,
    }
    return {
      ultimaCelulaInterna: objetivo,
      pontoSaidaExterna: objetivo,
    }
  }
  return {
    ultimaCelulaInterna: null,
    pontoSaidaExterna: null,
  }
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
function interpretarFrameTelemetria(textoBruto, dimensoesAtuais = null) {
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

  const dimensoesEfetivas = resolverDimensoesLabirinto(dados, dimensoesAtuais)

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
    if (!ehPosicaoPermitida(dados, dimensoesEfetivas)) {
      return { valido: false, motivo: 'fora do intervalo: coordenadas invalidas' }
    }
    if (!NOMES_ORIENTACAO[dados.orientacao]) {
      return { valido: false, motivo: `orientacao invalida: ${dados.orientacao}` }
    }
    if (!TEXTOS_ESTADO_EXECUCAO[dados.estado]) {
      return { valido: false, motivo: `estado invalido: ${dados.estado}` }
    }
  }

  return { valido: true, dados, dimensoes: dimensoesEfetivas }
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

/**
 * Formata uma string ISO de data/hora no fuso America/Sao_Paulo em DD/MM/AAAA HH:MM(:SS).
 */
function formatarDataHoraBr(isoString, incluirSegundos = false) {
  if (!isoString) {
    return '--'
  }
  const data = new Date(isoString)
  if (Number.isNaN(data.getTime())) {
    return '--'
  }
  const opcoes = {
    timeZone: 'America/Sao_Paulo',
    day: '2-digit',
    month: '2-digit',
    year: 'numeric',
    hour: '2-digit',
    minute: '2-digit',
    hour12: false,
  }
  if (incluirSegundos) {
    opcoes.second = '2-digit'
  }
  return new Intl.DateTimeFormat('pt-BR', opcoes).format(data).replace(',', '')
}

/**
 * Extrai a data AAAA-MM-DD no fuso America/Sao_Paulo a partir de uma string ISO.
 */
function extrairDataIso(isoString) {
  if (!isoString || typeof isoString !== 'string') {
    return ''
  }
  if (/^\d{4}-\d{2}-\d{2}$/.test(isoString)) {
    return isoString
  }
  const data = new Date(isoString)
  if (Number.isNaN(data.getTime())) {
    return isoString.slice(0, 10)
  }
  const partes = new Intl.DateTimeFormat('en-CA', {
    timeZone: 'America/Sao_Paulo',
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
  }).formatToParts(data)
  const ano = partes.find((p) => p.type === 'year')?.value || ''
  const mes = partes.find((p) => p.type === 'month')?.value || ''
  const dia = partes.find((p) => p.type === 'day')?.value || ''
  return `${ano}-${mes}-${dia}`
}

/**
 * Determina o status de um registro de log sem rotular todo erro como Descartado.
 */
function resolverStatusLog(log) {
  if (log?.status) {
    return String(log.status)
  }
  const msg = String(log?.mensagem || '')
  if (/descartad|checksum|formato invalido|json invalido|fora do intervalo|versao nao suportada/i.test(msg)) {
    return 'Descartado'
  }
  return 'Registrado'
}

/**
 * Reconstrói mapa de células, posição final, trajeto e saída a partir das leituras de CorridaDetalhe.
 */
function reconstruirDadosMapaCorrida(detalhe) {
  if (!detalhe) {
    return {
      dimensoes: null,
      celulas: {},
      roboPos: null,
      trajeto: [],
      ultimaCelula: null,
      pontoSaida: null,
    }
  }
  const dimensoes = ehLabirintoOficial(detalhe.largura_celula, detalhe.altura_celula)
    ? { largura: detalhe.largura_celula, altura: detalhe.altura_celula }
    : null
  const leituras = Array.isArray(detalhe.leituras) ? detalhe.leituras : []
  const celulas = {}
  const trajeto = []

  for (const leitura of leituras) {
    if (!ehPosicaoPermitida(leitura, dimensoes)) {
      continue
    }
    const ultimoPonto = trajeto[trajeto.length - 1]
    if (!ultimoPonto || ultimoPonto.pos_x !== leitura.pos_x || ultimoPonto.pos_y !== leitura.pos_y) {
      trajeto.push({ pos_x: leitura.pos_x, pos_y: leitura.pos_y })
    }
    celulas[`${leitura.pos_x},${leitura.pos_y}`] = {
      pos_x: leitura.pos_x,
      pos_y: leitura.pos_y,
      paredes: leitura.paredes,
      valor_flood: leitura.valor_flood ?? null,
      orientacao: leitura.orientacao,
      visitada: true,
    }
  }

  const ultimaLeitura = leituras[leituras.length - 1] || null
  const roboPos =
    ultimaLeitura && ehPosicaoPermitida(ultimaLeitura, dimensoes)
      ? {
          pos_x: ultimaLeitura.pos_x,
          pos_y: ultimaLeitura.pos_y,
          orientacao: ultimaLeitura.orientacao || 'N',
        }
      : null
  const { ultimaCelulaInterna, pontoSaidaExterna } = calcularSaidaLabirinto(
    dimensoes,
    detalhe.status === 'concluida',
  )

  return {
    dimensoes,
    celulas,
    roboPos,
    trajeto,
    ultimaCelula: ultimaCelulaInterna,
    pontoSaida: pontoSaidaExterna,
  }
}

export default function Dashboard({
  wsUrl = import.meta.env?.VITE_WS_URL || 'ws://localhost:8000/ws/painel',
  apiBaseUrl = import.meta.env?.VITE_API_URL || 'http://localhost:8000',
}) {
  const [abaAtiva, setAbaAtiva] = useState('monitoramento')
  const [conectadoWs, setConectadoWs] = useState(false)
  const [roboOnline, setRoboOnline] = useState(false)

  // Estados da aba Histórico (3.8.5 / 3.8.2 - RF07-S, RF08-S, RF09-S, RF10-S)
  const [labirintosHistorico, setLabirintosHistorico] = useState([])
  const [erroLabirintos, setErroLabirintos] = useState(null)
  const [corridasHistorico, setCorridasHistorico] = useState([])
  const [carregandoHistorico, setCarregandoHistorico] = useState(false)
  const [erroHistorico, setErroHistorico] = useState(null)
  const [filtroLabirintoId, setFiltroLabirintoId] = useState('')
  const [filtroStatus, setFiltroStatus] = useState('')
  const [filtroTentativa, setFiltroTentativa] = useState('')
  const [filtroData, setFiltroData] = useState('')
  const [contadorRecargaHistorico, setContadorRecargaHistorico] = useState(0)

  const [corridaDetalhe, setCorridaDetalhe] = useState(null)
  const [carregandoDetalhe, setCarregandoDetalhe] = useState(false)
  const [erroDetalhe, setErroDetalhe] = useState(null)

  // Estados da aba Logs (3.8.7 - RF20-S, RF24-S)
  const [logsEventos, setLogsEventos] = useState([])
  const [carregandoLogs, setCarregandoLogs] = useState(false)
  const [erroLogs, setErroLogs] = useState(null)
  const [filtroTipoLog, setFiltroTipoLog] = useState('')
  const [filtroOrigemLog, setFiltroOrigemLog] = useState('')
  const [filtroDataLog, setFiltroDataLog] = useState('')
  const [filtroBuscaLog, setFiltroBuscaLog] = useState('')
  const [logSelecionado, setLogSelecionado] = useState(null)
  const [contadorRecargaLogs, setContadorRecargaLogs] = useState(0)

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
  const dimensoesLabirintoRef = useRef(dimensoesLabirinto)

  function selecionarTipoLabirinto(largura, altura) {
    const novasDimensoes = { largura, altura }
    dimensoesLabirintoRef.current = novasDimensoes
    setDimensoesLabirinto(novasDimensoes)
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
      const resultado = interpretarFrameTelemetria(eventoWs.data, dimensoesLabirintoRef.current)
      if (!resultado.valido) {
        return
      }

      const { dados, dimensoes: novasDimensoes } = resultado

      ultimoPacoteRef.current = recebidoEm
      roboOnlineRef.current = true
      setUltimoPacoteEm(recebidoEm)
      setSegundosDesdeUltimoPacote(0)
      setRoboOnline(true)

      if (typeof dados.corrida === 'number') {
        setCorridaAtual(dados.corrida)
      }

      // Lê largura_celula e altura_celula em todo pacote válido
      dimensoesLabirintoRef.current = novasDimensoes
      setDimensoesLabirinto(novasDimensoes)

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

  const baseUrlLimpa = (apiBaseUrl || 'http://localhost:8000').replace(/\/+$/, '')

  // Carrega labirintos e histórico de corridas das rotas 3.8.2 (GET /labirintos e GET /corridas)
  useEffect(() => {
    if (abaAtiva !== 'historico' || typeof fetch !== 'function') {
      return undefined
    }
    let cancelado = false

    async function carregarHistorico() {
      setCarregandoHistorico(true)
      setErroHistorico(null)
      setErroLabirintos(null)

      const params = new URLSearchParams()
      if (filtroLabirintoId) {
        params.set('id_labirinto', String(filtroLabirintoId))
      }
      if (filtroStatus) {
        params.set('status', filtroStatus)
      }
      if (filtroTentativa) {
        params.set('num_tentativa', String(filtroTentativa))
      }
      if (filtroData) {
        params.set('data', filtroData)
      }
      const queryString = params.toString()
      const urlCorridas = `${baseUrlLimpa}/corridas${queryString ? `?${queryString}` : ''}`
      const urlLabirintos = `${baseUrlLimpa}/labirintos`

      try {
        const [resLabirintos, resCorridas] = await Promise.all([
          fetch(urlLabirintos).catch(() => null),
          fetch(urlCorridas),
        ])

        if (cancelado) {
          return
        }

        if (resLabirintos && typeof resLabirintos.json === 'function' && resLabirintos.ok) {
          const dadosLabirintos = await resLabirintos.json()
          if (!cancelado) {
            if (Array.isArray(dadosLabirintos) && dadosLabirintos.length > 0) {
              setLabirintosHistorico(dadosLabirintos)
              setErroLabirintos(null)
            } else {
              setLabirintosHistorico([])
              setErroLabirintos('Erro ao carregar labirintos')
            }
          }
        } else if (!cancelado) {
          setLabirintosHistorico([])
          setErroLabirintos('Erro ao carregar labirintos')
        }

        if (!resCorridas || !resCorridas.ok) {
          throw new Error(`Falha ao carregar histórico de corridas (HTTP ${resCorridas?.status ?? 'erro'})`)
        }

        const dadosCorridas = await resCorridas.json()
        if (!cancelado) {
          setCorridasHistorico(Array.isArray(dadosCorridas) ? dadosCorridas : [])
        }
      } catch (erro) {
        if (!cancelado) {
          setErroHistorico(erro?.message || 'Erro de rede ao consultar histórico de corridas')
        }
      } finally {
        if (!cancelado) {
          setCarregandoHistorico(false)
        }
      }
    }

    carregarHistorico()

    return () => {
      cancelado = true
    }
  }, [abaAtiva, baseUrlLimpa, filtroLabirintoId, filtroStatus, filtroTentativa, filtroData, contadorRecargaHistorico])

  const abortDetalheRef = useRef(null)

  useEffect(() => {
    return () => {
      if (abortDetalheRef.current) {
        abortDetalheRef.current.abort()
      }
    }
  }, [])

  async function abrirDetalhesCorrida(corridaResumo) {
    if (!corridaResumo?.id || typeof fetch !== 'function') {
      return
    }
    if (abortDetalheRef.current) {
      abortDetalheRef.current.abort()
    }
    const controller = typeof AbortController !== 'undefined' ? new AbortController() : null
    abortDetalheRef.current = controller

    setCarregandoDetalhe(true)
    setErroDetalhe(null)
    try {
      const opcoesFetch = controller ? { signal: controller.signal } : undefined
      const resposta = await fetch(
        `${baseUrlLimpa}/corridas/${corridaResumo.id}`,
        opcoesFetch,
      )
      if (controller?.signal.aborted) {
        return
      }
      if (!resposta || !resposta.ok) {
        throw new Error(`Não foi possível carregar os detalhes da corrida #${corridaResumo.id}`)
      }
      const dados = await resposta.json()
      if (controller?.signal.aborted || abortDetalheRef.current !== controller) {
        return
      }
      setCorridaDetalhe(dados)
    } catch (erro) {
      if (erro?.name === 'AbortError' || controller?.signal.aborted) {
        return
      }
      setErroDetalhe(erro?.message || 'Erro ao carregar detalhes da corrida')
    } finally {
      if (!controller || abortDetalheRef.current === controller) {
        setCarregandoDetalhe(false)
      }
    }
  }

  function limparFiltrosHistorico() {
    setFiltroLabirintoId('')
    setFiltroStatus('')
    setFiltroTentativa('')
    setFiltroData('')
  }

  // Carrega eventos e erros da rota 3.8.7 (GET /logs)
  useEffect(() => {
    if (abaAtiva !== 'logs' || typeof fetch !== 'function') {
      return undefined
    }
    let cancelado = false

    async function carregarLogs() {
      setCarregandoLogs(true)
      setErroLogs(null)

      const params = new URLSearchParams()
      if (filtroTipoLog) {
        params.set('tipo', filtroTipoLog)
      }
      if (filtroOrigemLog) {
        params.set('origem', filtroOrigemLog)
      }
      const queryString = params.toString()
      const urlLogs = `${baseUrlLimpa}/logs${queryString ? `?${queryString}` : ''}`

      try {
        const resposta = await fetch(urlLogs)
        if (!resposta || !resposta.ok) {
          throw new Error(`Falha ao consultar logs de eventos (HTTP ${resposta?.status ?? 'erro'})`)
        }
        const dados = await resposta.json()
        if (!cancelado) {
          setLogsEventos(Array.isArray(dados) ? dados : [])
        }
      } catch (erro) {
        if (!cancelado) {
          setErroLogs(erro?.message || 'Erro de rede ao consultar logs de eventos')
        }
      } finally {
        if (!cancelado) {
          setCarregandoLogs(false)
        }
      }
    }

    carregarLogs()

    return () => {
      cancelado = true
    }
  }, [abaAtiva, baseUrlLimpa, filtroTipoLog, filtroOrigemLog, contadorRecargaLogs])

  function limparFiltrosLogs() {
    setFiltroTipoLog('')
    setFiltroOrigemLog('')
    setFiltroDataLog('')
    setFiltroBuscaLog('')
  }

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

  const { ultimaCelulaInterna, pontoSaidaExterna } = calcularSaidaLabirinto(
    dimensoesLabirinto,
    statusCorrida === 'concluida',
  )
  const roboSaiuDoLabirinto = Boolean(ultimaCelulaInterna && pontoSaidaExterna)

  const codigoCelulaAtual = roboSaiuDoLabirinto
    ? `Saída (${pontoSaidaExterna.pos_x}, ${pontoSaidaExterna.pos_y})`
    : formatarCodigoCelula(roboPos.pos_x, roboPos.pos_y)
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

  // Cálculos derivados para a aba Histórico (3.8.5 / 3.8.2)
  const labirintosDisponiveis = labirintosHistorico

  const totalCorridasEncontradas = corridasHistorico.length
  const totalCorridasConcluidas = corridasHistorico.filter((c) => c.status === 'concluida').length
  const totalCorridasAbandonadas = corridasHistorico.filter((c) => c.status === 'abandonada').length
  const dadosMapaDetalhe = reconstruirDadosMapaCorrida(corridaDetalhe)
  const qtdCelulasDetalhe = Object.keys(dadosMapaDetalhe.celulas).length
  const leiturasDetalhe = Array.isArray(corridaDetalhe?.leituras) ? corridaDetalhe.leituras : []
  const tensaoInicialDetalhe =
    leiturasDetalhe.length > 0 && typeof leiturasDetalhe[0].tensao_bateria === 'number'
      ? leiturasDetalhe[0].tensao_bateria
      : null
  const tensaoFinalDetalhe =
    leiturasDetalhe.length > 0 &&
    typeof leiturasDetalhe[leiturasDetalhe.length - 1].tensao_bateria === 'number'
      ? leiturasDetalhe[leiturasDetalhe.length - 1].tensao_bateria
      : null
  const orientacaoFinalDetalhe =
    dadosMapaDetalhe.roboPos?.orientacao && NOMES_ORIENTACAO[dadosMapaDetalhe.roboPos.orientacao]
      ? NOMES_ORIENTACAO[dadosMapaDetalhe.roboPos.orientacao].nome
      : '--'

  // URL para exportação geral com base nos filtros da tabela
  const paramsCorridasCsv = new URLSearchParams()
  if (filtroLabirintoId) {
    paramsCorridasCsv.set('id_labirinto', String(filtroLabirintoId))
  }
  if (filtroStatus) {
    paramsCorridasCsv.set('status', filtroStatus)
  }
  if (filtroTentativa) {
    paramsCorridasCsv.set('num_tentativa', String(filtroTentativa))
  }
  if (filtroData) {
    paramsCorridasCsv.set('data', filtroData)
  }
  const queryCorridasCsv = paramsCorridasCsv.toString()
  const urlExportarCorridasCsv = `${baseUrlLimpa}/exportacao/corridas.csv${
    queryCorridasCsv ? `?${queryCorridasCsv}` : ''
  }`

  // URL para exportação exclusiva da corrida aberta em Detalhes
  const urlExportarCorridaDetalheCsv = corridaDetalhe?.id
    ? `${baseUrlLimpa}/exportacao/corridas.csv?id_corrida=${encodeURIComponent(corridaDetalhe.id)}`
    : ''

  // Cálculos derivados para a aba Logs (3.8.7)
  const logsFiltrados = logsEventos.filter((log) => {
    if (filtroTipoLog && log.tipo !== filtroTipoLog) {
      return false
    }
    if (filtroOrigemLog && log.origem !== filtroOrigemLog) {
      return false
    }
    if (filtroDataLog && extrairDataIso(log.ocorrido_em) !== filtroDataLog) {
      return false
    }
    if (filtroBuscaLog.trim()) {
      const termo = filtroBuscaLog.trim().toLowerCase()
      const textoBusca = `${log.mensagem || ''} ${log.origem || ''} ${log.tipo || ''} ${
        log.id_corrida ?? ''
      }`.toLowerCase()
      if (!textoBusca.includes(termo)) {
        return false
      }
    }
    return true
  })

  const totalLogsAvisos = logsFiltrados.filter((l) => l.tipo === 'aviso').length
  const totalLogsErros = logsFiltrados.filter((l) => l.tipo === 'erro').length
  const paramsLogsCsv = new URLSearchParams()
  if (filtroTipoLog) {
    paramsLogsCsv.set('tipo', filtroTipoLog)
  }
  if (filtroOrigemLog) {
    paramsLogsCsv.set('origem', filtroOrigemLog)
  }
  if (filtroDataLog) {
    paramsLogsCsv.set('data', filtroDataLog)
  }
  if (filtroBuscaLog.trim()) {
    paramsLogsCsv.set('busca', filtroBuscaLog.trim())
  }
  const queryLogsCsv = paramsLogsCsv.toString()
  const urlExportarLogsCsv = `${baseUrlLimpa}/exportacao/logs.csv${
    queryLogsCsv ? `?${queryLogsCsv}` : ''
  }`

  return (
    <div className="painel-layout">
      {ultimoPacoteEm !== null && !roboOnline && <ConnectionAlert timeoutMs={TIMEOUT_OFFLINE_MS} />}
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
                    : 'Logs e Eventos'}
            </h1>
            <p className="subtitulo-pagina">
              {abaAtiva === 'dashboard'
                ? 'Acompanhamento geral do Micromouse'
                : abaAtiva === 'monitoramento'
                  ? 'Acompanhamento da execução e telemetria do Micromouse'
                  : abaAtiva === 'historico'
                    ? 'Histórico de tentativas e trajetos anteriores'
                    : 'Depuração e auditoria de eventos e erros do sistema'}
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
              <article
                className={`card-topo ${!onlineAtivo && ultimoPacoteEm !== null ? 'card-alerta-conexao' : ''}`}
                data-testid="card-status-robo"
              >
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
              <article
                className={`card-topo ${alertaBateriaCritica ? 'card-bateria-critica' : ''}`}
                data-testid="card-bateria-visao-geral"
              >
                <div
                  className={`icone-quadrado ${alertaBateriaCritica ? 'vermelho' : 'azul-claro-icone'}`}
                  aria-hidden="true"
                >
                  ⚡
                </div>
                <div className="card-topo-conteudo card-conteudo-bateria-topo">
                  <div className="card-topo-cabecalho-bateria">
                    <span className="card-topo-rotulo">Bateria</span>
                    <span
                      className={`tensao-badge ${alertaBateriaCritica ? 'critica' : ''}`}
                      data-testid="tensao-bateria-visao-geral"
                    >
                      {formatarDecimalBr(tensaoBateria, 2)} V
                    </span>
                  </div>
                  <strong
                    className={`card-topo-valor ${alertaBateriaCritica ? 'texto-vermelho' : ''}`}
                    data-testid="porcentagem-bateria-visao-geral"
                  >
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
                  {alertaBateriaCritica ? (
                    <span
                      className="card-topo-sub alerta-critico-texto"
                      data-testid="alerta-bateria-critica-visao-geral"
                    >
                      <span className="ponto-inline vermelho" /> ⚠ Bateria crítica ({formatarDecimalBr(tensaoBateria, 2)} V — rearme em {TEXTO_REARME_BATERIA} V)
                    </span>
                  ) : (
                    <span className="card-topo-sub" data-testid="status-bateria-normal-visao-geral">
                      <span className="ponto-inline neutro" /> Nível normal
                    </span>
                  )}
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
                    data-ultima-celula={
                      ultimaCelulaInterna ? `${ultimaCelulaInterna.pos_x},${ultimaCelulaInterna.pos_y}` : ''
                    }
                    data-saida-labirinto={
                      pontoSaidaExterna ? `${pontoSaidaExterna.pos_x},${pontoSaidaExterna.pos_y}` : ''
                    }
                  >
                    {dimensoesLabirinto ? (
                      <MazeMap
                        largura={dimensoesLabirinto.largura}
                        altura={dimensoesLabirinto.altura}
                        celulas={mapaCelulas}
                        roboPos={roboPos}
                        trajeto={trajeto}
                        ultimaCelula={ultimaCelulaInterna}
                        pontoSaida={pontoSaidaExterna}
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

                      {roboSaiuDoLabirinto && ultimaCelulaInterna && (
                        <div className="pilula-saida-labirinto" data-testid="indicador-saida-painel">
                          ✓ Saída do mapa em ({pontoSaidaExterna.pos_x}, {pontoSaidaExterna.pos_y}) · Última célula:{' '}
                          {formatarCodigoCelula(ultimaCelulaInterna.pos_x, ultimaCelulaInterna.pos_y)} ({ultimaCelulaInterna.pos_x},{' '}
                          {ultimaCelulaInterna.pos_y})
                        </div>
                      )}

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
                    <span
                      className={`icone-indicador ${alertaBateriaCritica ? 'vermelho' : 'verde'}`}
                      aria-hidden="true"
                    >
                      🔋
                    </span>
                    <h3 className="card-indicador-titulo">Bateria</h3>
                    <span
                      className={`tensao-badge ${alertaBateriaCritica ? 'critica' : ''}`}
                      data-testid="tensao-bateria"
                    >
                      {formatarDecimalBr(tensaoBateria, 2)} V
                    </span>
                  </div>

                  <div className="bateria-linha-principal">
                    <strong
                      className={`bateria-porcentagem ${alertaBateriaCritica ? 'texto-vermelho' : ''}`}
                      data-testid="porcentagem-bateria"
                    >
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
                        ⚠ Bateria crítica ({formatarDecimalBr(tensaoBateria, 2)} V — rearme em {TEXTO_REARME_BATERIA} V)
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
                      {roboSaiuDoLabirinto
                        ? `Saída (${pontoSaidaExterna.pos_x}, ${pontoSaidaExterna.pos_y}) · Última: ${formatarCodigoCelula(ultimaCelulaInterna.pos_x, ultimaCelulaInterna.pos_y)} (${ultimaCelulaInterna.pos_x}, ${ultimaCelulaInterna.pos_y})`
                        : `${codigoCelulaAtual} (${roboPos.pos_x}, ${roboPos.pos_y})`}
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
            ABA 3: HISTÓRICO E CONSULTAS (3.8.5 / 3.8.2 - historico.png e detalhes.png)
           ========================================================================= */}
        {abaAtiva === 'historico' && (
          <div className="secao-historico-container" data-testid="tela-historico">
            {corridaDetalhe ? (
              /* Subtela: Detalhes da Corrida (detalhes.png / detalhes_4x4.png, CT-S07 e CT-S08) */
              <div className="detalhes-corrida-view" data-testid="tela-detalhes-corrida">
                <div className="cabecalho-detalhes-corrida">
                  <div>
                    <div className="breadcrumb-historico">
                      <span>Histórico</span>
                      <span aria-hidden="true">›</span>
                      <strong>Corrida #{String(corridaDetalhe.id).padStart(3, '0')}</strong>
                    </div>
                    <h2 className="card-titulo">Detalhes da Corrida</h2>
                    <p className="card-subtitulo">
                      Visualize os dados completos e o trajeto da execução selecionada
                    </p>
                  </div>

                  <div className="acoes-cabecalho-historico">
                    <a
                      href={urlExportarCorridaDetalheCsv}
                      download={`corrida_${corridaDetalhe.id}.csv`}
                      className="botao-acao-secundario"
                      data-testid="botao-exportar-csv-detalhe"
                    >
                      Exportar Corrida
                    </a>
                    <button
                      type="button"
                      className="botao-acao-secundario"
                      onClick={() => window.print?.()}
                      data-testid="botao-exportar-pdf-detalhe"
                    >
                      Exportar PDF
                    </button>
                    <button
                      type="button"
                      className="botao-voltar-historico"
                      onClick={() => setCorridaDetalhe(null)}
                      data-testid="botao-voltar-historico"
                    >
                      ← Voltar ao histórico
                    </button>
                  </div>
                </div>

                {/* Faixa de resumo superior de Detalhes da Corrida */}
                <section className="card-painel faixa-resumo-detalhes" aria-label="Resumo da corrida selecionada">
                  <div className="item-resumo-detalhe">
                    <span className="rotulo-caixa-alta">ID DA CORRIDA</span>
                    <strong data-testid="detalhe-id-corrida">
                      #{String(corridaDetalhe.id).padStart(3, '0')}
                    </strong>
                  </div>
                  <div className="item-resumo-detalhe">
                    <span className="rotulo-caixa-alta">DATA E HORA</span>
                    <strong data-testid="detalhe-data-hora">
                      {formatarDataHoraBr(corridaDetalhe.iniciado_em)}
                    </strong>
                  </div>
                  <div className="item-resumo-detalhe">
                    <span className="rotulo-caixa-alta">LABIRINTO</span>
                    <strong data-testid="detalhe-labirinto">
                      {corridaDetalhe.largura_celula} × {corridaDetalhe.altura_celula} ({corridaDetalhe.labirinto})
                    </strong>
                  </div>
                  <div className="item-resumo-detalhe">
                    <span className="rotulo-caixa-alta">TENTATIVA</span>
                    <strong data-testid="detalhe-tentativa">{corridaDetalhe.num_tentativa}</strong>
                  </div>
                  <div className="item-resumo-detalhe">
                    <span className="rotulo-caixa-alta">RESULTADO</span>
                    <span
                      className={`pilula-resultado ${
                        corridaDetalhe.status === 'concluida'
                          ? 'concluida'
                          : corridaDetalhe.status === 'abandonada'
                            ? 'abandonada'
                            : 'em_andamento'
                      }`}
                      data-testid="detalhe-resultado"
                    >
                      <span className="ponto-inline" />
                      {corridaDetalhe.status === 'concluida'
                        ? 'Concluído'
                        : corridaDetalhe.status === 'abandonada'
                          ? 'Não concluído'
                          : 'Em andamento'}
                    </span>
                  </div>
                </section>

                {/* Grade Central: Trajeto no Labirinto (<MazeMap />) + Cards de Desempenho e Bateria */}
                <section className="grade-detalhes-corrida">
                  <article className="card-painel" aria-label="Trajeto no Labirinto">
                    <div className="card-mapa-cabecalho">
                      <div>
                        <h3 className="card-titulo">Trajeto no Labirinto</h3>
                        <p className="card-subtitulo">
                          Mapa {corridaDetalhe.largura_celula} × {corridaDetalhe.altura_celula} · paredes e
                          exploração registradas
                        </p>
                      </div>
                      <span className="tensao-badge" data-testid="detalhe-orientacao-badge">
                        {orientacaoFinalDetalhe}
                      </span>
                    </div>

                    <div
                      className="container-maze-map"
                      data-testid="espaco-mapa-detalhe-corrida"
                      data-celulas-count={qtdCelulasDetalhe}
                      data-trajeto-count={dadosMapaDetalhe.trajeto.length}
                      data-trajeto={dadosMapaDetalhe.trajeto.map((p) => `${p.pos_x},${p.pos_y}`).join(';')}
                    >
                      {dadosMapaDetalhe.dimensoes ? (
                        <MazeMap
                          largura={dadosMapaDetalhe.dimensoes.largura}
                          altura={dadosMapaDetalhe.dimensoes.altura}
                          celulas={dadosMapaDetalhe.celulas}
                          roboPos={dadosMapaDetalhe.roboPos}
                          trajeto={dadosMapaDetalhe.trajeto}
                          ultimaCelula={dadosMapaDetalhe.ultimaCelula}
                          pontoSaida={dadosMapaDetalhe.pontoSaida}
                        />
                      ) : (
                        <p className="texto-secundario">Dimensões de labirinto indisponíveis para renderização.</p>
                      )}
                    </div>
                  </article>

                  <div className="coluna-cards-detalhe">
                    <article className="card-painel" data-testid="card-desempenho-detalhe">
                      <h3 className="card-indicador-titulo">Desempenho</h3>
                      <div className="linhas-duplas-indicador">
                        <div className="linha-indicador">
                          <span>Tempo de conclusão</span>
                          <strong data-testid="detalhe-tempo-conclusao">
                            {corridaDetalhe.tempo_conc_s !== null && corridaDetalhe.tempo_conc_s !== undefined
                              ? `${formatarCronometro(corridaDetalhe.tempo_conc_s * 1000)} (${formatarDecimalBr(corridaDetalhe.tempo_conc_s, 2)} s)`
                              : corridaDetalhe.duracao_s !== null && corridaDetalhe.duracao_s !== undefined
                                ? `${formatarCronometro(corridaDetalhe.duracao_s * 1000)} (${formatarDecimalBr(corridaDetalhe.duracao_s, 2)} s)`
                                : '--'}
                          </strong>
                        </div>
                        <div className="linha-indicador">
                          <span>Velocidade média</span>
                          <strong data-testid="detalhe-velocidade-media">
                            {corridaDetalhe.velocidade_media !== null &&
                            corridaDetalhe.velocidade_media !== undefined
                              ? `${formatarDecimalBr(corridaDetalhe.velocidade_media, 2)} m/s`
                              : '--'}
                          </strong>
                        </div>
                        <div className="linha-indicador">
                          <span>Células percorridas</span>
                          <strong data-testid="detalhe-celulas-percorridas">{qtdCelulasDetalhe}</strong>
                        </div>
                        <div className="linha-indicador">
                          <span>Desafio cumprido</span>
                          <strong className={corridaDetalhe.status === 'concluida' ? 'texto-verde' : 'texto-vermelho'}>
                            {corridaDetalhe.status === 'concluida' ? 'Sim' : 'Não'}
                          </strong>
                        </div>
                      </div>
                    </article>

                    <article className="card-painel" data-testid="card-bateria-detalhe">
                      <h3 className="card-indicador-titulo">Bateria e Telemetria</h3>
                      <div className="linhas-duplas-indicador">
                        <div className="linha-indicador">
                          <span>Tensão inicial</span>
                          <strong>
                            {tensaoInicialDetalhe !== null ? `${formatarDecimalBr(tensaoInicialDetalhe, 2)} V` : '--'}
                          </strong>
                        </div>
                        <div className="linha-indicador">
                          <span>Tensão final</span>
                          <strong>
                            {tensaoFinalDetalhe !== null ? `${formatarDecimalBr(tensaoFinalDetalhe, 2)} V` : '--'}
                          </strong>
                        </div>
                        <div className="linha-indicador">
                          <span>Consumo médio</span>
                          <strong data-testid="detalhe-consumo-medio">
                            {corridaDetalhe.consumo_medio_ma !== null &&
                            corridaDetalhe.consumo_medio_ma !== undefined
                              ? `${formatarDecimalBr(corridaDetalhe.consumo_medio_ma, 1)} mA`
                              : '--'}
                          </strong>
                        </div>
                        <div className="linha-indicador">
                          <span>Leituras registradas</span>
                          <strong data-testid="detalhe-total-leituras">
                            {corridaDetalhe.total_leituras ?? leiturasDetalhe.length}
                          </strong>
                        </div>
                      </div>
                    </article>
                  </div>
                </section>
              </div>
            ) : (
              /* Visão Principal: Consulta Geral dos Resultados e Filtro por Labirinto (historico.png) */
              <>
                {/* Cards de Resumo da Consulta Geral e Controle de Tentativas por Labirinto (RF09-S e RF10-S) */}
                <section className="grade-resumo-topo" aria-label="Resumo do histórico de corridas">
                  <article className="card-topo" data-testid="card-resumo-total-corridas">
                    <div className="icone-quadrado petroleo" aria-hidden="true">
                      ◷
                    </div>
                    <div className="card-topo-conteudo">
                      <span className="card-topo-rotulo">Total de Corridas</span>
                      <strong className="card-topo-valor" data-testid="valor-total-corridas-historico">
                        {totalCorridasEncontradas}
                      </strong>
                      <span className="card-topo-sub">Consulta geral e filtrada</span>
                    </div>
                  </article>

                  <article className="card-topo" data-testid="card-resumo-concluidas">
                    <div className="icone-quadrado verde" aria-hidden="true">
                      ✓
                    </div>
                    <div className="card-topo-conteudo">
                      <span className="card-topo-rotulo">Concluídas</span>
                      <strong className="card-topo-valor texto-verde" data-testid="valor-corridas-concluidas">
                        {totalCorridasConcluidas}
                      </strong>
                      <span className="card-topo-sub">Objetivo alcançado</span>
                    </div>
                  </article>

                  <article className="card-topo" data-testid="card-resumo-abandonadas">
                    <div className="icone-quadrado vermelho" aria-hidden="true">
                      ✕
                    </div>
                    <div className="card-topo-conteudo">
                      <span className="card-topo-rotulo">Não Concluídas</span>
                      <strong className="card-topo-valor texto-vermelho" data-testid="valor-corridas-abandonadas">
                        {totalCorridasAbandonadas}
                      </strong>
                      <span className="card-topo-sub">Abandonadas ou interrompidas</span>
                    </div>
                  </article>

                  <article className="card-topo" data-testid="card-tentativas-por-labirinto">
                    <div className="icone-quadrado neutro" aria-hidden="true">
                      ⊞
                    </div>
                    <div className="card-topo-conteudo">
                      <span className="card-topo-rotulo">Tentativas por Labirinto</span>
                      {erroLabirintos ? (
                        <span className="texto-vermelho" data-testid="falha-resumo-labirintos">
                          Erro ao carregar labirintos
                        </span>
                      ) : (
                        <div className="lista-chips-labirintos" data-testid="resumo-tentativas-labirintos">
                          {labirintosDisponiveis.map((lab) => {
                            const dimTexto = `${lab.largura_celula} × ${lab.altura_celula}`
                            const ativo = String(filtroLabirintoId) === String(lab.id)
                            return (
                              <button
                                key={lab.id}
                                type="button"
                                className={`chip-labirinto-tentativa ${ativo ? 'ativo' : ''}`}
                                onClick={() =>
                                  setFiltroLabirintoId(ativo ? '' : String(lab.id))
                                }
                                data-testid={`chip-labirinto-${lab.largura_celula}x${lab.altura_celula}`}
                              >
                                <strong>{dimTexto}:</strong> {lab.total_tentativas ?? 0}
                              </button>
                            )
                          })}
                        </div>
                      )}
                    </div>
                  </article>
                </section>

                {/* Painel "Filtrar execuções" (fiel a historico.png e CT-S10 / CT-S11 / CT-S12) */}
                <section className="card-painel painel-filtros-historico" aria-label="Filtrar execuções">
                  <div className="cabecalho-filtros-historico">
                    <div>
                      <h2 className="card-titulo-pequeno">Filtrar execuções</h2>
                      <p className="card-subtitulo">Refine os dados exibidos na tabela</p>
                    </div>
                    <div className="acoes-filtros-historico">
                      <button
                        type="button"
                        className="botao-acao-secundario"
                        onClick={() => setContadorRecargaHistorico((c) => c + 1)}
                        data-testid="botao-atualizar-historico"
                      >
                        ↻ Atualizar
                      </button>
                      <button
                        type="button"
                        className="botao-acao-secundario"
                        onClick={limparFiltrosHistorico}
                        data-testid="botao-limpar-filtros-historico"
                      >
                        Limpar filtros
                      </button>
                    </div>
                  </div>

                  <div className="grade-filtros-historico">
                    <label className="campo-filtro">
                      <span className="rotulo-caixa-alta">LABIRINTO</span>
                      <select
                        aria-label="Labirinto"
                        data-testid="filtro-labirinto"
                        value={filtroLabirintoId}
                        onChange={(e) => setFiltroLabirintoId(e.target.value)}
                      >
                        <option value="">Todos</option>
                        {erroLabirintos ? (
                          <option value="" disabled data-testid="erro-labirintos-opcao">
                            Erro ao carregar labirintos
                          </option>
                        ) : (
                          labirintosDisponiveis.map((lab) => (
                            <option key={lab.id} value={String(lab.id)}>
                              {lab.largura_celula} × {lab.altura_celula} ({lab.nome})
                            </option>
                          ))
                        )}
                      </select>
                      {erroLabirintos && (
                        <span className="texto-erro-campo" role="alert" data-testid="alerta-erro-labirintos">
                          ⚠ {erroLabirintos}
                        </span>
                      )}
                    </label>

                    <label className="campo-filtro">
                      <span className="rotulo-caixa-alta">RESULTADO</span>
                      <select
                        aria-label="Resultado"
                        data-testid="filtro-resultado"
                        value={filtroStatus}
                        onChange={(e) => setFiltroStatus(e.target.value)}
                      >
                        <option value="">Todos</option>
                        <option value="concluida">Concluído</option>
                        <option value="abandonada">Não concluído</option>
                        <option value="em_andamento">Em andamento</option>
                      </select>
                    </label>

                    <label className="campo-filtro">
                      <span className="rotulo-caixa-alta">TENTATIVA</span>
                      <select
                        aria-label="Tentativa"
                        data-testid="filtro-tentativa"
                        value={filtroTentativa}
                        onChange={(e) => setFiltroTentativa(e.target.value)}
                      >
                        <option value="">Todas</option>
                        {[1, 2, 3, 4, 5, 6, 7, 8, 9, 10].map((num) => (
                          <option key={num} value={String(num)}>
                            Tentativa {num}
                          </option>
                        ))}
                      </select>
                    </label>

                    <label className="campo-filtro">
                      <span className="rotulo-caixa-alta">DATA DA EXECUÇÃO</span>
                      <input
                        type="date"
                        aria-label="Data da execução"
                        data-testid="filtro-data-execucao"
                        value={filtroData}
                        onChange={(e) => setFiltroData(e.target.value)}
                      />
                    </label>
                  </div>
                </section>

                {/* Seção "Execuções Registradas" (Consulta Geral dos Resultados) */}
                <section className="card-painel" aria-label="Execuções Registradas">
                  <div className="cabecalho-execucoes-registradas">
                    <div>
                      <h2 className="card-titulo">Execuções Registradas</h2>
                      <p className="contador-corridas-encontradas" data-testid="contador-corridas-encontradas">
                        <span className="ponto-inline verde" /> {totalCorridasEncontradas} corridas encontradas
                      </p>
                    </div>

                    <div className="acoes-cabecalho-historico">
                      <button
                        type="button"
                        className="botao-acao-secundario"
                        onClick={() => window.print?.()}
                        data-testid="botao-exportar-pdf"
                      >
                        Exportar PDF
                      </button>
                      <a
                        href={urlExportarCorridasCsv}
                        download="corridas.csv"
                        className="botao-acao-secundario"
                        data-testid="botao-exportar-csv"
                      >
                        Exportar CSV
                      </a>
                      <button
                        type="button"
                        className="botao-voltar"
                        onClick={() => setAbaAtiva('dashboard')}
                      >
                        Voltar para a Visão Geral
                      </button>
                    </div>
                  </div>

                  {erroHistorico && (
                    <div className="alerta-erro-consulta" role="alert" data-testid="erro-historico">
                      <span>⚠ {erroHistorico}</span>
                      <button
                        type="button"
                        className="botao-acao-secundario"
                        onClick={() => setContadorRecargaHistorico((c) => c + 1)}
                      >
                        Tentar novamente
                      </button>
                    </div>
                  )}

                  {erroDetalhe && (
                    <div className="alerta-erro-consulta" role="alert" data-testid="erro-detalhe-corrida">
                      <span>⚠ {erroDetalhe}</span>
                    </div>
                  )}

                  {carregandoDetalhe && (
                    <div className="estado-carregando-consulta" data-testid="carregando-detalhe-corrida">
                      Carregando detalhes da corrida...
                    </div>
                  )}

                  {carregandoHistorico ? (
                    <div className="estado-carregando-consulta" data-testid="carregando-historico">
                      Carregando dados do histórico...
                    </div>
                  ) : corridasHistorico.length === 0 ? (
                    filtroLabirintoId ? (
                      <div className="estado-vazio-historico" data-testid="estado-labirinto-sem-registros">
                        <span className="icone-secao-grande" aria-hidden="true">
                          ⊞
                        </span>
                        <strong className="titulo-vazio-historico">Labirinto sem registros</strong>
                        <p className="texto-secundario">
                          Nenhuma corrida registrada para este labirinto.
                        </p>
                      </div>
                    ) : (
                      <div className="estado-vazio-historico" data-testid="estado-historico-vazio">
                        <span className="icone-secao-grande" aria-hidden="true">
                          ◷
                        </span>
                        <strong className="titulo-vazio-historico">Nenhuma corrida encontrada</strong>
                        <p className="texto-secundario">
                          Nenhuma corrida registrada para os filtros selecionados.
                        </p>
                      </div>
                    )
                  ) : (
                    <>
                      <div className="tabela-historico-container">
                        <table className="tabela-historico" data-testid="tabela-execucoes-registradas">
                          <thead>
                            <tr>
                              <th>DATA E HORA</th>
                              <th>LABIRINTO</th>
                              <th>TENTATIVA</th>
                              <th>TEMPO DE CONCLUSÃO</th>
                              <th>VELOCIDADE MÉDIA</th>
                              <th>CONSUMO DE BATERIA</th>
                              <th>RESULTADO</th>
                              <th>AÇÕES</th>
                            </tr>
                          </thead>
                          <tbody>
                            {corridasHistorico.map((corrida) => {
                              const tempoSegundos = corrida.tempo_conc_s ?? corrida.duracao_s
                              const dimensaoTexto = `${corrida.largura_celula} × ${corrida.altura_celula}`
                              return (
                                <tr key={corrida.id} data-testid={`linha-corrida-${corrida.id}`}>
                                  <td>{formatarDataHoraBr(corrida.iniciado_em)}</td>
                                  <td>
                                    <span className="celula-labirinto-tabela">
                                      <span aria-hidden="true">⊞</span>
                                      <strong>{dimensaoTexto}</strong>
                                      {corrida.labirinto &&
                                      corrida.labirinto !== `${corrida.largura_celula}x${corrida.altura_celula}` ? (
                                        <span className="texto-secundario">({corrida.labirinto})</span>
                                      ) : null}
                                    </span>
                                  </td>
                                  <td>Tentativa {corrida.num_tentativa}</td>
                                  <td>
                                    {tempoSegundos !== null && tempoSegundos !== undefined
                                      ? `${formatarCronometro(tempoSegundos * 1000)} (${formatarDecimalBr(tempoSegundos, 2)} s)`
                                      : '--'}
                                  </td>
                                  <td>
                                    {corrida.velocidade_media !== null && corrida.velocidade_media !== undefined
                                      ? `${formatarDecimalBr(corrida.velocidade_media, 2)} m/s`
                                      : '--'}
                                  </td>
                                  <td>
                                    {corrida.consumo_medio_ma !== null && corrida.consumo_medio_ma !== undefined
                                      ? `${formatarDecimalBr(corrida.consumo_medio_ma, 1)} mA`
                                      : '--'}
                                  </td>
                                  <td>
                                    <span
                                      className={`pilula-resultado ${
                                        corrida.status === 'concluida'
                                          ? 'concluida'
                                          : corrida.status === 'abandonada'
                                            ? 'abandonada'
                                            : 'em_andamento'
                                      }`}
                                    >
                                      <span className="ponto-inline" />
                                      {corrida.status === 'concluida'
                                        ? 'Concluído'
                                        : corrida.status === 'abandonada'
                                          ? 'Não concluído'
                                          : 'Em andamento'}
                                    </span>
                                  </td>
                                  <td>
                                    <button
                                      type="button"
                                      className="botao-ver-detalhes-linha"
                                      onClick={() => abrirDetalhesCorrida(corrida)}
                                      data-testid={`botao-detalhes-corrida-${corrida.id}`}
                                    >
                                      Ver detalhes <span aria-hidden="true">→</span>
                                    </button>
                                  </td>
                                </tr>
                              )
                            })}
                          </tbody>
                        </table>
                      </div>

                      <div className="rodape-tabela-historico" data-testid="rodape-tabela-historico">
                        <span className="texto-secundario">
                          Exibindo os últimos {corridasHistorico.length} registros (limite de 100 por consulta)
                        </span>
                        <span className="texto-secundario">Consulta sob demanda ao banco de dados</span>
                      </div>
                    </>
                  )}
                </section>
              </>
            )}
          </div>
        )}

        {/* =========================================================================
            ABA 4: LOGS E EVENTOS (3.8.7 - logs.png e logs_erro.png, RF20-S / RF24-S)
           ========================================================================= */}
        {abaAtiva === 'logs' && (
          <div className="secao-logs-container" data-testid="tela-logs">
            {/* Faixa Superior de 4 Cards da aba Logs */}
            <section className="grade-resumo-topo" aria-label="Resumo de logs e eventos">
              <article className="card-topo" data-testid="card-logs-total">
                <div className="icone-quadrado azul-claro-icone" aria-hidden="true">
                  ☰
                </div>
                <div className="card-topo-conteudo">
                  <span className="card-topo-rotulo">Eventos registrados</span>
                  <strong className="card-topo-valor" data-testid="valor-logs-total">
                    {logsFiltrados.length}
                  </strong>
                </div>
              </article>

              <article className="card-topo" data-testid="card-logs-avisos">
                <div className="icone-quadrado neutro" aria-hidden="true">
                  ⚠
                </div>
                <div className="card-topo-conteudo">
                  <span className="card-topo-rotulo">Avisos</span>
                  <strong className="card-topo-valor texto-ambar" data-testid="valor-logs-avisos">
                    {totalLogsAvisos}
                  </strong>
                </div>
              </article>

              <article className="card-topo" data-testid="card-logs-erros">
                <div className="icone-quadrado vermelho" aria-hidden="true">
                  ✕
                </div>
                <div className="card-topo-conteudo">
                  <span className="card-topo-rotulo">Erros</span>
                  <strong className="card-topo-valor texto-vermelho" data-testid="valor-logs-erros">
                    {totalLogsErros}
                  </strong>
                </div>
              </article>

              <article className="card-topo" data-testid="card-logs-comunicacao">
                <div className={`icone-quadrado ${onlineAtivo ? 'verde' : 'vermelho'}`} aria-hidden="true">
                  ((•))
                </div>
                <div className="card-topo-conteudo">
                  <span className="card-topo-rotulo">Status da Comunicação</span>
                  <strong className={`card-topo-valor ${onlineAtivo ? 'texto-verde' : 'texto-vermelho'}`}>
                    {onlineAtivo ? 'Normal' : 'Offline'}
                  </strong>
                  <span className="card-topo-sub">
                    {ultimoPacoteEm !== null
                      ? `Último pacote recebido há ${formatarDecimalBr(segundosDesdeUltimoPacote, 1)} s`
                      : 'Aguardando telemetria'}
                  </span>
                </div>
              </article>
            </section>

            {/* Painel "Filtros de consulta" (fiel a logs.png e CT-S14) */}
            <section className="card-painel painel-filtros-historico" aria-label="Filtros de consulta de logs">
              <div className="cabecalho-filtros-historico">
                <div>
                  <h2 className="card-titulo-pequeno">Filtros de consulta</h2>
                  <p className="card-subtitulo">Exibindo todas as origens e níveis</p>
                </div>
                <div className="acoes-filtros-historico">
                  <a
                    href={urlExportarLogsCsv}
                    download="logs.csv"
                    className="botao-acao-secundario"
                    data-testid="botao-exportar-logs-csv"
                  >
                    Exportar CSV
                  </a>
                  <button
                    type="button"
                    className="botao-acao-secundario"
                    onClick={limparFiltrosLogs}
                    data-testid="botao-limpar-filtros-logs"
                  >
                    Limpar filtros
                  </button>
                </div>
              </div>

              <div className="grade-filtros-historico">
                <label className="campo-filtro">
                  <span className="rotulo-caixa-alta">TIPO DE EVENTO</span>
                  <select
                    aria-label="Tipo de evento"
                    data-testid="filtro-tipo-log"
                    value={filtroTipoLog}
                    onChange={(e) => setFiltroTipoLog(e.target.value)}
                  >
                    <option value="">Todos</option>
                    <option value="info">Informação</option>
                    <option value="aviso">Aviso</option>
                    <option value="erro">Erro</option>
                  </select>
                </label>

                <label className="campo-filtro">
                  <span className="rotulo-caixa-alta">ORIGEM</span>
                  <select
                    aria-label="Origem"
                    data-testid="filtro-origem-log"
                    value={filtroOrigemLog}
                    onChange={(e) => setFiltroOrigemLog(e.target.value)}
                  >
                    <option value="">Todos</option>
                    <option value="firmware">Firmware</option>
                    <option value="backend">Backend</option>
                    <option value="frontend">Frontend</option>
                  </select>
                </label>

                <label className="campo-filtro">
                  <span className="rotulo-caixa-alta">DATA DA OCORRÊNCIA</span>
                  <input
                    type="date"
                    aria-label="Data da ocorrência"
                    data-testid="filtro-data-log"
                    value={filtroDataLog}
                    onChange={(e) => setFiltroDataLog(e.target.value)}
                  />
                </label>

                <label className="campo-filtro">
                  <span className="rotulo-caixa-alta">BUSCA</span>
                  <input
                    type="search"
                    aria-label="Busca"
                    placeholder="Buscar nos logs..."
                    data-testid="filtro-busca-log"
                    value={filtroBuscaLog}
                    onChange={(e) => setFiltroBuscaLog(e.target.value)}
                  />
                </label>
              </div>
            </section>

            {/* Tabela "Registro de Eventos" + Painel Lateral "Detalhes do evento" (logs_erro.png / CT-S03) */}
            <div className={logSelecionado ? 'grade-logs-com-detalhe' : ''}>
              <section className="card-painel" aria-label="Registro de Eventos">
                <div className="cabecalho-execucoes-registradas">
                  <div>
                    <h2 className="card-titulo">Registro de Eventos</h2>
                    <p className="contador-corridas-encontradas" data-testid="contador-logs-encontrados">
                      {logsFiltrados.length} eventos encontrados
                    </p>
                  </div>

                  <div className="acoes-cabecalho-historico">
                    <button
                      type="button"
                      className="botao-acao-secundario"
                      onClick={() => setContadorRecargaLogs((c) => c + 1)}
                      data-testid="botao-atualizar-logs"
                    >
                      ↻ Atualizar
                    </button>
                    <button
                      type="button"
                      className="botao-voltar"
                      onClick={() => setAbaAtiva('dashboard')}
                    >
                      Voltar para a Visão Geral
                    </button>
                  </div>
                </div>

                {erroLogs && (
                  <div className="alerta-erro-consulta" role="alert" data-testid="erro-logs">
                    <span>⚠ {erroLogs}</span>
                  </div>
                )}

                {carregandoLogs ? (
                  <div className="estado-carregando-consulta" data-testid="carregando-logs">
                    Carregando registro de eventos...
                  </div>
                ) : logsFiltrados.length === 0 ? (
                  <div className="estado-vazio-historico" data-testid="estado-logs-vazio">
                    <span className="icone-secao-grande" aria-hidden="true">
                      &gt;_
                    </span>
                    <strong className="titulo-vazio-historico">Nenhum evento encontrado</strong>
                    <p className="texto-secundario">
                      Não há registros de log para os filtros informados.
                    </p>
                  </div>
                ) : (
                  <div className="tabela-historico-container">
                    <table className="tabela-historico" data-testid="tabela-registro-eventos">
                      <thead>
                        <tr>
                          <th>DATA E HORA</th>
                          <th>NÍVEL</th>
                          <th>ORIGEM</th>
                          <th>EVENTO</th>
                          <th>CORRIDA</th>
                          <th>DETALHES</th>
                        </tr>
                      </thead>
                      <tbody>
                        {logsFiltrados.map((log) => {
                          const selecionado = logSelecionado?.id === log.id
                          const rotuloNivel =
                            log.tipo === 'erro'
                              ? 'Erro'
                              : log.tipo === 'aviso'
                                ? 'Aviso'
                                : 'Informação'
                          const rotuloOrigem =
                            log.origem === 'backend'
                              ? 'Backend'
                              : log.origem === 'firmware'
                                ? 'Firmware'
                                : log.origem === 'frontend'
                                  ? 'Frontend'
                                  : log.origem
                          return (
                            <tr
                              key={log.id}
                              className={selecionado ? 'linha-log-selecionada' : ''}
                              onClick={() => setLogSelecionado(log)}
                              data-testid={`linha-log-${log.id}`}
                            >
                              <td>{formatarDataHoraBr(log.ocorrido_em, true)}</td>
                              <td>
                                <span
                                  className={`pilula-resultado ${
                                    log.tipo === 'erro'
                                      ? 'abandonada'
                                      : log.tipo === 'aviso'
                                        ? 'em_andamento'
                                        : 'info'
                                  }`}
                                >
                                  <span className="ponto-inline" />
                                  {rotuloNivel}
                                </span>
                              </td>
                              <td>{rotuloOrigem}</td>
                              <td>
                                <strong>{log.mensagem}</strong>
                              </td>
                              <td>
                                {log.id_corrida ? `#${String(log.id_corrida).padStart(3, '0')}` : '--'}
                              </td>
                              <td>
                                <button
                                  type="button"
                                  className="botao-ver-detalhes-linha"
                                  onClick={(e) => {
                                    e.stopPropagation()
                                    setLogSelecionado(log)
                                  }}
                                  data-testid={`botao-detalhe-log-${log.id}`}
                                >
                                  Ver evento ›
                                </button>
                              </td>
                            </tr>
                          )
                        })}
                      </tbody>
                    </table>
                  </div>
                )}

                {!carregandoLogs && logsFiltrados.length > 0 && (
                  <div className="rodape-tabela-historico" data-testid="rodape-tabela-logs">
                    <span className="texto-secundario">
                      Exibindo os últimos {logsFiltrados.length} registros (limite de 100 por consulta)
                    </span>
                    <span className="texto-secundario">Consulta sob demanda ao banco de dados</span>
                  </div>
                )}
              </section>

              {logSelecionado && (
                <aside
                  className="card-painel painel-detalhes-evento"
                  aria-label="Detalhes do evento"
                  data-testid="painel-detalhes-evento"
                >
                  <div className="cabecalho-filtros-historico">
                    <div>
                      <h3 className="card-titulo">Detalhes do evento</h3>
                      <p className="card-subtitulo">
                        {formatarDataHoraBr(logSelecionado.ocorrido_em, true)}
                      </p>
                    </div>
                    <button
                      type="button"
                      className="botao-acao-secundario"
                      onClick={() => setLogSelecionado(null)}
                      aria-label="Fechar detalhes do evento"
                    >
                      ✕
                    </button>
                  </div>

                  <div className="linhas-duplas-indicador">
                    <div className="linha-indicador">
                      <span>Nível</span>
                      <strong
                        className={
                          logSelecionado.tipo === 'erro'
                            ? 'texto-vermelho'
                            : logSelecionado.tipo === 'aviso'
                              ? 'texto-ambar'
                              : 'texto-azul'
                        }
                      >
                        {logSelecionado.tipo === 'erro'
                          ? 'Erro'
                          : logSelecionado.tipo === 'aviso'
                            ? 'Aviso'
                            : 'Informação'}
                      </strong>
                    </div>
                    <div className="linha-indicador">
                      <span>Origem</span>
                      <strong>
                        {logSelecionado.origem === 'backend'
                          ? 'Backend'
                          : logSelecionado.origem === 'firmware'
                            ? 'Firmware'
                            : logSelecionado.origem === 'frontend'
                              ? 'Frontend'
                              : logSelecionado.origem}
                      </strong>
                    </div>
                    <div className="linha-indicador">
                      <span>Corrida</span>
                      <strong>
                        {logSelecionado.id_corrida
                          ? `#${String(logSelecionado.id_corrida).padStart(3, '0')}`
                          : '--'}
                      </strong>
                    </div>
                    <div className="linha-indicador">
                      <span>Status</span>
                      <strong
                        data-testid="detalhe-evento-status"
                        className={
                          resolverStatusLog(logSelecionado) === 'Descartado'
                            ? 'texto-vermelho'
                            : 'texto-verde'
                        }
                      >
                        {resolverStatusLog(logSelecionado)}
                      </strong>
                    </div>
                  </div>

                  <div className="bloco-motivo-evento" data-testid="detalhe-evento-motivo">
                    <span className="rotulo-caixa-alta">Motivo</span>
                    <p>{logSelecionado.mensagem}</p>
                  </div>
                </aside>
              )}
            </div>
          </div>
        )}
      </main>
    </div>
  )
}
