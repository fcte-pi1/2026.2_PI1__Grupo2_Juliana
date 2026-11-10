// Firmware do Micromouse (ESP32-DEVKIT-V1).
//
// Nesta etapa (robô sem bateria e sem estrutura) ele lê os sensores e envia
// a telemetria pelo Wi-Fi com dados reais, parado na bancada (#215), e aceita
// atualização pelo Wi-Fi (OTA, #217). Os motores ficam desligados.
//
// Comandos pelo monitor serial (115200) ou pelo botão BOOT:
//   i  inicia uma corrida (evento 'inicio')       BOOT: inicia/encerra
//   f  encerra a corrida como concluída (evento 'fim')
//   a  encerra a corrida como abandonada (manual)
//   z  zera o yaw (a orientação atual vira o Norte)
//   s  mostra o estado agora
//   r  zera as estatísticas de RTT (envio -> ack)
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_heap_caps.h>

#include "pacote.h"
#include "pinos.h"
#include "sensores.h"
#include "telemetria_wifi.h"

#if __has_include("credenciais.h")
#include "credenciais.h"
#else
#define WIFI_SSID ""
#define WIFI_SENHA ""
#define SERVIDOR_HOST ""
#define SERVIDOR_PORTA 8000
#define OTA_SENHA ""
#endif

namespace
{
const uint32_t PERIODO_TELEMETRIA_MS = 200; // 5 Hz (docs/4.4.7)
const uint32_t PERIODO_STATUS_MS = 1000;

Sensores sensores;
Preferences preferencias;
bool wifiAtivo = false;
bool otaIniciado = false;

bool emCorrida = false;
uint32_t inicioCorrida = 0;
uint32_t ultimaTelemetria = 0;
uint32_t ultimoStatus = 0;
double somaCorrente = 0;
uint32_t amostrasCorrente = 0;

float tensaoContrato()
{
    float v = sensores.leitura().tensao_v;
    return v < 0 ? 0 : (v > 9 ? 9 : v); // faixa do contrato: 0 a 9 V
}

void iniciarCorrida()
{
    if (emCorrida)
        return;
    // Contador de corridas do robô, guardado na flash (sobrevive ao reset).
    uint32_t numero = preferencias.getUInt("corrida", 0) + 1;
    preferencias.putUInt("corrida", numero);
    sensores.zerarYaw();
    inicioCorrida = millis();
    somaCorrente = 0;
    amostrasCorrente = 0;
    TelemetriaWifi::novaCorrida(numero, inicioCorrida);
    TelemetriaWifi::publicar(criarEventoInicio(numero, 0, 0, tensaoContrato()));
    emCorrida = true;
    ultimaTelemetria = millis() - PERIODO_TELEMETRIA_MS; // primeira tel já no próximo laço
    Serial.printf("\n>>> corrida %u iniciada\n", numero);
}

void encerrarCorrida(bool concluida)
{
    if (!emCorrida)
        return;
    uint32_t c = TelemetriaWifi::corridaAtual();
    uint32_t t = millis() - inicioCorrida;
    float consumo = amostrasCorrente ? (float)(somaCorrente / amostrasCorrente) : 0;
    if (consumo < 0)
        consumo = 0;
    MensagemTelemetria fim = concluida
                                 ? criarEventoFimConcluida(c, 0, t, t / 1000.0f, consumo)
                                 : criarEventoFimAbandonada(c, 0, t, MOTIVO_MANUAL, consumo);
    TelemetriaWifi::publicar(fim);
    TelemetriaWifi::encerrarCorrida();
    emCorrida = false;
    Serial.printf("\n>>> corrida %u encerrada (%s)\n", c, concluida ? "concluida" : "abandonada");
}

void enviarTelemetria()
{
    const LeituraSensores &l = sensores.leitura();
    somaCorrente += l.corrente_ma;
    amostrasCorrente++;
    // Parado na bancada: posição (0,0), velocidade 0 e sem Flood Fill (null).
    MensagemTelemetria tel = criarTelemetria(0, 0, millis() - inicioCorrida, 0, 0,
                                             letra(sensores.orientacao()), sensores.mascaraParedes(),
                                             0.0f, tensaoContrato(), -1, "parado");
    TelemetriaWifi::publicar(tel);
}

void mostrarStatus()
{
    const LeituraSensores &l = sensores.leitura();
    EstatisticasTelemetria e = TelemetriaWifi::estatisticas();
    Serial.printf("[%6lus] wifi=%d srv=%d corrida=%u env=%u reenv=%u ack=%u pend=%u queda=%u "
                  "rtt=%u/%u/%u>500:%u | "
                  "E=%4u F=%4u D=%4u mm paredes=%2u yaw=%6.1f %c | %.2fV %.1fmA | heap=%u\n",
                  millis() / 1000, e.wifi, e.servidor, TelemetriaWifi::corridaAtual(), e.enviados,
                  e.reenvios, e.acks, e.pendentes, e.quedas, e.rttUltimo, e.rttMedio, e.rttMax,
                  e.rttAcima500, l.distancia_mm[LADO_ESQUERDO],
                  l.distancia_mm[LADO_FRONTAL], l.distancia_mm[LADO_DIREITO],
                  sensores.mascaraParedes(), l.yawGraus, letra(sensores.orientacao()), l.tensao_v,
                  l.corrente_ma, ESP.getFreeHeap());
}

void tratarComando(char c)
{
    switch (c)
    {
    case 'i': iniciarCorrida(); break;
    case 'f': encerrarCorrida(true); break;
    case 'a': encerrarCorrida(false); break;
    case 'z': sensores.zerarYaw(); Serial.println(">>> yaw zerado"); break;
    case 's': mostrarStatus(); break;
    case 'r': TelemetriaWifi::zerarRtt(); Serial.println(">>> RTT zerado"); break;
    default: break;
    }
}

void verificarBotao()
{
    // Só reage a uma descida (solto -> apertado). O pino começa como "apertado"
    // para não disparar se o reset pelo USB deixar o BOOT em nível baixo.
    static bool anterior = false;
    static uint32_t ultimaMudanca = 0;
    if (millis() < 3000)
        return;
    bool agora = digitalRead(PINO_BOTAO_BOOT);
    if (agora != anterior && millis() - ultimaMudanca > 50)
    {
        ultimaMudanca = millis();
        anterior = agora;
        if (!agora) // apertou
            emCorrida ? encerrarCorrida(true) : iniciarCorrida();
    }
}

void iniciarOta()
{
    ArduinoOTA.setHostname("micromouse");
    if (OTA_SENHA[0] != '\0')
        ArduinoOTA.setPassword(OTA_SENHA);
    ArduinoOTA.onStart([]() {
        if (emCorrida)
            encerrarCorrida(false);
        Serial.println("\n>>> OTA: recebendo firmware novo");
    });
    ArduinoOTA.onEnd([]() { Serial.println("\n>>> OTA: concluido, reiniciando"); });
    ArduinoOTA.onError([](ota_error_t erro) { Serial.printf("\n>>> OTA: erro %u\n", erro); });
    ArduinoOTA.begin();
    otaIniciado = true;
    Serial.printf(">>> OTA pronto em %s (micromouse.local)\n", WiFi.localIP().toString().c_str());
}
} // namespace

void setup()
{
    pinMode(PINO_DRV_SLEEP, OUTPUT);
    digitalWrite(PINO_DRV_SLEEP, LOW); // motores desligados nesta etapa
    pinMode(PINO_BOTAO_BOOT, INPUT_PULLUP);
    pinMode(PINO_LED, OUTPUT);
    Serial.begin(115200);
    delay(300);
    Serial.printf("\n=== Micromouse iniciado (firmware de %s %s) ===\n", __DATE__, __TIME__);
    // O buffer da telemetria (~100 KB) é reservado antes de tudo.
    uint32_t maiorBloco = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    bool memoria = TelemetriaWifi::reservarMemoria();
    Serial.printf("Buffer de telemetria: %s (maior bloco livre antes: %u B)\n",
                  memoria ? "reservado" : "SEM MEMORIA", maiorBloco);

    preferencias.begin("micromouse", false);
    uint8_t ok = sensores.iniciar();
    const LeituraSensores &l = sensores.leitura();
    Serial.printf("Sensores: %u de 5 responderam (IMU=%d INA226=%d)\n", ok, l.imuOk, l.inaOk);

    wifiAtivo = TelemetriaWifi::iniciar(WIFI_SSID, WIFI_SENHA, SERVIDOR_HOST, SERVIDOR_PORTA);
    if (wifiAtivo)
        Serial.printf("Wi-Fi: conectando em \"%s\", servidor ws://%s:%u/ws/telemetria\n", WIFI_SSID,
                      SERVIDOR_HOST, SERVIDOR_PORTA);
    else
        Serial.println("Wi-Fi: desligado (falta include/credenciais.h)");
    Serial.println("Comandos: i=inicia  f=fim  a=abandona  z=zera yaw  s=status  r=zera RTT  (BOOT inicia/encerra)");
}

void loop()
{
    sensores.atualizar();

    uint32_t agora = millis();
    if (emCorrida && agora - ultimaTelemetria >= PERIODO_TELEMETRIA_MS)
    {
        // Soma o período em vez de copiar o relógio: o atraso de um laço não
        // se acumula e a média fica em 5 Hz. Se atrasar muito, recomeça.
        ultimaTelemetria += PERIODO_TELEMETRIA_MS;
        if (agora - ultimaTelemetria >= PERIODO_TELEMETRIA_MS)
            ultimaTelemetria = agora;
        enviarTelemetria();
    }
    if (agora - ultimoStatus >= PERIODO_STATUS_MS)
    {
        ultimoStatus = agora;
        mostrarStatus();
        digitalWrite(PINO_LED, TelemetriaWifi::estatisticas().servidor); // LED aceso = servidor conectado
    }

    while (Serial.available())
        tratarComando((char)Serial.read());
    verificarBotao();

    if (wifiAtivo && WiFi.status() == WL_CONNECTED)
    {
        if (!otaIniciado)
            iniciarOta();
        ArduinoOTA.handle();
    }
}
