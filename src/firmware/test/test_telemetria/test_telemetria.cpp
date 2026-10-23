#include <unity.h>
#include <string.h>
#include <stdio.h>

#include "pacote.h"
#include "buffer_telemetria.h"
#include "crc16.h"

void setUp() {}
void tearDown() {}

// ==============================================================================
// 1. Testes de Serialização e Formato do Contrato (docs/4.4.7)
// ==============================================================================

void test_serializar_telemetria_exemplo_contrato() {
  MensagemTelemetria msg = criarTelemetria(
      7, 42, 8400, 2, 1, 'L', 9, 0.312f, 7.86f, 3, "explorando");

  char quadro[256];
  size_t len = serializarQuadro(msg, quadro, sizeof(quadro));

  const char *esperado =
      "{\"v\":1,\"t\":\"tel\",\"corrida\":7,\"seq\":42,\"tempo_ms\":8400,"
      "\"pos_x\":2,\"pos_y\":1,\"orientacao\":\"L\",\"paredes\":9,"
      "\"velocidade\":0.312,\"tensao_bateria\":7.86,\"valor_flood\":3,\"estado\":\"explorando\"}|2917";

  TEST_ASSERT_GREATER_THAN(0, len);
  TEST_ASSERT_EQUAL_STRING(esperado, quadro);
}

void test_serializar_telemetria_com_flood_nulo() {
  MensagemTelemetria msg = criarTelemetria(
      7, 43, 8600, 2, 2, 'N', 3, 0.250f, 7.84f, -1, "explorando");

  char quadro[256];
  size_t len = serializarQuadro(msg, quadro, sizeof(quadro));

  TEST_ASSERT_GREATER_THAN(0, len);
  TEST_ASSERT_NOT_NULL(strstr(quadro, "\"valor_flood\":null"));

  // Validação do checksum do frame gerado
  const char *pipe = strrchr(quadro, '|');
  TEST_ASSERT_NOT_NULL(pipe);
  size_t tamJson = pipe - quadro;
  uint16_t crcCalculado = crc16_ccitt((const uint8_t*)quadro, tamJson);
  char crcHexEsperado[8];
  snprintf(crcHexEsperado, sizeof(crcHexEsperado), "|%04X", crcCalculado);
  TEST_ASSERT_EQUAL_STRING(crcHexEsperado, pipe);
}

void test_serializar_evento_inicio() {
  MensagemTelemetria msg = criarEventoInicio(7, 0, 0, 8.21f);

  char quadro[256];
  size_t len = serializarQuadro(msg, quadro, sizeof(quadro));

  const char *esperado =
      "{\"v\":1,\"t\":\"evt\",\"corrida\":7,\"seq\":0,\"tempo_ms\":0,"
      "\"evento\":\"inicio\",\"tensao_bateria\":8.21}|4886";

  TEST_ASSERT_GREATER_THAN(0, len);
  TEST_ASSERT_EQUAL_STRING(esperado, quadro);
}

void test_serializar_evento_bateria_critica() {
  MensagemTelemetria msg = criarEventoBateriaCritica(7, 97, 19350, 6.98f);

  char quadro[256];
  size_t len = serializarQuadro(msg, quadro, sizeof(quadro));

  const char *esperado =
      "{\"v\":1,\"t\":\"evt\",\"corrida\":7,\"seq\":97,\"tempo_ms\":19350,"
      "\"evento\":\"bateria_critica\",\"tensao_bateria\":6.98}|8E50";

  TEST_ASSERT_GREATER_THAN(0, len);
  TEST_ASSERT_EQUAL_STRING(esperado, quadro);
}

void test_serializar_evento_reconexao() {
  MensagemTelemetria msg = criarEventoReconexao(7, 160, 32010, 20150, 101);

  char quadro[256];
  size_t len = serializarQuadro(msg, quadro, sizeof(quadro));

  const char *esperado =
      "{\"v\":1,\"t\":\"evt\",\"corrida\":7,\"seq\":160,\"tempo_ms\":32010,"
      "\"evento\":\"reconexao\",\"offline_ms\":20150,\"pendentes\":101}|1D4F";

  TEST_ASSERT_GREATER_THAN(0, len);
  TEST_ASSERT_EQUAL_STRING(esperado, quadro);
}

void test_serializar_evento_fim_concluida() {
  MensagemTelemetria msg = criarEventoFimConcluida(7, 311, 62240, 62.24f, 412.5f);

  char quadro[256];
  size_t len = serializarQuadro(msg, quadro, sizeof(quadro));

  const char *esperado =
      "{\"v\":1,\"t\":\"evt\",\"corrida\":7,\"seq\":311,\"tempo_ms\":62240,"
      "\"evento\":\"fim\",\"resultado\":\"concluida\",\"tempo_conc_s\":62.24,"
      "\"consumo_medio_ma\":412.5}|E1BA";

  TEST_ASSERT_GREATER_THAN(0, len);
  TEST_ASSERT_EQUAL_STRING(esperado, quadro);
}

void test_serializar_evento_fim_abandonada() {
  MensagemTelemetria msg = criarEventoFimAbandonada(8, 55, 11020, MOTIVO_BATERIA_CRITICA, 398.0f);

  char quadro[256];
  size_t len = serializarQuadro(msg, quadro, sizeof(quadro));

  const char *esperado =
      "{\"v\":1,\"t\":\"evt\",\"corrida\":8,\"seq\":55,\"tempo_ms\":11020,"
      "\"evento\":\"fim\",\"resultado\":\"abandonada\",\"motivo\":\"bateria_critica\","
      "\"consumo_medio_ma\":398.0}|0C0B";

  TEST_ASSERT_GREATER_THAN(0, len);
  TEST_ASSERT_EQUAL_STRING(esperado, quadro);
}

void test_serializar_evento_fim_motivos_diversos() {
  char buf[256];

  MensagemTelemetria m1 = criarEventoFimAbandonada(1, 1, 100, MOTIVO_FALHA_SENSOR_BATERIA, 200.0f);
  serializarJson(m1, buf, sizeof(buf));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"motivo\":\"falha_sensor_bateria\""));

  MensagemTelemetria m2 = criarEventoFimAbandonada(1, 2, 200, MOTIVO_COLISAO, 200.0f);
  serializarJson(m2, buf, sizeof(buf));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"motivo\":\"colisao\""));

  MensagemTelemetria m3 = criarEventoFimAbandonada(1, 3, 300, MOTIVO_TEMPO_ESGOTADO, 200.0f);
  serializarJson(m3, buf, sizeof(buf));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"motivo\":\"tempo_esgotado\""));

  MensagemTelemetria m4 = criarEventoFimAbandonada(1, 4, 400, MOTIVO_MANUAL, 200.0f);
  serializarJson(m4, buf, sizeof(buf));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"motivo\":\"manual\""));
}

void test_serializar_buffer_insuficiente() {
  MensagemTelemetria msg = criarTelemetria(1, 1, 100, 0, 0, 'N', 0, 0.0f, 7.4f, 0, "parado");
  char curto[15];

  // Deve falhar com segurança e retornar 0 sem estouro
  TEST_ASSERT_EQUAL_UINT32(0, serializarJson(msg, curto, sizeof(curto)));
  TEST_ASSERT_EQUAL_UINT32(0, serializarQuadro(msg, curto, sizeof(curto)));
  TEST_ASSERT_EQUAL_UINT32(0, serializarQuadro(msg, nullptr, 100));
}

// ==============================================================================
// 2. Testes de Parsing e Validação de Frame ACK
// ==============================================================================

void test_processar_ack_valido() {
  const char *ackValido = "{\"t\":\"ack\",\"corrida\":7,\"seq\":42}|7278";
  uint32_t corrida = 0;
  uint32_t seq = 0;

  bool ok = processarAck(ackValido, corrida, seq);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_UINT32(7, corrida);
  TEST_ASSERT_EQUAL_UINT32(42, seq);
}

void test_processar_ack_crc_invalido() {
  const char *ackCorrompido = "{\"t\":\"ack\",\"corrida\":7,\"seq\":42}|9999";
  uint32_t corrida = 0;
  uint32_t seq = 0;

  bool ok = processarAck(ackCorrompido, corrida, seq);
  TEST_ASSERT_FALSE(ok);
}

void test_processar_ack_malformado() {
  uint32_t corrida = 0;
  uint32_t seq = 0;

  TEST_ASSERT_FALSE(processarAck(nullptr, corrida, seq));
  TEST_ASSERT_FALSE(processarAck("", corrida, seq));
  TEST_ASSERT_FALSE(processarAck("{}", corrida, seq));
  TEST_ASSERT_FALSE(processarAck("{\"t\":\"ack\",\"corrida\":7,\"seq\":42}", corrida, seq)); // sem CRC
  TEST_ASSERT_FALSE(processarAck("{\"t\":\"ack\",\"corrida\":7,\"seq\":42}|XYZ", corrida, seq)); // CRC curto
  TEST_ASSERT_FALSE(processarAck("{\"t\":\"tel\",\"corrida\":7,\"seq\":42}|7278", corrida, seq)); // tipo não-ack
}

// ==============================================================================
// 3. Testes do Buffer de Telemetria e Fila de Retransmissão
// ==============================================================================

void test_buffer_estado_inicial_e_enfileirar() {
  BufferTelemetria buf;

  TEST_ASSERT_TRUE(buf.vazio());
  TEST_ASSERT_FALSE(buf.cheio());
  TEST_ASSERT_EQUAL_UINT32(0, buf.pendentes());
  // RF25-S / CT-SE17: uma corrida de 10 min a 5 Hz (3000 pacotes) precisa caber inteira
  TEST_ASSERT_GREATER_OR_EQUAL(3000, buf.capacidade());
  TEST_ASSERT_EQUAL_UINT32(CAPACIDADE_BUFFER_PADRAO, buf.capacidade());

  MensagemTelemetria msg = criarTelemetria(1, 0, 100, 0, 0, 'N', 0, 0.0f, 7.5f, 0, "explorando");
  TEST_ASSERT_TRUE(buf.enfileirar(msg));

  TEST_ASSERT_FALSE(buf.vazio());
  TEST_ASSERT_EQUAL_UINT32(1, buf.pendentes());
  TEST_ASSERT_EQUAL_UINT32(1, buf.totalEnfileirados());
}

void test_buffer_obter_proximo_envio_e_confirmar_ack() {
  BufferTelemetria buf;
  MensagemTelemetria m1 = criarTelemetria(1, 1, 100, 0, 0, 'N', 0, 0.0f, 7.5f, 0, "explorando");
  buf.enfileirar(m1);

  MensagemTelemetria msgOut;
  bool ehReenvio = true;

  // 1. Mensagem nova é priorizada e marcada como primeiro envio
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(1000, msgOut, ehReenvio));
  TEST_ASSERT_FALSE(ehReenvio);
  TEST_ASSERT_EQUAL_UINT32(1, msgOut.seq);

  // Registra o envio no instante 1000 ms
  buf.registrarEnvio(1, 1, 1000);

  // Em t=1500 (500 ms depois), ainda está dentro da janela de espera do ACK (2000 ms)
  TEST_ASSERT_FALSE(buf.obterProximoEnvio(1500, msgOut, ehReenvio));

  // Confirma ACK
  TEST_ASSERT_TRUE(buf.confirmarAck(1, 1));
  TEST_ASSERT_EQUAL_UINT32(0, buf.pendentes());
  TEST_ASSERT_EQUAL_UINT32(1, buf.totalConfirmados());

  // Confirmar ACK inexistente deve retornar false
  TEST_ASSERT_FALSE(buf.confirmarAck(1, 999));
}

void test_buffer_retransmissao_por_timeout_2s() {
  BufferTelemetria buf;
  MensagemTelemetria m1 = criarTelemetria(1, 10, 200, 1, 1, 'L', 2, 0.3f, 7.4f, 1, "explorando");
  buf.enfileirar(m1);

  MensagemTelemetria msgOut;
  bool ehReenvio = false;

  // Envio inicial em t=1000
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(1000, msgOut, ehReenvio));
  TEST_ASSERT_FALSE(ehReenvio);
  buf.registrarEnvio(1, 10, 1000);

  // Antes do timeout (1999 ms decorridos) -> não reenvia
  TEST_ASSERT_FALSE(buf.obterProximoEnvio(2999, msgOut, ehReenvio));

  // Exatamente no timeout de 2000 ms (t=3000) -> reenvia!
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(3000, msgOut, ehReenvio));
  TEST_ASSERT_TRUE(ehReenvio);
  TEST_ASSERT_EQUAL_UINT32(10, msgOut.seq);
}

void test_buffer_descarte_apos_3_tentativas() {
  BufferTelemetria buf;
  MensagemTelemetria m1 = criarTelemetria(1, 20, 200, 1, 1, 'L', 2, 0.3f, 7.4f, 1, "explorando");
  buf.enfileirar(m1);

  MensagemTelemetria msgOut;
  bool ehReenvio = false;

  // Tentativa 1 em t=1000
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(1000, msgOut, ehReenvio));
  buf.registrarEnvio(1, 20, 1000);

  // Tentativa 2 em t=3000 (após timeout)
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(3000, msgOut, ehReenvio));
  TEST_ASSERT_TRUE(ehReenvio);
  buf.registrarEnvio(1, 20, 3000);

  // Tentativa 3 em t=5000 (após timeout)
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(5000, msgOut, ehReenvio));
  TEST_ASSERT_TRUE(ehReenvio);
  buf.registrarEnvio(1, 20, 5000);

  // Em t=7000 (timeout novamente, mas já somou 3 tentativas):
  // O buffer deve descartar o pacote e retornar false (nenhum outro pacote na fila)
  TEST_ASSERT_FALSE(buf.obterProximoEnvio(7000, msgOut, ehReenvio));
  TEST_ASSERT_EQUAL_UINT32(0, buf.pendentes());
  TEST_ASSERT_EQUAL_UINT32(1, buf.totalDescartesTentativas());
}

void test_buffer_prioridade_novos_sobre_reenvios() {
  BufferTelemetria buf;
  MensagemTelemetria m1 = criarTelemetria(1, 1, 100, 0, 0, 'N', 0, 0.0f, 7.5f, 0, "explorando");
  buf.enfileirar(m1);

  MensagemTelemetria msgOut;
  bool ehReenvio = false;

  // Envia m1 em t=1000
  buf.obterProximoEnvio(1000, msgOut, ehReenvio);
  buf.registrarEnvio(1, 1, 1000);

  // Em t=3500, m1 está pronta para reenvio (timeout venceu).
  // Porém, geramos uma nova telemetria m2 em t=3500
  MensagemTelemetria m2 = criarTelemetria(1, 2, 3500, 0, 1, 'N', 0, 0.2f, 7.5f, 1, "explorando");
  buf.enfileirar(m2);

  // O buffer DEVE priorizar m2 (novo envio) sobre m1 (reenvio)
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(3500, msgOut, ehReenvio));
  TEST_ASSERT_FALSE(ehReenvio);
  TEST_ASSERT_EQUAL_UINT32(2, msgOut.seq);

  // Registra envio de m2
  buf.registrarEnvio(1, 2, 3500);

  // Próxima busca em t=3500 agora seleciona m1 para reenvio
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(3500, msgOut, ehReenvio));
  TEST_ASSERT_TRUE(ehReenvio);
  TEST_ASSERT_EQUAL_UINT32(1, msgOut.seq);
}

void test_buffer_overflow_ring_buffer_na_capacidade_maxima() {
  const uint32_t cap = (uint32_t)CAPACIDADE_BUFFER_PADRAO;
  BufferTelemetria buf;

  // Preenche até o limite da capacidade
  for (uint32_t i = 0; i < cap; i++) {
    MensagemTelemetria m = criarTelemetria(1, i, i * 200, 0, 0, 'N', 0, 0.0f, 7.4f, 0, "explorando");
    TEST_ASSERT_TRUE(buf.enfileirar(m));
  }

  TEST_ASSERT_TRUE(buf.cheio());
  TEST_ASSERT_EQUAL_UINT32(cap, buf.pendentes());
  TEST_ASSERT_EQUAL_UINT32(0, buf.totalDescartesOverflow());

  // Enfileira mais um item -> deve descartar o mais antigo (seq 0) e aceitar o novo
  MensagemTelemetria mNovo = criarTelemetria(1, cap, cap * 200, 1, 1, 'L', 0, 0.3f, 7.4f, 1, "explorando");
  TEST_ASSERT_TRUE(buf.enfileirar(mNovo));

  TEST_ASSERT_TRUE(buf.cheio());
  TEST_ASSERT_EQUAL_UINT32(cap, buf.pendentes());
  TEST_ASSERT_EQUAL_UINT32(1, buf.totalDescartesOverflow());

  // Enfileira mais 20 itens para estressar a rotação do anel
  for (uint32_t i = cap + 1; i < cap + 21; i++) {
    MensagemTelemetria m = criarTelemetria(1, i, i * 200, 0, 0, 'N', 0, 0.0f, 7.4f, 0, "explorando");
    buf.enfileirar(m);
  }

  TEST_ASSERT_EQUAL_UINT32(cap, buf.pendentes());
  TEST_ASSERT_EQUAL_UINT32(21, buf.totalDescartesOverflow());
  TEST_ASSERT_EQUAL_UINT32(cap, buf.ocupacaoMaxima());
}

void test_buffer_gerenciamento_queda_e_reconexao() {
  BufferTelemetria buf;

  // Simula queda de conexão em t=10000
  buf.registrarQuedaConexao(10000);

  // Durante a queda, o robô continua navegando e gerando telemetrias locais
  for (uint32_t i = 0; i < 5; i++) {
    MensagemTelemetria m = criarTelemetria(7, i, 10000 + (i * 200), (int8_t)i, 0, 'L', 0, 0.3f, 7.5f, 1, "explorando");
    buf.enfileirar(m);
  }

  // Em t=25000 (relógio do sistema) a conexão é restabelecida; a corrida está em 14500 ms
  MensagemTelemetria evtReconexao;
  TEST_ASSERT_TRUE(buf.gerarEventoReconexao(25000, 14500, 7, 50, evtReconexao));

  TEST_ASSERT_EQUAL_UINT8(EVT_RECONEXAO, evtReconexao.tipo);
  TEST_ASSERT_EQUAL_UINT32(7, evtReconexao.corrida);
  TEST_ASSERT_EQUAL_UINT32(50, evtReconexao.seq);
  TEST_ASSERT_EQUAL_UINT32(14500, evtReconexao.tempo_ms);   // tempo desde a largada, não o relógio
  TEST_ASSERT_EQUAL_UINT32(15000, evtReconexao.offline_ms); // 25000 - 10000
  TEST_ASSERT_EQUAL_UINT16(5, evtReconexao.pendentes);

  // Serialização do evento gerado
  char quadro[256];
  size_t len = serializarQuadro(evtReconexao, quadro, sizeof(quadro));
  TEST_ASSERT_GREATER_THAN(0, len);
  TEST_ASSERT_NOT_NULL(strstr(quadro, "\"evento\":\"reconexao\""));
  TEST_ASSERT_NOT_NULL(strstr(quadro, "\"offline_ms\":15000"));
  TEST_ASSERT_NOT_NULL(strstr(quadro, "\"pendentes\":5"));
}

void test_buffer_reconexao_sem_queda_nao_gera_evento() {
  BufferTelemetria buf;
  MensagemTelemetria evt;

  // Sem queda registrada, não há o que reportar
  TEST_ASSERT_FALSE(buf.gerarEventoReconexao(5000, 5000, 1, 9, evt));
  TEST_ASSERT_TRUE(buf.vazio());

  // Depois de uma reconexão, o estado volta a online e o evento não se repete
  buf.registrarQuedaConexao(1000);
  TEST_ASSERT_TRUE(buf.gerarEventoReconexao(2000, 2000, 1, 9, evt));
  TEST_ASSERT_FALSE(buf.gerarEventoReconexao(3000, 3000, 1, 10, evt));
}

void test_buffer_reconexao_vai_antes_dos_pendentes() {
  BufferTelemetria buf;
  buf.registrarQuedaConexao(1000);
  for (uint32_t i = 0; i < 3; i++) {
    buf.enfileirar(criarTelemetria(7, i, i * 200, 0, 0, 'N', 0, 0.3f, 7.4f, 0, "explorando"));
  }

  MensagemTelemetria evt;
  TEST_ASSERT_TRUE(buf.gerarEventoReconexao(5000, 4200, 7, 3, evt));
  TEST_ASSERT_EQUAL_UINT16(3, evt.pendentes); // só o que ficou retido durante a queda
  TEST_ASSERT_EQUAL_UINT32(4, buf.pendentes()); // o evento também aguarda ACK

  MensagemTelemetria prox;
  bool ehReenvio = true;
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(5000, prox, ehReenvio));
  TEST_ASSERT_FALSE(ehReenvio);
  TEST_ASSERT_EQUAL_UINT8(EVT_RECONEXAO, prox.tipo);
  buf.registrarEnvio(prox.corrida, prox.seq, 5000);

  // Em seguida os pendentes, do mais antigo para o mais novo
  for (uint32_t esperado = 0; esperado < 3; esperado++) {
    TEST_ASSERT_TRUE(buf.obterProximoEnvio(5000, prox, ehReenvio));
    TEST_ASSERT_EQUAL_UINT8(TIPO_TEL, prox.tipo);
    TEST_ASSERT_EQUAL_UINT32(esperado, prox.seq);
    buf.registrarEnvio(prox.corrida, prox.seq, 5000);
  }

  // O evento é confirmado como qualquer outra mensagem
  TEST_ASSERT_TRUE(buf.confirmarAck(7, 3));
}

void test_buffer_ack_fora_de_ordem_reaproveita_slots() {
  BufferTelemetria buf;
  const uint32_t cap = (uint32_t)CAPACIDADE_BUFFER_PADRAO;
  for (uint32_t i = 0; i < cap; i++) {
    buf.enfileirar(criarTelemetria(1, i, i * 200, 0, 0, 'N', 0, 0.0f, 7.4f, 0, "explorando"));
  }

  // O servidor confirma tudo menos o seq 0: sobram "buracos" no meio do anel
  for (uint32_t i = 1; i < cap; i++) {
    TEST_ASSERT_TRUE(buf.confirmarAck(1, i));
  }
  TEST_ASSERT_EQUAL_UINT32(1, buf.pendentes());
  TEST_ASSERT_FALSE(buf.cheio());

  // Todos os slots, menos um, são reaproveitáveis: nada pode ser descartado por overflow
  buf.enfileirar(criarTelemetria(1, cap, cap * 200, 0, 0, 'N', 0, 0.0f, 7.4f, 0, "explorando"));
  TEST_ASSERT_EQUAL_UINT32(0, buf.totalDescartesOverflow());
  TEST_ASSERT_EQUAL_UINT32(2, buf.pendentes());

  // A ordem FIFO se mantém depois da compactação
  MensagemTelemetria prox;
  bool ehReenvio = true;
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(0, prox, ehReenvio));
  TEST_ASSERT_EQUAL_UINT32(0, prox.seq);
  buf.registrarEnvio(prox.corrida, prox.seq, 0);
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(0, prox, ehReenvio));
  TEST_ASSERT_EQUAL_UINT32(cap, prox.seq);
}

void test_buffer_descarta_todos_os_itens_esgotados() {
  BufferTelemetria buf;
  buf.enfileirar(criarTelemetria(1, 0, 0, 0, 0, 'N', 0, 0.0f, 7.4f, 0, "explorando"));
  buf.enfileirar(criarTelemetria(1, 1, 200, 0, 0, 'N', 0, 0.0f, 7.4f, 0, "explorando"));

  // 3 tentativas para cada um dos dois, sem nenhum ACK
  MensagemTelemetria prox;
  bool ehReenvio = false;
  const uint32_t instantes[] = {0, 2000, 4000};
  for (uint32_t t : instantes) {
    for (int k = 0; k < 2; k++) {
      TEST_ASSERT_TRUE(buf.obterProximoEnvio(t, prox, ehReenvio));
      buf.registrarEnvio(prox.corrida, prox.seq, t);
    }
  }

  // Um item novo chega antes de o timeout da 3ª tentativa vencer
  buf.enfileirar(criarTelemetria(1, 2, 400, 0, 0, 'N', 0, 0.0f, 7.4f, 0, "explorando"));

  // Os dois esgotados saem na mesma chamada, sem pular nenhum, e o novo é entregue
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(6000, prox, ehReenvio));
  TEST_ASSERT_FALSE(ehReenvio);
  TEST_ASSERT_EQUAL_UINT32(2, prox.seq);
  TEST_ASSERT_EQUAL_UINT32(2, buf.totalDescartesTentativas());
  TEST_ASSERT_EQUAL_UINT32(1, buf.pendentes());
}

void test_buffer_seq_repetido_entre_corridas() {
  BufferTelemetria buf;
  // O seq reinicia a cada corrida: (1, 5) e (2, 5) coexistem no buffer
  buf.enfileirar(criarTelemetria(1, 5, 1000, 0, 0, 'N', 0, 0.0f, 7.4f, 0, "explorando"));
  buf.enfileirar(criarTelemetria(2, 5, 1000, 0, 0, 'N', 0, 0.0f, 7.4f, 0, "explorando"));

  // Registrar o envio da corrida 2 não pode marcar a mensagem da corrida 1 como enviada
  buf.registrarEnvio(2, 5, 1000);
  MensagemTelemetria prox;
  bool ehReenvio = true;
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(1000, prox, ehReenvio));
  TEST_ASSERT_FALSE(ehReenvio);
  TEST_ASSERT_EQUAL_UINT32(1, prox.corrida);

  // O ACK também diferencia as corridas
  TEST_ASSERT_TRUE(buf.confirmarAck(1, 5));
  TEST_ASSERT_EQUAL_UINT32(1, buf.pendentes());
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(3000, prox, ehReenvio));
  TEST_ASSERT_TRUE(ehReenvio);
  TEST_ASSERT_EQUAL_UINT32(2, prox.corrida);
}

// O buffer ocupa ~100 KB: nos testes de volume fica em memória estática, como no firmware
static BufferTelemetria bufferGrande;

void test_buffer_corrida_completa_sem_conexao_ct_se17() {
  // CT-SE17: corrida de 10 min no maior labirinto, sem nenhuma conexão. Nada pode ser
  // perdido por falta de memória e tudo precisa ser retransmitido, em ordem, ao reconectar.
  BufferTelemetria &buf = bufferGrande;
  buf.limpar();

  const uint32_t corrida = 7;
  const uint32_t total_tel = 3000; // 10 min a 5 Hz
  uint32_t seq = 0;

  buf.registrarQuedaConexao(0);
  TEST_ASSERT_TRUE(buf.enfileirar(criarEventoInicio(corrida, seq++, 0, 8.21f)));
  for (uint32_t k = 1; k <= total_tel; k++) {
    TEST_ASSERT_TRUE(buf.enfileirar(criarTelemetria(corrida, seq++, k * 200, (int8_t)(k % 12),
                                                    (int8_t)(k % 4), 'L', (uint8_t)(k % 16), 0.3f,
                                                    7.4f, (int16_t)(k % 50), "explorando")));
  }
  TEST_ASSERT_TRUE(buf.enfileirar(criarEventoFimConcluida(corrida, seq++, 600000, 600.0f, 412.5f)));

  const uint32_t retidos = total_tel + 2; // inicio + telemetrias + fim
  TEST_ASSERT_EQUAL_UINT32(retidos, buf.pendentes());
  TEST_ASSERT_EQUAL_UINT32(0, buf.totalDescartesOverflow());

  // Reconexão: o evento sai primeiro, depois todos os pendentes na ordem em que foram gerados
  MensagemTelemetria evt;
  TEST_ASSERT_TRUE(buf.gerarEventoReconexao(600500, 600500, corrida, seq, evt));
  TEST_ASSERT_EQUAL_UINT16(retidos, evt.pendentes);
  TEST_ASSERT_EQUAL_UINT32(0, buf.totalDescartesOverflow());

  MensagemTelemetria prox;
  bool ehReenvio = true;
  uint32_t entregues = 0;
  uint32_t esperado = 0;
  while (entregues <= retidos + 1 && buf.obterProximoEnvio(600500, prox, ehReenvio)) {
    TEST_ASSERT_FALSE(ehReenvio);
    if (entregues == 0) {
      TEST_ASSERT_EQUAL_UINT8(EVT_RECONEXAO, prox.tipo);
    } else {
      TEST_ASSERT_EQUAL_UINT32(esperado++, prox.seq);
    }
    buf.registrarEnvio(prox.corrida, prox.seq, 600500);
    TEST_ASSERT_TRUE(buf.confirmarAck(prox.corrida, prox.seq));
    entregues++;
  }

  TEST_ASSERT_EQUAL_UINT32(retidos + 1, entregues);
  TEST_ASSERT_EQUAL_UINT32(retidos, esperado);
  TEST_ASSERT_EQUAL_UINT32(retidos + 1, buf.totalConfirmados());
  TEST_ASSERT_EQUAL_UINT32(0, buf.totalDescartesTentativas());
  TEST_ASSERT_TRUE(buf.vazio());
}

// Enfileira, recupera e confere que o pacote é serializado exatamente como o original
static void conferirIdaEVolta(const MensagemTelemetria &original) {
  BufferTelemetria &buf = bufferGrande;
  buf.limpar();

  char esperado[256];
  char obtido[256];
  TEST_ASSERT_GREATER_THAN(0, serializarQuadro(original, esperado, sizeof(esperado)));

  TEST_ASSERT_TRUE(buf.enfileirar(original));
  MensagemTelemetria saida;
  bool ehReenvio = true;
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(0, saida, ehReenvio));
  TEST_ASSERT_GREATER_THAN(0, serializarQuadro(saida, obtido, sizeof(obtido)));

  TEST_ASSERT_EQUAL_STRING(esperado, obtido);
}

void test_buffer_preserva_pacotes_do_contrato() {
  // Exemplo válido da seção 7 do contrato: o frame recuperado tem o CRC 2917
  MensagemTelemetria valido = criarTelemetria(7, 42, 8400, 2, 1, 'L', 9, 0.312f, 7.86f, 3, "explorando");
  conferirIdaEVolta(valido);
  BufferTelemetria &buf = bufferGrande;
  MensagemTelemetria saida;
  bool ehReenvio = true;
  char quadro[256];
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(0, saida, ehReenvio));
  TEST_ASSERT_GREATER_THAN(0, serializarQuadro(saida, quadro, sizeof(quadro)));
  TEST_ASSERT_EQUAL_STRING("2917", strrchr(quadro, '|') + 1);

  // Eventos da seção 7
  conferirIdaEVolta(criarEventoInicio(7, 0, 0, 8.21f));
  conferirIdaEVolta(criarEventoBateriaCritica(7, 97, 19350, 6.98f));
  conferirIdaEVolta(criarEventoReconexao(7, 160, 32010, 20150, 101));
  conferirIdaEVolta(criarEventoFimConcluida(7, 311, 62240, 62.24f, 412.5f));
  conferirIdaEVolta(criarEventoFimAbandonada(8, 55, 11020, MOTIVO_BATERIA_CRITICA, 398.0f));
}

void test_buffer_preserva_variacoes_de_campos() {
  const char *estados[] = {"explorando", "retornando", "rapido", "parado"};
  const char orientacoes[] = {'N', 'S', 'L', 'O'};
  for (int i = 0; i < 4; i++) {
    conferirIdaEVolta(criarTelemetria(3, (uint32_t)i, 200u * (uint32_t)i, (int8_t)(i * 3), (int8_t)i,
                                      orientacoes[i], (uint8_t)(i * 5), 0.123f * (float)i,
                                      7.01f + (float)i, (int16_t)(i * 10), estados[i]));
  }

  // Limites das faixas do contrato e valor_flood nulo
  conferirIdaEVolta(criarTelemetria(1, 0, 0, 0, 0, 'N', 0, 0.0f, 0.0f, -1, "parado"));
  conferirIdaEVolta(criarTelemetria(1, 1, 200, 11, 3, 'S', 15, 5.0f, 9.0f, 0, "rapido"));
  conferirIdaEVolta(criarTelemetria(1, 2, 400, 11, 3, 'S', 15, 4.999f, 8.99f, 32767, "rapido"));

  const MotivoAbandono motivos[] = {MOTIVO_FALHA_SENSOR_BATERIA, MOTIVO_COLISAO,
                                    MOTIVO_TEMPO_ESGOTADO, MOTIVO_MANUAL};
  for (MotivoAbandono motivo : motivos) {
    conferirIdaEVolta(criarEventoFimAbandonada(2, 9, 5000, motivo, 1234.5f));
  }
}

void test_buffer_recusa_estado_fora_do_contrato() {
  BufferTelemetria &buf = bufferGrande;
  buf.limpar();

  MensagemTelemetria invalida = criarTelemetria(1, 0, 0, 0, 0, 'N', 0, 0.0f, 7.4f, 0, "voando");
  TEST_ASSERT_FALSE(buf.enfileirar(invalida));
  TEST_ASSERT_TRUE(buf.vazio());
  TEST_ASSERT_EQUAL_UINT32(0, buf.totalEnfileirados());
}

void test_buffer_memoria_estatica_cabe_na_ram() {
  // RNF03-S: o firmware inteiro pode usar no máximo 80% da RAM (320 KB no ESP32). O buffer
  // é estático (sem malloc): 3200 itens de 32 B, ~100 KB (~31%), com folga para o resto.
  TEST_ASSERT_EQUAL_UINT32(32, sizeof(ItemBuffer));
  TEST_ASSERT_LESS_THAN(110 * 1024, sizeof(BufferTelemetria));
}

// ==============================================================================
// Execução dos Testes Unity
// ==============================================================================

void test_offline_preserva_mensagens_com_loop_ativo() {
  BufferTelemetria &buf = bufferGrande;
  buf.limpar();
  buf.enfileirar(criarEventoInicio(1, 0, 0, 8.0f));
  for (uint32_t i = 1; i <= 50; i++)
    buf.enfileirar(criarTelemetria(1, i, i * 200, 0, 0, 'N', 0, 0, 8, 0, "parado"));
  buf.registrarQuedaConexao(0);
  MensagemTelemetria msg;
  bool reenvio;
  for (uint32_t t = 0; t < 20000; t += 200) {
    TEST_ASSERT_FALSE(buf.obterProximoEnvio(t, msg, reenvio));
    // Mesmo uma chamada indevida não pode consumir tentativas offline.
    buf.registrarEnvio(1, 0, t);
  }
  TEST_ASSERT_EQUAL_UINT32(51, buf.pendentes());
  TEST_ASSERT_EQUAL_UINT32(0, buf.totalDescartesTentativas());
  TEST_ASSERT_TRUE(buf.gerarEventoReconexao(20000, 20000, 1, 51, msg));
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(20000, msg, reenvio));
  TEST_ASSERT_EQUAL_UINT8(EVT_RECONEXAO, msg.tipo);
  buf.registrarEnvio(1, 51, 20000);
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(20000, msg, reenvio));
  TEST_ASSERT_EQUAL_UINT8(EVT_INICIO, msg.tipo);
  TEST_ASSERT_FALSE(reenvio);
}

void test_retomada_ordena_enviados_e_novos() {
  BufferTelemetria &buf = bufferGrande;
  buf.limpar();
  buf.enfileirar(criarEventoInicio(1, 0, 0, 8));
  buf.registrarEnvio(1, 0, 0);
  buf.registrarQuedaConexao(100);
  buf.enfileirar(criarEventoBateriaCritica(1, 1, 200, 6.9f));
  MensagemTelemetria msg;
  bool reenvio;
  TEST_ASSERT_TRUE(buf.gerarEventoReconexao(10000, 10000, 1, 2, msg));
  buf.enfileirar(criarEventoFimConcluida(1, 3, 10100, 10.1f, 100));
  const uint32_t ordem[] = {2, 0, 1, 3};
  for (uint32_t seq : ordem) {
    TEST_ASSERT_TRUE(buf.obterProximoEnvio(10100, msg, reenvio));
    TEST_ASSERT_EQUAL_UINT32(seq, msg.seq);
    TEST_ASSERT_EQUAL(seq == 0, reenvio);
    buf.registrarEnvio(1, seq, 10100);
  }
  TEST_ASSERT_FALSE(buf.obterProximoEnvio(10100, msg, reenvio));
}

void test_offline_pausa_timeout_da_ultima_tentativa() {
  BufferTelemetria &buf = bufferGrande;
  buf.limpar();
  buf.enfileirar(criarEventoInicio(1, 0, 0, 8));
  buf.registrarEnvio(1, 0, 0);
  buf.registrarEnvio(1, 0, 2000);
  buf.registrarEnvio(1, 0, 4000);
  buf.registrarQuedaConexao(4500);
  MensagemTelemetria msg;
  bool reenvio;
  TEST_ASSERT_FALSE(buf.obterProximoEnvio(20000, msg, reenvio));
  TEST_ASSERT_EQUAL_UINT32(0, buf.totalDescartesTentativas());
  TEST_ASSERT_TRUE(buf.gerarEventoReconexao(20000, 20000, 1, 1, msg));
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(20000, msg, reenvio));
  buf.registrarEnvio(1, 1, 20000);
  buf.confirmarAck(1, 1);
  TEST_ASSERT_FALSE(buf.obterProximoEnvio(21499, msg, reenvio));
  TEST_ASSERT_EQUAL_UINT32(1, buf.pendentes());
  TEST_ASSERT_FALSE(buf.obterProximoEnvio(21500, msg, reenvio));
  TEST_ASSERT_EQUAL_UINT32(1, buf.totalDescartesTentativas());
}

void test_overflow_preserva_eventos_e_recusa_buffer_so_de_eventos() {
  BufferTelemetria &buf = bufferGrande;
  buf.limpar();
  buf.enfileirar(criarEventoInicio(1, 0, 0, 8));
  buf.enfileirar(criarEventoBateriaCritica(1, 1, 200, 6.9f));
  buf.enfileirar(criarEventoFimConcluida(1, 2, 400, 0.4f, 100));
  for (uint32_t i = 3; i < buf.capacidade(); i++)
    buf.enfileirar(criarTelemetria(1, i, i * 200, 0, 0, 'N', 0, 0, 8, 0, "parado"));
  TEST_ASSERT_TRUE(buf.enfileirar(criarEventoInicio(2, 0, 0, 8)));
  TEST_ASSERT_EQUAL_UINT32(1, buf.totalDescartesOverflow());
  TEST_ASSERT_FALSE(buf.confirmarAck(1, 3));
  for (uint32_t seq = 0; seq < 3; seq++) TEST_ASSERT_TRUE(buf.confirmarAck(1, seq));
  TEST_ASSERT_TRUE(buf.confirmarAck(2, 0));

  buf.limpar();
  for (uint32_t i = 0; i < buf.capacidade(); i++)
    TEST_ASSERT_TRUE(buf.enfileirar(criarEventoInicio(i + 1, 0, 0, 8)));
  TEST_ASSERT_FALSE(buf.enfileirar(criarTelemetria(1, 1, 200, 0, 0, 'N', 0, 0, 8, 0, "parado")));
  buf.registrarQuedaConexao(0);
  MensagemTelemetria msg;
  bool reenvio;
  TEST_ASSERT_FALSE(buf.gerarEventoReconexao(1000, 1000, 1, 2, msg));
  TEST_ASSERT_FALSE(buf.obterProximoEnvio(1000, msg, reenvio));
  TEST_ASSERT_EQUAL_UINT32(2, buf.totalDescartesOverflow());
  TEST_ASSERT_TRUE(buf.confirmarAck(1, 0));
  TEST_ASSERT_TRUE(buf.gerarEventoReconexao(1000, 1000, 1, 2, msg));
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(1000, msg, reenvio));
  TEST_ASSERT_EQUAL_UINT8(EVT_RECONEXAO, msg.tipo);
}

void test_buffer_arredondamento_identico_ao_serializador() {
  const float tensoes[] = {7.125f, 7.135f, 7.145f, 8.005f};
  for (float tensao : tensoes) conferirIdaEVolta(criarEventoInicio(1, 0, 0, tensao));
  conferirIdaEVolta(criarTelemetria(1, 1, 200, 0, 0, 'N', 0, 0.0625f, 7.125f, 0, "parado"));
  conferirIdaEVolta(criarEventoFimConcluida(1, 2, 1000, 1.125f, 412.25f));
  // Leituras com mais casas que a resolução do protocolo.
  for (int i = 0; i <= 9000; i++)
    conferirIdaEVolta(criarEventoInicio(1, 0, 0, (float)i / 1000.0f));
}

void test_buffer_timeout_e_reconexao_com_rollover() {
  BufferTelemetria &buf = bufferGrande;
  buf.limpar();
  buf.enfileirar(criarEventoInicio(1, 0, 0, 8));
  buf.registrarEnvio(1, 0, UINT32_MAX - 999);
  MensagemTelemetria msg;
  bool reenvio;
  TEST_ASSERT_FALSE(buf.obterProximoEnvio(999, msg, reenvio));
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(1000, msg, reenvio));
  TEST_ASSERT_TRUE(reenvio);
  buf.registrarQuedaConexao(UINT32_MAX - 499);
  TEST_ASSERT_TRUE(buf.gerarEventoReconexao(500, 2000, 1, 1, msg));
  TEST_ASSERT_EQUAL_UINT32(1000, msg.offline_ms);

  buf.limpar();
  buf.enfileirar(criarEventoInicio(1, 0, 0, 8));
  buf.registrarEnvio(1, 0, UINT32_MAX - 4999);
  buf.registrarEnvio(1, 0, UINT32_MAX - 2999);
  buf.registrarEnvio(1, 0, UINT32_MAX - 999);
  buf.registrarQuedaConexao(UINT32_MAX - 499);
  TEST_ASSERT_TRUE(buf.gerarEventoReconexao(500, 2000, 1, 1, msg));
  TEST_ASSERT_TRUE(buf.obterProximoEnvio(500, msg, reenvio));
  buf.registrarEnvio(1, 1, 500);
  buf.confirmarAck(1, 1);
  TEST_ASSERT_FALSE(buf.obterProximoEnvio(1999, msg, reenvio));
  TEST_ASSERT_EQUAL_UINT32(1, buf.pendentes());
  TEST_ASSERT_FALSE(buf.obterProximoEnvio(2000, msg, reenvio));
  TEST_ASSERT_EQUAL_UINT32(1, buf.totalDescartesTentativas());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_buffer_arredondamento_identico_ao_serializador);
  RUN_TEST(test_buffer_timeout_e_reconexao_com_rollover);
  RUN_TEST(test_offline_preserva_mensagens_com_loop_ativo);
  RUN_TEST(test_retomada_ordena_enviados_e_novos);
  RUN_TEST(test_offline_pausa_timeout_da_ultima_tentativa);
  RUN_TEST(test_overflow_preserva_eventos_e_recusa_buffer_so_de_eventos);

  // Serialização e formato
  RUN_TEST(test_serializar_telemetria_exemplo_contrato);
  RUN_TEST(test_serializar_telemetria_com_flood_nulo);
  RUN_TEST(test_serializar_evento_inicio);
  RUN_TEST(test_serializar_evento_bateria_critica);
  RUN_TEST(test_serializar_evento_reconexao);
  RUN_TEST(test_serializar_evento_fim_concluida);
  RUN_TEST(test_serializar_evento_fim_abandonada);
  RUN_TEST(test_serializar_evento_fim_motivos_diversos);
  RUN_TEST(test_serializar_buffer_insuficiente);

  // Parsing de ACK
  RUN_TEST(test_processar_ack_valido);
  RUN_TEST(test_processar_ack_crc_invalido);
  RUN_TEST(test_processar_ack_malformado);

  // Buffer e Retransmissão
  RUN_TEST(test_buffer_estado_inicial_e_enfileirar);
  RUN_TEST(test_buffer_obter_proximo_envio_e_confirmar_ack);
  RUN_TEST(test_buffer_retransmissao_por_timeout_2s);
  RUN_TEST(test_buffer_descarte_apos_3_tentativas);
  RUN_TEST(test_buffer_prioridade_novos_sobre_reenvios);
  RUN_TEST(test_buffer_overflow_ring_buffer_na_capacidade_maxima);
  RUN_TEST(test_buffer_gerenciamento_queda_e_reconexao);
  RUN_TEST(test_buffer_reconexao_sem_queda_nao_gera_evento);
  RUN_TEST(test_buffer_reconexao_vai_antes_dos_pendentes);
  RUN_TEST(test_buffer_ack_fora_de_ordem_reaproveita_slots);
  RUN_TEST(test_buffer_descarta_todos_os_itens_esgotados);
  RUN_TEST(test_buffer_seq_repetido_entre_corridas);
  RUN_TEST(test_buffer_corrida_completa_sem_conexao_ct_se17);
  RUN_TEST(test_buffer_preserva_pacotes_do_contrato);
  RUN_TEST(test_buffer_preserva_variacoes_de_campos);
  RUN_TEST(test_buffer_recusa_estado_fora_do_contrato);
  RUN_TEST(test_buffer_memoria_estatica_cabe_na_ram);

  return UNITY_END();
}
