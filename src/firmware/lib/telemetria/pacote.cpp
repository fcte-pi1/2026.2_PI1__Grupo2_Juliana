#include "pacote.h"
#include "crc16.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char* motivoParaTexto(MotivoAbandono motivo) {
  switch (motivo) {
    case MOTIVO_BATERIA_CRITICA: return "bateria_critica";
    case MOTIVO_FALHA_SENSOR_BATERIA: return "falha_sensor_bateria";
    case MOTIVO_COLISAO: return "colisao";
    case MOTIVO_TEMPO_ESGOTADO: return "tempo_esgotado";
    case MOTIVO_MANUAL: return "manual";
    default: return "manual";
  }
}

MensagemTelemetria criarTelemetria(uint32_t corrida, uint32_t seq, uint32_t tempo_ms,
                                  int8_t pos_x, int8_t pos_y, char orientacao,
                                  uint8_t paredes, float velocidade, float tensao_bateria,
                                  int16_t valor_flood, const char* estado) {
  MensagemTelemetria msg;
  msg.versao = 1;
  msg.tipo = TIPO_TEL;
  msg.corrida = corrida;
  msg.seq = seq;
  msg.tempo_ms = tempo_ms;
  msg.pos_x = pos_x;
  msg.pos_y = pos_y;
  msg.orientacao = orientacao;
  msg.paredes = paredes;
  msg.velocidade = velocidade;
  msg.tensao_bateria = tensao_bateria;
  msg.valor_flood = valor_flood;
  if (estado != nullptr) {
    snprintf(msg.estado, sizeof(msg.estado), "%s", estado);
  }
  return msg;
}

MensagemTelemetria criarEventoInicio(uint32_t corrida, uint32_t seq, uint32_t tempo_ms,
                                    float tensao_bateria) {
  MensagemTelemetria msg;
  msg.versao = 1;
  msg.tipo = EVT_INICIO;
  msg.corrida = corrida;
  msg.seq = seq;
  msg.tempo_ms = tempo_ms;
  msg.tensao_bateria = tensao_bateria;
  return msg;
}

MensagemTelemetria criarEventoBateriaCritica(uint32_t corrida, uint32_t seq, uint32_t tempo_ms,
                                           float tensao_bateria) {
  MensagemTelemetria msg;
  msg.versao = 1;
  msg.tipo = EVT_BATERIA_CRITICA;
  msg.corrida = corrida;
  msg.seq = seq;
  msg.tempo_ms = tempo_ms;
  msg.tensao_bateria = tensao_bateria;
  return msg;
}

MensagemTelemetria criarEventoReconexao(uint32_t corrida, uint32_t seq, uint32_t tempo_ms,
                                       uint32_t offline_ms, uint16_t pendentes) {
  MensagemTelemetria msg;
  msg.versao = 1;
  msg.tipo = EVT_RECONEXAO;
  msg.corrida = corrida;
  msg.seq = seq;
  msg.tempo_ms = tempo_ms;
  msg.offline_ms = offline_ms;
  msg.pendentes = pendentes;
  return msg;
}

MensagemTelemetria criarEventoFimConcluida(uint32_t corrida, uint32_t seq, uint32_t tempo_ms,
                                          float tempo_conc_s, float consumo_medio_ma) {
  MensagemTelemetria msg;
  msg.versao = 1;
  msg.tipo = EVT_FIM;
  msg.corrida = corrida;
  msg.seq = seq;
  msg.tempo_ms = tempo_ms;
  msg.resultado = RESULTADO_CONCLUIDA;
  msg.tempo_conc_s = tempo_conc_s;
  msg.consumo_medio_ma = consumo_medio_ma;
  return msg;
}

MensagemTelemetria criarEventoFimAbandonada(uint32_t corrida, uint32_t seq, uint32_t tempo_ms,
                                           MotivoAbandono motivo, float consumo_medio_ma) {
  MensagemTelemetria msg;
  msg.versao = 1;
  msg.tipo = EVT_FIM;
  msg.corrida = corrida;
  msg.seq = seq;
  msg.tempo_ms = tempo_ms;
  msg.resultado = RESULTADO_ABANDONADA;
  msg.motivo = motivo;
  msg.consumo_medio_ma = consumo_medio_ma;
  return msg;
}

size_t serializarJson(const MensagemTelemetria &msg, char *buffer, size_t maxLen) {
  if (buffer == nullptr || maxLen == 0) {
    return 0;
  }

  // Cabeçalho comum a 'tel' e 'evt' (seção 2.1 do contrato)
  int n = snprintf(buffer, maxLen,
                   "{\"v\":%u,\"t\":\"%s\",\"corrida\":%" PRIu32 ",\"seq\":%" PRIu32
                   ",\"tempo_ms\":%" PRIu32,
                   (unsigned)msg.versao, (msg.tipo == TIPO_TEL) ? "tel" : "evt",
                   msg.corrida, msg.seq, msg.tempo_ms);
  if (n < 0 || (size_t)n >= maxLen) {
    return 0; // Truncado ou erro
  }
  const size_t usado = (size_t)n;
  char *resto = buffer + usado;
  const size_t espaco = maxLen - usado;

  int m = 0;
  switch (msg.tipo) {
    case TIPO_TEL: {
      char flood[8];
      if (msg.valor_flood >= 0) {
        snprintf(flood, sizeof(flood), "%d", msg.valor_flood);
      } else {
        snprintf(flood, sizeof(flood), "null");
      }
      m = snprintf(resto, espaco,
                   ",\"pos_x\":%d,\"pos_y\":%d,\"orientacao\":\"%c\",\"paredes\":%u,"
                   "\"velocidade\":%.3f,\"tensao_bateria\":%.2f,\"valor_flood\":%s,\"estado\":\"%s\"}",
                   msg.pos_x, msg.pos_y, msg.orientacao, (unsigned)msg.paredes,
                   msg.velocidade, msg.tensao_bateria, flood, msg.estado);
      break;
    }
    case EVT_INICIO:
      m = snprintf(resto, espaco, ",\"evento\":\"inicio\",\"tensao_bateria\":%.2f}",
                   msg.tensao_bateria);
      break;
    case EVT_BATERIA_CRITICA:
      m = snprintf(resto, espaco, ",\"evento\":\"bateria_critica\",\"tensao_bateria\":%.2f}",
                   msg.tensao_bateria);
      break;
    case EVT_RECONEXAO:
      m = snprintf(resto, espaco,
                   ",\"evento\":\"reconexao\",\"offline_ms\":%" PRIu32 ",\"pendentes\":%u}",
                   msg.offline_ms, (unsigned)msg.pendentes);
      break;
    case EVT_FIM:
      if (msg.resultado == RESULTADO_CONCLUIDA) {
        m = snprintf(resto, espaco,
                     ",\"evento\":\"fim\",\"resultado\":\"concluida\",\"tempo_conc_s\":%.2f,"
                     "\"consumo_medio_ma\":%.1f}",
                     msg.tempo_conc_s, msg.consumo_medio_ma);
      } else {
        m = snprintf(resto, espaco,
                     ",\"evento\":\"fim\",\"resultado\":\"abandonada\",\"motivo\":\"%s\","
                     "\"consumo_medio_ma\":%.1f}",
                     motivoParaTexto(msg.motivo), msg.consumo_medio_ma);
      }
      break;
    default:
      return 0; // Tipo desconhecido
  }

  if (m < 0 || (size_t)m >= espaco) {
    return 0; // Truncado ou erro
  }
  return usado + (size_t)m;
}

size_t serializarQuadro(const MensagemTelemetria &msg, char *buffer, size_t maxLen) {
  if (buffer == nullptr || maxLen < 6) {
    return 0;
  }

  // Deixa espaço para "|XXXX\0" (6 bytes)
  size_t tamJson = serializarJson(msg, buffer, maxLen - 5);
  if (tamJson == 0) {
    return 0;
  }

  uint16_t crc = crc16_ccitt((const uint8_t*)buffer, tamJson);
  int n = snprintf(buffer + tamJson, maxLen - tamJson, "|%04X", crc);
  if (n != 5) {
    return 0;
  }

  return tamJson + 5;
}

bool processarAck(const char *frame, uint32_t &corrida, uint32_t &seq) {
  if (frame == nullptr) {
    return false;
  }

  size_t len = strlen(frame);
  if (len < 6) { // Menor frame possível: {}|XXXX
    return false;
  }

  // Localiza o delimitador '|'
  const char *delimitador = strrchr(frame, '|');
  if (delimitador == nullptr) {
    return false;
  }

  size_t tamJson = delimitador - frame;
  const char *crcStr = delimitador + 1;
  if (strlen(crcStr) != 4) {
    return false;
  }

  // Validação do checksum
  char *endptr = nullptr;
  unsigned long crcRecebido = strtoul(crcStr, &endptr, 16);
  if (endptr == nullptr || *endptr != '\0') {
    return false;
  }

  uint16_t crcCalculado = crc16_ccitt((const uint8_t*)frame, tamJson);
  if ((uint16_t)crcRecebido != crcCalculado) {
    return false;
  }

  // Parsing dos campos do ACK: {"t":"ack","corrida":7,"seq":42}
  // Confirma presença de "t":"ack"
  if (strstr(frame, "\"t\":\"ack\"") == nullptr && strstr(frame, "\"t\": \"ack\"") == nullptr) {
    return false;
  }

  // Extração de "corrida":
  const char *posCorrida = strstr(frame, "\"corrida\":");
  if (posCorrida == nullptr) {
    posCorrida = strstr(frame, "\"corrida\": ");
    if (posCorrida == nullptr) return false;
    posCorrida += 11;
  } else {
    posCorrida += 10;
  }
  corrida = (uint32_t)strtoul(posCorrida, nullptr, 10);

  // Extração de "seq":
  const char *posSeq = strstr(frame, "\"seq\":");
  if (posSeq == nullptr) {
    posSeq = strstr(frame, "\"seq\": ");
    if (posSeq == nullptr) return false;
    posSeq += 7;
  } else {
    posSeq += 6;
  }
  seq = (uint32_t)strtoul(posSeq, nullptr, 10);

  return true;
}
