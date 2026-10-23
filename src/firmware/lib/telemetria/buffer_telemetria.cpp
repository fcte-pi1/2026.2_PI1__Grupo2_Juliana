#include "buffer_telemetria.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

namespace {

// Estados válidos de 'tel' (seção 2.2 do contrato); o índice é o que fica guardado no item
const char *const NOMES_ESTADO[] = {"explorando", "retornando", "rapido", "parado"};
const uint8_t TOTAL_ESTADOS = sizeof(NOMES_ESTADO) / sizeof(NOMES_ESTADO[0]);

bool codificarEstado(const char *texto, uint8_t &codigo) {
  for (uint8_t i = 0; i < TOTAL_ESTADOS; i++) {
    if (strcmp(texto, NOMES_ESTADO[i]) == 0) {
      codigo = i;
      return true;
    }
  }
  return false;
}

// Usa a mesma conversão decimal de serializarJson, inclusive nos empates.
// Extrai os dígitos sem uma segunda operação de arredondamento em float.
uint32_t escalarDecimal(float valor, unsigned casas, uint32_t limite) {
  if (!(valor > 0.0f)) return 0;
  if (!isfinite(valor)) return limite;
  char decimal[64];
  const int n = snprintf(decimal, sizeof(decimal), "%.*f", (int)casas, (double)valor);
  if (n < 0 || (size_t)n >= sizeof(decimal)) return limite;
  uint32_t resultado = 0;
  for (int i = 0; i < n; i++) {
    if (decimal[i] < '0' || decimal[i] > '9') continue;
    const uint32_t digito = (uint32_t)(decimal[i] - '0');
    if (resultado > (limite - digito) / 10) return limite;
    resultado = resultado * 10 + digito;
  }
  return resultado;
}

// Retorna false se o tipo ou o estado estiverem fora do contrato
bool paraItem(const MensagemTelemetria &msg, ItemBuffer &item) {
  memset(&item, 0, sizeof(item));
  item.corrida = msg.corrida;
  item.seq = msg.seq;
  item.tempo_ms = msg.tempo_ms;
  item.versao = msg.versao;
  item.tipo = (uint8_t)msg.tipo;

  switch (msg.tipo) {
    case TIPO_TEL: {
      uint8_t estado = 0;
      if (!codificarEstado(msg.estado, estado)) {
        return false;
      }
      DadosTelemetriaCompactos &d = item.dados.tel;
      d.pos_x = msg.pos_x;
      d.pos_y = msg.pos_y;
      d.orientacao = msg.orientacao;
      d.paredes = msg.paredes;
      d.velocidade_mm_s = (uint16_t)escalarDecimal(msg.velocidade, 3, UINT16_MAX);
      d.tensao_cv = (uint16_t)escalarDecimal(msg.tensao_bateria, 2, UINT16_MAX);
      d.valor_flood = msg.valor_flood;
      d.estado = estado;
      return true;
    }
    case EVT_INICIO:
    case EVT_BATERIA_CRITICA:
      item.dados.tensao.tensao_cv = (uint16_t)escalarDecimal(msg.tensao_bateria, 2, UINT16_MAX);
      return true;
    case EVT_RECONEXAO:
      item.dados.reconexao.offline_ms = msg.offline_ms;
      item.dados.reconexao.pendentes = msg.pendentes;
      return true;
    case EVT_FIM:
      item.dados.fim.tempo_conc_cs = escalarDecimal(msg.tempo_conc_s, 2, UINT32_MAX);
      item.dados.fim.consumo_dma = (uint16_t)escalarDecimal(msg.consumo_medio_ma, 1, UINT16_MAX);
      item.dados.fim.resultado = (uint8_t)msg.resultado;
      item.dados.fim.motivo = (uint8_t)msg.motivo;
      return true;
    default:
      return false;
  }
}

MensagemTelemetria paraMensagem(const ItemBuffer &item) {
  MensagemTelemetria msg;
  msg.versao = item.versao;
  msg.tipo = (TipoMensagem)item.tipo;
  msg.corrida = item.corrida;
  msg.seq = item.seq;
  msg.tempo_ms = item.tempo_ms;

  switch (msg.tipo) {
    case TIPO_TEL: {
      const DadosTelemetriaCompactos &d = item.dados.tel;
      msg.pos_x = d.pos_x;
      msg.pos_y = d.pos_y;
      msg.orientacao = d.orientacao;
      msg.paredes = d.paredes;
      msg.velocidade = (float)d.velocidade_mm_s / 1000.0f;
      msg.tensao_bateria = (float)d.tensao_cv / 100.0f;
      msg.valor_flood = d.valor_flood;
      strncpy(msg.estado, NOMES_ESTADO[d.estado < TOTAL_ESTADOS ? d.estado : 0],
              sizeof(msg.estado) - 1);
      msg.estado[sizeof(msg.estado) - 1] = '\0';
      break;
    }
    case EVT_INICIO:
    case EVT_BATERIA_CRITICA:
      msg.tensao_bateria = (float)item.dados.tensao.tensao_cv / 100.0f;
      break;
    case EVT_RECONEXAO:
      msg.offline_ms = item.dados.reconexao.offline_ms;
      msg.pendentes = item.dados.reconexao.pendentes;
      break;
    case EVT_FIM:
      msg.tempo_conc_s = (float)item.dados.fim.tempo_conc_cs / 100.0f;
      msg.consumo_medio_ma = (float)item.dados.fim.consumo_dma / 10.0f;
      msg.resultado = (ResultadoCorrida)item.dados.fim.resultado;
      msg.motivo = (MotivoAbandono)item.dados.fim.motivo;
      break;
  }
  return msg;
}

}  // namespace

BufferTelemetria::BufferTelemetria() {
  limpar();
}

void BufferTelemetria::limpar() {
  inicio_ = 0;
  quantidade_ = 0;
  pendentes_ = 0;
  instante_queda_ms_ = 0;
  offline_ = false;
  total_enfileirados_ = 0;
  total_confirmados_ = 0;
  descartes_overflow_ = 0;
  descartes_tentativas_ = 0;
  ocupacao_maxima_ = 0;

  memset(itens_, 0, sizeof(itens_));
}

uint32_t BufferTelemetria::decorridoMs(uint32_t agora_ms, uint32_t desde_ms) const {
  // Subtração modular: funciona também quando millis() volta a zero.
  return agora_ms - desde_ms;
}

void BufferTelemetria::avancarCabeca() {
  while (quantidade_ > 0 && itens_[inicio_].encerrado()) {
    inicio_ = (inicio_ + 1) % CAPACIDADE_BUFFER_PADRAO;
    quantidade_--;
  }
}

void BufferTelemetria::compactar() {
  // Leitura e escrita avançam no mesmo anel, com escrita sempre <= leitura: cópia in-place segura.
  size_t escrita = 0;
  for (size_t leitura = 0; leitura < quantidade_; leitura++) {
    const ItemBuffer &item = itens_[indice(leitura)];
    if (item.encerrado()) {
      continue;
    }
    if (escrita != leitura) {
      itens_[indice(escrita)] = item;
    }
    escrita++;
  }
  quantidade_ = escrita;
}

bool BufferTelemetria::liberarEspaco() {
  if (quantidade_ < CAPACIDADE_BUFFER_PADRAO) return true;
  compactar();
  if (quantidade_ < CAPACIDADE_BUFFER_PADRAO) return true;

  // Preserva eventos: perder inicio impede a associação da corrida no backend.
  for (size_t i = 0; i < quantidade_; i++) {
    ItemBuffer &item = itens_[indice(i)];
    if (item.tipo == TIPO_TEL) {
      item.encerrar();
      pendentes_--;
      descartes_overflow_++;
      compactar();
      return true;
    }
  }
  // Memória finita: sem telemetria descartável, recusa o novo item.
  descartes_overflow_++;
  return false;
}

void BufferTelemetria::registrarInsercao() {
  quantidade_++;
  pendentes_++;
  total_enfileirados_++;
  if (pendentes_ > ocupacao_maxima_) {
    ocupacao_maxima_ = (uint32_t)pendentes_;
  }
}

bool BufferTelemetria::inserirNaCauda(const ItemBuffer &item) {
  if (!liberarEspaco()) return false;
  itens_[indice(quantidade_)] = item;
  registrarInsercao();
  return true;
}

bool BufferTelemetria::inserirNaFrente(const ItemBuffer &item) {
  if (!liberarEspaco()) return false;
  inicio_ = (inicio_ + CAPACIDADE_BUFFER_PADRAO - 1) % CAPACIDADE_BUFFER_PADRAO;
  itens_[inicio_] = item;
  registrarInsercao();
  return true;
}

bool BufferTelemetria::enfileirar(const MensagemTelemetria &msg) {
  ItemBuffer item;
  if (!paraItem(msg, item)) {
    return false;
  }
  return inserirNaCauda(item);
}

bool BufferTelemetria::confirmarAck(uint32_t corrida, uint32_t seq) {
  for (size_t i = 0; i < quantidade_; i++) {
    ItemBuffer &item = itens_[indice(i)];
    if (!item.encerrado() && item.corrida == corrida && item.seq == seq) {
      item.encerrar();
      pendentes_--;
      total_confirmados_++;
      avancarCabeca();
      return true;
    }
  }
  return false;
}

bool BufferTelemetria::obterProximoEnvio(uint32_t agora_ms, MensagemTelemetria &msg, bool &ehReenvio) {
  if (offline_) return false;

  // 0. Descarta quem esgotou as tentativas e deixou vencer o timeout do último envio.
  //    Só marca aqui e avança a cabeça uma vez ao final, para não deslocar os índices do loop.
  for (size_t i = 0; i < quantidade_; i++) {
    ItemBuffer &item = itens_[indice(i)];
    if (!item.encerrado() && item.enviado() && item.tentativas >= max_tentativas_ &&
        decorridoMs(agora_ms, item.ultimo_envio_ms) >= timeout_ack_ms_) {
      item.encerrar();
      pendentes_--;
      descartes_tentativas_++;
    }
  }
  avancarCabeca();

  // Uma passagem FIFO pelos retidos na queda, incluindo os já enviados.
  // A marca é retirada no envio; ACKs e descartes também encerram a pendência.
  for (size_t i = 0; i < quantidade_; i++) {
    const ItemBuffer &item = itens_[indice(i)];
    if (!item.encerrado() && (item.flags & ItemBuffer::FLAG_RETOMADA)) {
      if (item.enviado() && item.tentativas >= max_tentativas_) continue;
      msg = paraMensagem(item);
      ehReenvio = item.enviado();
      return true;
    }
  }

  // 1. Prioridade: mensagens novas ainda não enviadas, da mais antiga para a mais nova
  for (size_t i = 0; i < quantidade_; i++) {
    const ItemBuffer &item = itens_[indice(i)];
    if (!item.encerrado() && !item.enviado()) {
      msg = paraMensagem(item);
      ehReenvio = false;
      return true;
    }
  }

  // 2. Mensagens já enviadas que atingiram o timeout de ACK (2 s por padrão)
  for (size_t i = 0; i < quantidade_; i++) {
    const ItemBuffer &item = itens_[indice(i)];
    if (!item.encerrado() && item.enviado() &&
        decorridoMs(agora_ms, item.ultimo_envio_ms) >= timeout_ack_ms_) {
      msg = paraMensagem(item);
      ehReenvio = true;
      return true;
    }
  }

  return false;
}

void BufferTelemetria::registrarEnvio(uint32_t corrida, uint32_t seq, uint32_t agora_ms) {
  if (offline_) return;
  for (size_t i = 0; i < quantidade_; i++) {
    ItemBuffer &item = itens_[indice(i)];
    if (!item.encerrado() && item.corrida == corrida && item.seq == seq) {
      item.flags &= (uint8_t)~ItemBuffer::FLAG_RETOMADA;
      item.marcarEnviado();
      item.ultimo_envio_ms = agora_ms;
      item.tentativas++;
      return;
    }
  }
}

void BufferTelemetria::registrarQuedaConexao(uint32_t agora_ms) {
  if (!offline_) {
    instante_queda_ms_ = agora_ms;
    offline_ = true;
  }
}

bool BufferTelemetria::gerarEventoReconexao(uint32_t agora_ms, uint32_t tempo_ms, uint32_t corrida,
                                            uint32_t seq, MensagemTelemetria &eventoReconexao) {
  if (!offline_) {
    return false;
  }

  // 'pendentes' conta o que ficou retido durante a queda, sem o próprio evento
  eventoReconexao = criarEventoReconexao(corrida, seq, tempo_ms,
                                         decorridoMs(agora_ms, instante_queda_ms_),
                                         (uint16_t)pendentes_);
  ItemBuffer item;
  paraItem(eventoReconexao, item);
  if (!inserirNaFrente(item)) return false;
  const uint32_t duracao = decorridoMs(agora_ms, instante_queda_ms_);
  for (size_t i = 0; i < quantidade_; i++) {
    ItemBuffer &retido = itens_[indice(i)];
    if (retido.encerrado()) continue;
    if (!retido.enviado() || retido.tentativas < max_tentativas_) {
      retido.flags |= ItemBuffer::FLAG_RETOMADA;
    }
    // Pausa a janela de ACK durante a desconexão, sem renovar tentativas.
    if (retido.enviado()) retido.ultimo_envio_ms += duracao;
  }
  offline_ = false;
  return true;
}
