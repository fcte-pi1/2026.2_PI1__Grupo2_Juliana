#include "telemetria_wifi.h"

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <new>

#include "buffer_telemetria.h"

namespace
{
// ~100 KB. Não cabe como global (estoura a região .dram0.bss do ESP32), então
// é reservado no heap logo no boot, antes do Wi-Fi fragmentar a RAM.
BufferTelemetria *pbuffer = nullptr;
WebSocketsClient ws;
SemaphoreHandle_t trava = nullptr;

uint32_t corrida = 0; // 0 = sem corrida ativa
uint32_t proximoSeq = 0;
uint32_t inicioCorrida = 0;
bool conectado = false;
bool houveQueda = false;
EstatisticasTelemetria stats = {};

// RTT (envio -> ack) das últimas mensagens. O backend grava, responde o ack e
// só então repassa ao painel: o RTT é um limite superior do atraso robô ->
// painel, sem precisar sincronizar o relógio do ESP32 com o do computador.
struct Envio
{
    uint32_t corrida, seq, instante;
};
Envio envios[64] = {};
uint8_t proximoEnvio = 0;
uint64_t somaRtt = 0;

void registrarRtt(uint32_t c, uint32_t s)
{
    for (Envio &e : envios)
        if (e.instante != 0 && e.corrida == c && e.seq == s)
        {
            uint32_t rtt = millis() - e.instante;
            e.instante = 0;
            somaRtt += rtt;
            stats.rttAmostras++;
            stats.rttUltimo = rtt;
            stats.rttMedio = (uint32_t)(somaRtt / stats.rttAmostras);
            if (rtt > stats.rttMax)
                stats.rttMax = rtt;
            if (rtt > 500)
                stats.rttAcima500++;
            return;
        }
}

struct Trava
{
    Trava() { xSemaphoreTake(trava, portMAX_DELAY); }
    ~Trava() { xSemaphoreGive(trava); }
};

void aoEvento(WStype_t tipo, uint8_t *dados, size_t n)
{
    switch (tipo)
    {
    case WStype_CONNECTED:
    {
        Trava t;
        conectado = true;
        if (houveQueda && corrida != 0)
        {
            MensagemTelemetria evt;
            if (pbuffer->gerarEventoReconexao(millis(), millis() - inicioCorrida, corrida, proximoSeq, evt))
                proximoSeq++;
        }
        houveQueda = false;
        break;
    }
    case WStype_DISCONNECTED:
    {
        Trava t;
        if (conectado && corrida != 0)
        {
            pbuffer->registrarQuedaConexao(millis());
            houveQueda = true;
            stats.quedas++;
        }
        conectado = false;
        break;
    }
    case WStype_TEXT:
    {
        char quadro[96];
        size_t m = n < sizeof(quadro) - 1 ? n : sizeof(quadro) - 1;
        memcpy(quadro, dados, m);
        quadro[m] = '\0';
        uint32_t c = 0, s = 0;
        if (processarAck(quadro, c, s))
        {
            Trava t;
            if (pbuffer->confirmarAck(c, s))
                stats.acks++;
            registrarRtt(c, s);
        }
        break;
    }
    default:
        break;
    }
}

// Envia o que estiver pendente, sem segurar a trava durante a rede.
void enviarPendentes()
{
    for (int i = 0; i < 10; i++)
    {
        MensagemTelemetria msg;
        bool reenvio = false;
        {
            Trava t;
            if (!pbuffer->obterProximoEnvio(millis(), msg, reenvio))
                return;
        }
        char quadro[320];
        size_t n = serializarQuadro(msg, quadro, sizeof(quadro));
        if (n == 0 || !ws.sendTXT(quadro, n))
            return;
        Trava t;
        pbuffer->registrarEnvio(msg.corrida, msg.seq, millis());
        envios[proximoEnvio] = {msg.corrida, msg.seq, millis()};
        proximoEnvio = (uint8_t)((proximoEnvio + 1) % 64);
        stats.enviados++;
        if (reenvio)
            stats.reenvios++;
    }
}

void tarefa(void *)
{
    for (;;)
    {
        ws.loop();
        if (conectado)
            enviarPendentes();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
} // namespace

namespace TelemetriaWifi
{
bool reservarMemoria()
{
    if (pbuffer != nullptr)
        return true;
    void *mem = heap_caps_malloc(sizeof(BufferTelemetria), MALLOC_CAP_8BIT);
    if (mem == nullptr)
        return false;
    pbuffer = new (mem) BufferTelemetria();
    return true;
}

bool iniciar(const char *ssid, const char *senha, const char *host, uint16_t porta)
{
    if (ssid == nullptr || ssid[0] == '\0')
        return false;
    trava = xSemaphoreCreateMutex();
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // menos latência
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, senha);

    ws.begin(host, porta, "/ws/telemetria");
    ws.onEvent(aoEvento);
    ws.setReconnectInterval(1000);
    // Ping a cada 1 s: detecta queda silenciosa da rede em ~3 s.
    ws.enableHeartbeat(1000, 1000, 2);

    // Núcleo 0: o núcleo 1 desta ESP32 tem defeito na cache (platformio.ini).
    xTaskCreatePinnedToCore(tarefa, "telemetria", 8192, nullptr, 1, nullptr, 0);
    return true;
}

void novaCorrida(uint32_t c, uint32_t inicio_ms)
{
    Trava t;
    corrida = c;
    proximoSeq = 0;
    inicioCorrida = inicio_ms;
    houveQueda = false;
}

void encerrarCorrida()
{
    Trava t;
    corrida = 0;
}

bool publicar(MensagemTelemetria msg)
{
    if (trava == nullptr)
        return false;
    Trava t;
    msg.corrida = corrida;
    msg.seq = proximoSeq;
    if (!pbuffer->enfileirar(msg))
        return false;
    proximoSeq++;
    return true;
}

void zerarRtt()
{
    if (trava == nullptr)
        return;
    Trava t;
    somaRtt = 0;
    stats.rttAmostras = stats.rttUltimo = stats.rttMedio = stats.rttMax = stats.rttAcima500 = 0;
}

uint32_t corridaAtual()
{
    if (trava == nullptr)
        return 0;
    Trava t;
    return corrida;
}

EstatisticasTelemetria estatisticas()
{
    EstatisticasTelemetria e = {};
    if (trava == nullptr)
        return e;
    Trava t;
    e = stats;
    e.wifi = WiFi.status() == WL_CONNECTED;
    e.servidor = conectado;
    e.pendentes = pbuffer->pendentes();
    e.ocupacaoMaxima = pbuffer->ocupacaoMaxima();
    e.descartes = pbuffer->totalDescartesOverflow() + pbuffer->totalDescartesTentativas();
    return e;
}
} // namespace TelemetriaWifi
