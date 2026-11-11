// Firmware do Micromouse (ESP32-DEVKIT-V1).
//
// Lê os sensores, monitora a bateria, aciona os motores (limitados a 5 V) e
// envia a telemetria pelo Wi-Fi. A navegação (Flood Fill) roda no núcleo 1.
//
// A bateria e o USB não podem ficar ligados juntos: com a bateria, use o
// console pelo Wi-Fi (python console_wifi.py <IP>, porta 23) e grave por OTA.
//
// Comandos (serial ou console pelo Wi-Fi) e botão BOOT:
//   i  inicia uma corrida (evento 'inicio')       BOOT: inicia/encerra
//   f  encerra a corrida como concluída            a  encerra como abandonada
//   c  calibração (sensores, bateria, encoders e motores)
//   l  log de erros                                s  estado agora
//   m  teste dos motores: RODAS NO AR (PWM x pulsos/s x corrente)
//   e  mostra e zera os encoders (para medir os pulsos por volta da roda)
//   p  PARA os motores                             z  zera o yaw
//   r  zera as estatísticas de RTT
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include "console.h"
#include "log_erros.h"
#include "monitor_bateria.h"
#include "motores.h"
#include "navegacao_nucleo1.h"
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
const uint32_t PERIODO_BATERIA_MS = 200;
const uint32_t PERIODO_STATUS_MS = 1000;

Sensores sensores;
MonitorBateria bateria;
Preferences preferencias;
bool wifiAtivo = false;
bool otaIniciado = false;

bool emCorrida = false;
uint32_t inicioCorrida = 0;
uint32_t ultimaTelemetria = 0;
uint32_t ultimaBateria = 0;
uint32_t ultimoStatus = 0;

float tensaoContrato()
{
    float v = bateria.presente() ? bateria.tensao() : sensores.leitura().tensao_v;
    return v < 0 ? 0 : (v > 9 ? 9 : v); // faixa do contrato: 0 a 9 V
}

// ---------- calibração (#213) ----------

// Gira cada roda um pouco para frente e de volta e confere o encoder. Move o
// robô alguns milímetros e volta.
void testarMotorNoLugar(const char *nome, float e, float d, int32_t (*contar)())
{
    int32_t antes = contar();
    uint32_t t0 = millis();
    Motores::definir(e, d);
    while (millis() - t0 < 300)
    {
        Motores::atualizar(millis());
        delay(5);
    }
    int32_t andou = contar() - antes;
    Motores::definir(-e, -d);
    t0 = millis();
    while (millis() - t0 < 300)
    {
        Motores::atualizar(millis());
        delay(5);
    }
    Motores::parar();
    if (andou == 0)
        LogErros::registrar(true, "motor/encoder %s: girou sem pulsos do encoder", nome);
    else if (andou < 0)
        LogErros::registrar(false, "motor/encoder %s: sentido invertido (%ld pulsos); troque INVERTE_*", nome,
                            (long)andou);
}

// Calibração automática (RF17-S): sensores respondendo, bateria, yaw e
// encoders zerados, motores girando. Devolve false se algo impede a corrida.
bool calibrar(bool testarMotores)
{
    LogErros::limparFatais();
    const LeituraSensores &l = sensores.leitura();
    const char *nomes[] = {"esquerdo", "frontal", "direito"};
    for (int i = 0; i < 3; i++)
        if (l.defeito[i])
            LogErros::registrar(true, "VL53L0X %s sem resposta", nomes[i]);
    if (!l.imuOk)
        LogErros::registrar(true, "BNO085 sem resposta");
    if (!l.inaOk)
        LogErros::registrar(true, "INA226 sem resposta (sem leitura da bateria)");

    if (!bateria.presente())
        LogErros::registrar(false, "sem bateria (%.2f V): so USB, motores bloqueados", l.tensao_v);
    else if (bateria.tensao() < MonitorBateria::PARADA_V)
        LogErros::registrar(true, "bateria abaixo do minimo: %.2f V", bateria.tensao());
    else if (bateria.tensao() < MonitorBateria::AVISO_V)
        LogErros::registrar(false, "bateria baixa: %.2f V", bateria.tensao());

    sensores.zerarYaw();
    Encoders::zerar();
    if (testarMotores && bateria.presente())
    {
        testarMotorNoLugar("esquerdo", 0.4f, 0, Encoders::esquerdo);
        testarMotorNoLugar("direito", 0, 0.4f, Encoders::direito);
        Encoders::zerar();
    }
    bool ok = LogErros::fatais() == 0;
    console.printf(">>> calibracao %s: E=%u F=%u D=%u mm, bateria %.2f V (%u%%), PWM max %.0f%%\n",
                   ok ? "OK" : "FALHOU", l.distancia_mm[LADO_ESQUERDO], l.distancia_mm[LADO_FRONTAL],
                   l.distancia_mm[LADO_DIREITO], bateria.tensao(), bateria.percentual(),
                   Motores::pwmMaximo() * 100);
    return ok;
}

// ---------- corrida ----------

void iniciarCorrida()
{
    if (emCorrida)
        return;
    if (!calibrar(false))
    {
        console.println(">>> corrida NAO iniciada: calibracao falhou (comando l mostra o log)");
        return;
    }
    // Contador de corridas do robô, guardado na flash (sobrevive ao reset).
    uint32_t numero = preferencias.getUInt("corrida", 0) + 1;
    preferencias.putUInt("corrida", numero);
    inicioCorrida = millis();
    bateria.iniciarConsumo();
    TelemetriaWifi::novaCorrida(numero, inicioCorrida);
    TelemetriaWifi::publicar(criarEventoInicio(numero, 0, 0, tensaoContrato()));
    emCorrida = true;
    ultimaTelemetria = millis() - PERIODO_TELEMETRIA_MS; // primeira tel já no próximo laço
    console.printf("\n>>> corrida %u iniciada\n", numero);
}

void encerrarCorrida(bool concluida, MotivoAbandono motivo = MOTIVO_MANUAL)
{
    if (!emCorrida)
        return;
    Motores::parar();
    uint32_t c = TelemetriaWifi::corridaAtual();
    uint32_t t = millis() - inicioCorrida;
    float consumo = bateria.consumoMedio();
    MensagemTelemetria fim = concluida ? criarEventoFimConcluida(c, 0, t, t / 1000.0f, consumo)
                                       : criarEventoFimAbandonada(c, 0, t, motivo, consumo);
    TelemetriaWifi::publicar(fim);
    TelemetriaWifi::encerrarCorrida();
    emCorrida = false;
    console.printf("\n>>> corrida %u encerrada (%s)\n", c, concluida ? "concluida" : "abandonada");
}

void enviarTelemetria()
{
    // Posição (0,0) por enquanto: a pose vem da navegação quando o robô andar.
    MensagemTelemetria tel = criarTelemetria(
        0, 0, millis() - inicioCorrida, 0, 0, letra(sensores.orientacao()), sensores.mascaraParedes(),
        fabsf(Encoders::velocidadeMs()), tensaoContrato(), Navegacao::estado().valorFlood, "parado");
    TelemetriaWifi::publicar(tel);
}

// ---------- bateria (#216): regras do contrato ----------

void verificarBateria(uint32_t agora)
{
    const LeituraSensores &l = sensores.leitura();
    EventoBateria evento = bateria.atualizar(agora, l.inaOk, l.tensao_v, l.corrente_ma);
    Motores::informarTensaoBateria(bateria.presente() ? bateria.tensao() : 0);
    switch (evento)
    {
    case BATERIA_AVISO:
        LogErros::registrar(false, "bateria critica: %.2f V", bateria.tensao());
        if (emCorrida)
            TelemetriaWifi::publicar(criarEventoBateriaCritica(TelemetriaWifi::corridaAtual(), 0,
                                                               agora - inicioCorrida, tensaoContrato()));
        break;
    case BATERIA_PARAR:
        Motores::parar();
        LogErros::registrar(true, "bateria abaixo de %.1f V: motores parados", MonitorBateria::PARADA_V);
        encerrarCorrida(false, MOTIVO_BATERIA_CRITICA);
        break;
    case BATERIA_FALHA_SENSOR:
        Motores::parar();
        LogErros::registrar(true, "INA226 parou de responder: motores parados");
        encerrarCorrida(false, MOTIVO_FALHA_SENSOR_BATERIA);
        break;
    case BATERIA_OK:
        break;
    }
}

// ---------- teste dos motores (#275, rodas no ar) ----------

// Espera ms milissegundos mantendo motores, encoders e sensores em dia.
void esperarAtualizando(uint32_t ms)
{
    uint32_t t0 = millis();
    while (millis() - t0 < ms)
    {
        sensores.atualizar();
        Motores::atualizar(millis());
        Encoders::atualizar(millis());
        delay(5);
    }
}

void testarMotores()
{
    if (!bateria.presente())
    {
        console.println(">>> teste dos motores: sem bateria, motores bloqueados");
        return;
    }
    console.printf("\n>>> TESTE DOS MOTORES (rodas no ar!) bateria %.2f V, PWM max %.0f%% = %.1f V no motor\n",
                   bateria.tensao(), Motores::pwmMaximo() * 100, Motores::TENSAO_MAX_MOTOR_V);
    console.println("motor     pot   PWM%   pulsos/s   RPM roda   corrente mA");
    const float potencias[] = {0.3f, 0.6f, 1.0f, -0.3f, -0.6f, -1.0f};
    for (int m = 0; m < 2; m++)
        for (float p : potencias)
        {
            Motores::definir(m == 0 ? p : 0, m == 1 ? p : 0);
            esperarAtualizando(1200); // 0,5 s de rampa + medida estável
            float pps = m == 0 ? Encoders::pulsosPorSegundoEsquerda() : Encoders::pulsosPorSegundoDireita();
            float pwm = m == 0 ? Motores::pwmEsquerdo() : Motores::pwmDireito();
            console.printf("%-8s %5.1f  %5.1f  %9.0f  %9.1f  %10.0f\n", m == 0 ? "esquerdo" : "direito", p,
                           fabsf(pwm) * 100, pps, pps / Encoders::PULSOS_POR_VOLTA * 60, sensores.leitura().corrente_ma);
            if (!bateria.presente())
                break;
        }
    Motores::parar();
    console.println(">>> teste dos motores concluido");
}

// ---------- console ----------

void mostrarStatus()
{
    const LeituraSensores &l = sensores.leitura();
    EstatisticasTelemetria e = TelemetriaWifi::estatisticas();
    console.printf("[%6lus] wifi=%d srv=%d corrida=%u env=%u ack=%u pend=%u queda=%u rtt=%u/%u/%u | "
                   "E=%4u F=%4u D=%4u mm paredes=%2u yaw=%6.1f %c | heap=%u\n",
                   millis() / 1000, e.wifi, e.servidor, TelemetriaWifi::corridaAtual(), e.enviados, e.acks,
                   e.pendentes, e.quedas, e.rttUltimo, e.rttMedio, e.rttMax, l.distancia_mm[LADO_ESQUERDO],
                   l.distancia_mm[LADO_FRONTAL], l.distancia_mm[LADO_DIREITO], sensores.mascaraParedes(),
                   l.yawGraus, letra(sensores.orientacao()), ESP.getFreeHeap());
    float autonomia = bateria.autonomiaMinutos();
    console.printf("         bateria: %s %.2fV %u%% %.0fmA autonomia=%s%.0fmin %s | motores: %s PWM E=%.0f%% D=%.0f%% "
                   "(max %.0f%%) | encoders E=%ld D=%ld v=%.3fm/s\n",
                   bateria.presente() ? "sim" : "NAO (USB)", bateria.tensao(), bateria.percentual(),
                   bateria.corrente(), autonomia < 0 ? "-" : "", autonomia < 0 ? 0 : autonomia,
                   bateria.critica() ? "CRITICA" : "", Motores::habilitado() ? "ligados" : "parados",
                   Motores::pwmEsquerdo() * 100, Motores::pwmDireito() * 100, Motores::pwmMaximo() * 100,
                   (long)Encoders::esquerdo(), (long)Encoders::direito(), Encoders::velocidadeMs());
    Navegacao::Estado n = Navegacao::estado();
    console.printf("         navegacao: nucleo=%u flood=%d ciclos=%u tempo=%u/%u us corridas simuladas ok=%u erradas=%u\n",
                   n.nucleo, n.valorFlood, n.ciclos, n.tempoMedioUs, n.tempoMaxUs, n.corridasOk,
                   n.corridasErradas);
}

void tratarComando(char c)
{
    switch (c)
    {
    case 'i': iniciarCorrida(); break;
    case 'f': encerrarCorrida(true); break;
    case 'a': encerrarCorrida(false); break;
    case 'c': calibrar(true); break;
    case 'l': LogErros::mostrar(console); break;
    case 'm': testarMotores(); break;
    case 'e':
        console.printf(">>> encoders: E=%ld D=%ld pulsos (zerados agora)\n", (long)Encoders::esquerdo(),
                       (long)Encoders::direito());
        Encoders::zerar();
        break;
    case 'p': Motores::parar(); console.println(">>> motores parados"); break;
    case 'z': sensores.zerarYaw(); console.println(">>> yaw zerado"); break;
    case 's': mostrarStatus(); break;
    case 'r': TelemetriaWifi::zerarRtt(); console.println(">>> RTT zerado"); break;
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
        Motores::parar();
        if (emCorrida)
            encerrarCorrida(false);
        console.println("\n>>> OTA: recebendo firmware novo");
    });
    ArduinoOTA.onEnd([]() { console.println("\n>>> OTA: concluido, reiniciando"); });
    ArduinoOTA.onError([](ota_error_t erro) { console.printf("\n>>> OTA: erro %u\n", erro); });
    ArduinoOTA.begin();
    otaIniciado = true;
    console.iniciarRede();
    console.printf(">>> OTA e console pelo Wi-Fi prontos em %s (porta 23)\n", WiFi.localIP().toString().c_str());
}
} // namespace

void setup()
{
    Motores::iniciar(); // primeiro: driver em sleep, motores soltos
    pinMode(PINO_BOTAO_BOOT, INPUT_PULLUP);
    pinMode(PINO_LED, OUTPUT);
    Serial.begin(115200);
    delay(300);
    console.printf("\n=== Micromouse iniciado (firmware de %s %s) ===\n", __DATE__, __TIME__);
    // O buffer da telemetria (~100 KB) é reservado antes de tudo.
    uint32_t maiorBloco = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    bool memoria = TelemetriaWifi::reservarMemoria();
    console.printf("Buffer de telemetria: %s (maior bloco livre antes: %u B)\n",
                   memoria ? "reservado" : "SEM MEMORIA", maiorBloco);

    preferencias.begin("micromouse", false);
    uint8_t ok = sensores.iniciar();
    Encoders::iniciar();
    console.printf("Sensores: %u de 5 responderam\n", ok);

    // Algumas leituras da bateria antes de calibrar (média móvel).
    for (int i = 0; i < 10; i++)
    {
        sensores.atualizar();
        verificarBateria(millis());
        delay(PERIODO_BATERIA_MS / 2);
    }
    calibrar(true); // calibração ao ligar (RF17-S)

    Navegacao::iniciar(4, 4); // tarefa no núcleo 1 (labirinto 4x4 da bancada)
    wifiAtivo = TelemetriaWifi::iniciar(WIFI_SSID, WIFI_SENHA, SERVIDOR_HOST, SERVIDOR_PORTA);
    if (wifiAtivo)
        console.printf("Wi-Fi: conectando em \"%s\", servidor ws://%s:%u/ws/telemetria\n", WIFI_SSID,
                       SERVIDOR_HOST, SERVIDOR_PORTA);
    else
        console.println("Wi-Fi: desligado (falta include/credenciais.h)");
    console.println("Comandos: i f a c l s m e p z r (veja o topo do main.cpp)");
}

void loop()
{
    uint32_t agora = millis();
    sensores.atualizar();
    Motores::atualizar(agora);
    Encoders::atualizar(agora);
    const LeituraSensores &lido = sensores.leitura();
    Navegacao::atualizarSensores(lido.parede[LADO_FRONTAL], lido.parede[LADO_ESQUERDO], lido.parede[LADO_DIREITO],
                                 sensores.orientacao());

    if (agora - ultimaBateria >= PERIODO_BATERIA_MS)
    {
        ultimaBateria = agora;
        verificarBateria(agora);
    }
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

    for (int c = console.ler(); c >= 0; c = console.ler())
        tratarComando((char)c);
    verificarBotao();

    if (wifiAtivo && WiFi.status() == WL_CONNECTED)
    {
        if (!otaIniciado)
            iniciarOta();
        ArduinoOTA.handle();
        console.atender();
    }
    // O loop roda no núcleo 0 (defeito no núcleo 1), onde o watchdog vigia a
    // tarefa ociosa: sem ceder a vez, um laço rápido derruba a ESP32.
    vTaskDelay(1);
}
