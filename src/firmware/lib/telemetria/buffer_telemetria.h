#pragma once

#include "pacote.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Parâmetros de retransmissão conforme a seção 6 do Contrato de Telemetria
const uint32_t TIMEOUT_ACK_PADRAO_MS = 2000; // 2 segundos sem ack -> reenvia
const uint8_t MAX_TENTATIVAS_PADRAO = 3;     // 3 envios no total: inicial + 2 reenvios; descarta após o último timeout

// RF25-S / CT-SE17: uma corrida completa sem nenhuma conexão no maior labirinto, que dura
// até 10 min. A 5 Hz são 3000 pacotes de telemetria; os 200 restantes são margem para os
// eventos (inicio, fim, bateria_critica, reconexao) e pendências de uma corrida anterior.
// Com ItemBuffer de 32 B o buffer ocupa ~100 KB (~31% da RAM do ESP32).
const size_t CAPACIDADE_BUFFER_PADRAO = 3200;

// Dados específicos de cada tipo de mensagem. Os valores decimais são guardados como
// inteiros na mesma resolução do contrato (velocidade 3 casas, tensão 2, tempo 2, consumo 1),
// então a mensagem reconstruída é serializada exatamente como a original.
struct DadosTelemetriaCompactos {
  int8_t pos_x;
  int8_t pos_y;
  char orientacao;
  uint8_t paredes;
  uint16_t velocidade_mm_s;
  uint16_t tensao_cv;
  int16_t valor_flood; // -1 = null
  uint8_t estado;      // índice do estado na tabela do contrato
};

struct DadosTensaoCompactos {  // evento 'inicio' e 'bateria_critica'
  uint16_t tensao_cv;
};

struct DadosReconexaoCompactos {
  uint32_t offline_ms;
  uint16_t pendentes;
};

struct DadosFimCompactos {
  uint32_t tempo_conc_cs;
  uint16_t consumo_dma;  // décimos de mA
  uint8_t resultado;
  uint8_t motivo;
};

// Registro de uma mensagem retida no buffer (32 B, sem ponteiros nem alocação dinâmica).
struct ItemBuffer {
  enum : uint8_t { FLAG_ENVIADO = 0x01, FLAG_ENCERRADO = 0x02, FLAG_RETOMADA = 0x04 };

  uint32_t corrida;
  uint32_t seq;
  uint32_t tempo_ms;
  uint32_t ultimo_envio_ms;
  union {
    DadosTelemetriaCompactos tel;
    DadosTensaoCompactos tensao;
    DadosReconexaoCompactos reconexao;
    DadosFimCompactos fim;
  } dados;
  uint8_t versao;
  uint8_t tipo;       // TipoMensagem
  uint8_t tentativas;
  uint8_t flags;

  bool enviado() const { return (flags & FLAG_ENVIADO) != 0; }
  // Encerrado = confirmado por ACK ou descartado: o slot pode ser reaproveitado
  bool encerrado() const { return (flags & FLAG_ENCERRADO) != 0; }
  void marcarEnviado() { flags = (uint8_t)(flags | FLAG_ENVIADO); }
  void encerrar() { flags = (uint8_t)(flags | FLAG_ENCERRADO); }
};

static_assert(sizeof(ItemBuffer) == 32, "ItemBuffer deve ocupar 32 B para o buffer caber na RAM");

// Gerenciador de armazenamento local e fila de retransmissão em memória estática.
// Ocupa ~100 KB: instanciar como variável global/estática, nunca na pilha das tasks.
class BufferTelemetria {
public:
  BufferTelemetria();

  // Enfileira uma nova mensagem para envio.
  // Se o buffer estiver cheio, primeiro reaproveita os slots já encerrados (ACK fora de
  // ordem); depois descarta a telemetria mais antiga, preservando eventos.
  // Se só houver eventos, recusa a inserção e contabiliza o descarte por overflow.
  // Retorna false se não houver espaço seguro ou se tipo/estado forem inválidos.
  bool enfileirar(const MensagemTelemetria &msg);

  // Confirma o recebimento de ACK do servidor para a mensagem (corrida, seq).
  // Retorna true se a mensagem correspondente foi localizada e confirmada.
  bool confirmarAck(uint32_t corrida, uint32_t seq);

  // Seleciona a próxima mensagem que deve ser transmitida:
  // Offline: não envia nem expira mensagens. Na retomada, envia primeiro reconexao
  // e os pendentes da queda em ordem, antes de voltar à prioridade normal.
  // 0. Descarta as mensagens que esgotaram as tentativas sem ACK.
  // 1. Prioriza mensagens novas ainda não enviadas (enviado == false), da mais antiga
  //    para a mais nova.
  // 2. Se não houver novas, busca mensagens cujo tempo desde o último envio >= timeout_ack_ms.
  // Retorna true se encontrou uma mensagem para envio/reenvio.
  bool obterProximoEnvio(uint32_t agora_ms, MensagemTelemetria &msg, bool &ehReenvio);

  // Registra que a mensagem (corrida, seq) foi enviada no instante agora_ms,
  // incrementando o número de tentativas e atualizando o tempo do último envio.
  // Chamar somente após envio bem-sucedido; chamadas offline são ignoradas.
  // O seq reinicia a cada corrida, então a corrida faz parte da identificação.
  void registrarEnvio(uint32_t corrida, uint32_t seq, uint32_t agora_ms);

  // Notifica o buffer sobre a perda de conexão com a rede Wi-Fi / servidor
  void registrarQuedaConexao(uint32_t agora_ms);

  // Gera o evento 'reconexao' conforme o contrato ao restabelecer a conexão e o coloca
  // na frente da fila, para ser enviado antes dos pendentes (e reenviado se ficar sem ACK).
  //  - agora_ms: relógio do sistema, o mesmo usado em obterProximoEnvio (mede offline_ms).
  //  - tempo_ms: tempo desde a largada, que vai no campo 'tempo_ms' da mensagem.
  // Retorna false se nenhuma queda foi registrada ou não houver espaço para o evento.
  // Neste último caso permanece offline; o chamador pode liberar espaço por ACK e tentar novamente.
  bool gerarEventoReconexao(uint32_t agora_ms, uint32_t tempo_ms, uint32_t corrida,
                            uint32_t seq, MensagemTelemetria &eventoReconexao);

  // Limpa todo o buffer e zera os contadores
  void limpar();

  // Consultas de estado
  size_t pendentes() const { return pendentes_; }
  size_t capacidade() const { return CAPACIDADE_BUFFER_PADRAO; }
  bool vazio() const { return pendentes_ == 0; }
  bool cheio() const { return pendentes_ >= CAPACIDADE_BUFFER_PADRAO; }

  // Métricas de auditoria e diagnóstico (CT-SE16, CT-SE17)
  uint32_t totalEnfileirados() const { return total_enfileirados_; }
  uint32_t totalConfirmados() const { return total_confirmados_; }
  uint32_t totalDescartesOverflow() const { return descartes_overflow_; }
  uint32_t totalDescartesTentativas() const { return descartes_tentativas_; }
  uint32_t ocupacaoMaxima() const { return ocupacao_maxima_; }

  // Configuração de tempos
  void setTimeoutAckMs(uint32_t timeout) { timeout_ack_ms_ = timeout; }
  void setMaxTentativas(uint8_t max_tentativas) { max_tentativas_ = max_tentativas; }

private:
  size_t indice(size_t posicao) const { return (inicio_ + posicao) % CAPACIDADE_BUFFER_PADRAO; }
  uint32_t decorridoMs(uint32_t agora_ms, uint32_t desde_ms) const;

  void avancarCabeca();      // remove os itens encerrados do início do anel
  void compactar();          // remove os itens encerrados do meio, preservando a ordem
  bool liberarEspaco();      // reaproveita slots ou descarta a telemetria mais antiga
  bool inserirNaCauda(const ItemBuffer &item);
  bool inserirNaFrente(const ItemBuffer &item);
  void registrarInsercao();

  ItemBuffer itens_[CAPACIDADE_BUFFER_PADRAO];
  size_t inicio_ = 0;
  size_t quantidade_ = 0; // slots ocupados no anel (inclui encerrados ainda não removidos)
  size_t pendentes_ = 0;  // itens ainda sem ACK e sem descarte

  uint32_t timeout_ack_ms_ = TIMEOUT_ACK_PADRAO_MS;
  uint8_t max_tentativas_ = MAX_TENTATIVAS_PADRAO;

  uint32_t instante_queda_ms_ = 0;
  bool offline_ = false;

  uint32_t total_enfileirados_ = 0;
  uint32_t total_confirmados_ = 0;
  uint32_t descartes_overflow_ = 0;
  uint32_t descartes_tentativas_ = 0;
  uint32_t ocupacao_maxima_ = 0;
};
