#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Tipos de mensagem conforme o Contrato de Telemetria (docs/4.4.7)
enum TipoMensagem : uint8_t {
  TIPO_TEL,
  EVT_INICIO,
  EVT_FIM,
  EVT_BATERIA_CRITICA,
  EVT_RECONEXAO
};

// Resultado da corrida no evento 'fim'
enum ResultadoCorrida : uint8_t {
  RESULTADO_CONCLUIDA,
  RESULTADO_ABANDONADA
};

// Motivo de abandono no evento 'fim'
enum MotivoAbandono : uint8_t {
  MOTIVO_NENHUM,
  MOTIVO_BATERIA_CRITICA,
  MOTIVO_FALHA_SENSOR_BATERIA,
  MOTIVO_COLISAO,
  MOTIVO_TEMPO_ESGOTADO,
  MOTIVO_MANUAL
};

// Estrutura compacta e estática de uma mensagem de telemetria ou evento
struct MensagemTelemetria {
  // Cabeçalho obrigatório (seção 2.1)
  uint8_t versao = 1;
  TipoMensagem tipo = TIPO_TEL;
  uint32_t corrida = 1;      // Número da corrida no robô (contador local)
  uint32_t seq = 0;          // Sequência da mensagem (inicia em 0)
  uint32_t tempo_ms = 0;     // Milissegundos desde a largada

  // Dados de telemetria periódica ('tel', seção 2.2)
  int8_t pos_x = 0;
  int8_t pos_y = 0;
  char orientacao = 'N';     // 'N', 'L', 'S', 'O'
  uint8_t paredes = 0;       // Máscara 0 a 15 (Norte=1, Leste=2, Sul=4, Oeste=8)
  float velocidade = 0.0f;   // m/s (3 casas decimais)
  float tensao_bateria = 0.0f; // V (2 casas decimais)
  int16_t valor_flood = -1;  // >= 0 ou -1 para valor null
  char estado[16] = "explorando"; // "explorando", "retornando", "rapido", "parado"

  // Dados específicos de eventos ('evt', seção 3)
  ResultadoCorrida resultado = RESULTADO_CONCLUIDA;
  MotivoAbandono motivo = MOTIVO_NENHUM;
  float tempo_conc_s = 0.0f;
  float consumo_medio_ma = 0.0f;
  uint32_t offline_ms = 0;
  uint16_t pendentes = 0;
};

// Construtores auxiliares para montagem de pacotes
MensagemTelemetria criarTelemetria(uint32_t corrida, uint32_t seq, uint32_t tempo_ms,
                                  int8_t pos_x, int8_t pos_y, char orientacao,
                                  uint8_t paredes, float velocidade, float tensao_bateria,
                                  int16_t valor_flood, const char* estado);

MensagemTelemetria criarEventoInicio(uint32_t corrida, uint32_t seq, uint32_t tempo_ms,
                                    float tensao_bateria);

MensagemTelemetria criarEventoBateriaCritica(uint32_t corrida, uint32_t seq, uint32_t tempo_ms,
                                           float tensao_bateria);

MensagemTelemetria criarEventoReconexao(uint32_t corrida, uint32_t seq, uint32_t tempo_ms,
                                       uint32_t offline_ms, uint16_t pendentes);

MensagemTelemetria criarEventoFimConcluida(uint32_t corrida, uint32_t seq, uint32_t tempo_ms,
                                          float tempo_conc_s, float consumo_medio_ma);

MensagemTelemetria criarEventoFimAbandonada(uint32_t corrida, uint32_t seq, uint32_t tempo_ms,
                                           MotivoAbandono motivo, float consumo_medio_ma);

// Serializa a mensagem para JSON puro (sem CRC)
// Retorna a quantidade de bytes escritos no buffer (excluindo '\0') ou 0 em erro
size_t serializarJson(const MensagemTelemetria &msg, char *buffer, size_t maxLen);

// Serializa no formato do contrato: <JSON>|<CRC16> em 4 dígitos hexadecimais maiúsculos
// Retorna a quantidade de bytes escritos no buffer (excluindo '\0') ou 0 em erro
size_t serializarQuadro(const MensagemTelemetria &msg, char *buffer, size_t maxLen);

// Faz o parsing e validação de um frame de confirmação ACK do servidor:
// Formato esperado: {"t":"ack","corrida":7,"seq":42}|7278
// Aceita os três campos do ACK em qualquer ordem, com espaços JSON opcionais.
// Recusa chaves duplicadas, campos extras e inteiros fora de uint32_t (corrida >= 1).
// Retorna true se o frame for válido; em falha, preserva corrida e seq.
bool processarAck(const char *frame, uint32_t &corrida, uint32_t &seq);
