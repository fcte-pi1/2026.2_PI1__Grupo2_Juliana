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
#include <Wire.h>
#include <esp_system.h>
#include <stdarg.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include "console.h"
#include "filtro_distancia.h"
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

// ---------- autoteste de todas as peças (comando T) ----------

bool inaLer(uint8_t reg, int32_t &v)
{
    Wire.beginTransmission(0x40);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom((uint8_t)0x40, (uint8_t)2) != 2)
        return false;
    v = (int16_t)((Wire.read() << 8) | Wire.read());
    return true;
}

bool inaEscrever(uint8_t reg, uint16_t v)
{
    Wire.beginTransmission(0x40);
    Wire.write(reg);
    Wire.write(v >> 8);
    Wire.write(v & 0xFF);
    return Wire.endTransmission() == 0;
}

struct Resultado
{
    int ok = 0, falhas = 0;
    void item(bool passou, const char *peca, const char *formato, ...)
    {
        char texto[120];
        va_list args;
        va_start(args, formato);
        vsnprintf(texto, sizeof(texto), formato, args);
        va_end(args);
        console.printf("  [%s] %-22s %s\n", passou ? " OK  " : "FALHA", peca, texto);
        passou ? ok++ : falhas++;
    }
};

void autoTeste()
{
    Motores::parar();
    Resultado r;
    console.println("\n================ AUTOTESTE DA PLACA (rodas no ar!) ================");

    // ESP32
    const char *motivos[] = {"?", "ligou", "externo", "software", "PANICO", "watchdog int", "watchdog tarefa",
                             "watchdog", "sono", "QUEDA DE ENERGIA", "SDIO"};
    esp_reset_reason_t motivo = esp_reset_reason();
    r.item(motivo != ESP_RST_BROWNOUT && motivo != ESP_RST_PANIC, "ESP32: ultimo reset", "%s",
           motivo < 11 ? motivos[motivo] : "?");
    r.item(ESP.getFreeHeap() > 30000, "ESP32: memoria", "%u B livres", ESP.getFreeHeap());
    Navegacao::Estado n = Navegacao::estado();
    r.item(n.nucleo == 1 && n.corridasErradas == 0, "ESP32: nucleo 1", "navegacao no nucleo %u, %u corridas ok",
           n.nucleo, n.corridasOk);
    r.item(WiFi.status() == WL_CONNECTED, "Wi-Fi", "sinal %d dBm", WiFi.RSSI());

    // I2C
    const uint8_t enderecos[] = {0x30, 0x31, 0x32, 0x4A, 0x40};
    const char *nomesI2c[] = {"VL53L0X esq (0x30)", "VL53L0X front (0x31)", "VL53L0X dir (0x32)",
                              "BNO085 (0x4A)", "INA226 (0x40)"};
    for (int i = 0; i < 5; i++)
    {
        Wire.beginTransmission(enderecos[i]);
        r.item(Wire.endTransmission() == 0, nomesI2c[i], "responde no I2C");
    }

    // VL53L0X: 2 s parado
    uint16_t menor[3] = {65535, 65535, 65535}, maior[3] = {0, 0, 0};
    float yaw0 = sensores.leitura().yawGraus, yawMax = 0;
    for (uint32_t t0 = millis(); millis() - t0 < 2000;)
    {
        sensores.atualizar();
        const LeituraSensores &l = sensores.leitura();
        for (int i = 0; i < 3; i++)
        {
            menor[i] = l.distancia_mm[i] < menor[i] ? l.distancia_mm[i] : menor[i];
            maior[i] = l.distancia_mm[i] > maior[i] ? l.distancia_mm[i] : maior[i];
        }
        float d = fabsf(l.yawGraus - yaw0);
        yawMax = d > yawMax ? d : yawMax;
        delay(20);
    }
    const char *lados[] = {"VL53L0X esquerdo", "VL53L0X frontal", "VL53L0X direito"};
    for (int i = 0; i < 3; i++)
    {
        bool semAlvo = menor[i] >= DIST_SEM_ALVO;
        r.item(!sensores.leitura().defeito[i] && (semAlvo || maior[i] - menor[i] < 30), lados[i],
               semAlvo ? "sem alvo no alcance (ok)" : "%u a %u mm parado (ruido %u mm)", menor[i], maior[i],
               maior[i] - menor[i]);
    }

    // BNO085
    r.item(sensores.leitura().imuOk && yawMax < 2, "BNO085 (giroscopio)", "yaw variou %.2f graus em 2 s parado",
           yawMax);

    // INA226
    int32_t fabricante = 0, chip = 0, barramento = 0, shunt = 0;
    inaLer(0xFE, fabricante);
    inaLer(0xFF, chip);
    inaLer(0x02, barramento);
    inaLer(0x01, shunt);
    r.item((uint16_t)fabricante == 0x5449, "INA226: identificacao", "fabricante 0x%04X chip 0x%04X",
           (uint16_t)fabricante, (uint16_t)chip);
    r.item(bateria.presente(), "Bateria", "%.2f V (%u%%)", (uint16_t)barramento * 0.00125f, bateria.percentual());
    r.item(abs((int)shunt) > 20, "INA226: corrente", "%.1f mA com a placa ligada (esperado ~100-200 mA)",
           (int16_t)shunt * 0.0025f / 0.1f);

    // Encoders parados
    Encoders::zerar();
    delay(1000);
    r.item(Encoders::esquerdo() == 0 && Encoders::direito() == 0, "Encoders parados",
           "E=%ld D=%ld pulsos em 1 s sem mexer", (long)Encoders::esquerdo(), (long)Encoders::direito());

    // Pinos do DRV8833: curto entre entradas
    const int pinos[] = {PINO_MOTOR_E_IN1, PINO_MOTOR_E_IN2, PINO_MOTOR_D_IN1, PINO_MOTOR_D_IN2};
    int curtos = 0;
    for (int p : pinos)
        ledcDetachPin(p);
    for (int i = 0; i < 4; i++)
    {
        for (int p : pinos)
            pinMode(p, INPUT_PULLDOWN);
        pinMode(pinos[i], OUTPUT);
        digitalWrite(pinos[i], HIGH);
        delay(5);
        for (int j = 0; j < 4; j++)
            if (j != i && digitalRead(pinos[j]) == HIGH)
                curtos++;
        digitalWrite(pinos[i], LOW);
    }
    Motores::iniciar();
    r.item(curtos == 0, "DRV8833: pinos IN1-IN4", curtos ? "%d curtos entre entradas" : "sem curto", curtos);

    // Motores a 5 V: rotação, corrente no INA226 e queda da bateria
    if (bateria.presente())
    {
        inaEscrever(0x00, 0x4927); // média de 128, para ver microvolts
        delay(300);
        int32_t vRepouso = 0;
        inaLer(0x02, vRepouso);
        for (int m = 0; m < 2; m++)
        {
            int32_t p0 = m == 0 ? Encoders::esquerdo() : Encoders::direito();
            int32_t shuntMax = 0, vMin = vRepouso, v = 0;
            Motores::definir(m == 0 ? 1 : 0, m == 1 ? 1 : 0);
            for (uint32_t t0 = millis(); millis() - t0 < 2000;)
            {
                Motores::atualizar(millis());
                if (inaLer(0x01, v) && abs((int)v) > abs((int)shuntMax))
                    shuntMax = v;
                if (inaLer(0x02, v) && (uint16_t)v < (uint16_t)vMin)
                    vMin = v;
                delay(30);
            }
            Motores::parar();
            int32_t pulsos = (m == 0 ? Encoders::esquerdo() : Encoders::direito()) - p0;
            float rpm = fabsf((float)pulsos) / Encoders::PULSOS_POR_VOLTA * 60 / 1.5f;
            float queda = ((uint16_t)vRepouso - (uint16_t)vMin) * 1.25f;
            r.item(rpm > 20, m == 0 ? "Motor esquerdo (5 V)" : "Motor direito (5 V)",
                   "%.0f RPM, corrente %.0f mA, bateria caiu %.0f mV", rpm, (int16_t)shuntMax * 0.025f, queda);
            delay(400);
        }
        inaEscrever(0x00, 0x4127);
    }
    else
        r.item(false, "Motores", "sem bateria: nao testados");

    // Servidor
    EstatisticasTelemetria e = TelemetriaWifi::estatisticas();
    r.item(e.servidor, "Servidor (telemetria)", e.servidor ? "conectado, RTT medio %u ms" : "desconectado (backend ligado?)",
           e.rttMedio);

    console.printf("================ %d OK, %d FALHA(S) ================\n", r.ok, r.falhas);
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
    case 'h':
    case 'j':
    {
        // Segura um motor no máximo (5 V) por 10 s, para medir com o multímetro.
        if (!bateria.presente())
        {
            console.println(">>> sem bateria, motores bloqueados");
            break;
        }
        bool esq = c == 'h';
        console.printf(">>> motor %s no maximo por 10 s (PWM %.0f%% de %.2f V)\n", esq ? "ESQUERDO" : "DIREITO",
                       Motores::pwmMaximo() * 100, bateria.tensao());
        Motores::definir(esq ? 1 : 0, esq ? 0 : 1);
        for (int s = 0; s < 10; s++)
        {
            esperarAtualizando(1000);
            console.printf("    %2ds: pulsos/s E=%.0f D=%.0f corrente=%.0f mA\n", s + 1,
                           Encoders::pulsosPorSegundoEsquerda(), Encoders::pulsosPorSegundoDireita(),
                           sensores.leitura().corrente_ma);
        }
        Motores::parar();
        console.println(">>> motor parado");
        break;
    }
    case 'd':
    {
        // Diagnóstico sem multímetro: cada motor, sentido e modo de PWM a 5 V por
        // 2 s. Pulsos = a roda girou; queda da bateria = o motor puxou corrente.
        if (!bateria.presente())
        {
            console.println(">>> sem bateria, motores bloqueados");
            break;
        }
        console.println("\n>>> DIAGNOSTICO DOS MOTORES (rodas no ar!) a 5 V, 2 s cada");
        console.println("motor     sentido  PWM     pulsos   queda bateria mV   corrente mA");
        for (int lento = 0; lento < 2; lento++)
            for (int m = 0; m < 2; m++)
                for (int sentido = 1; sentido >= -1; sentido -= 2)
                {
                    Motores::usarDecaimentoLento(lento);
                    esperarAtualizando(300);
                    float antes = sensores.leitura().tensao_v;
                    int32_t p0 = m == 0 ? Encoders::esquerdo() : Encoders::direito();
                    Motores::definir(m == 0 ? sentido : 0, m == 1 ? sentido : 0);
                    float menor = antes, maiorCorrente = 0;
                    uint32_t t0 = millis();
                    while (millis() - t0 < 2000)
                    {
                        esperarAtualizando(20);
                        const LeituraSensores &l = sensores.leitura();
                        if (l.tensao_v < menor)
                            menor = l.tensao_v;
                        if (fabsf(l.corrente_ma) > maiorCorrente)
                            maiorCorrente = fabsf(l.corrente_ma);
                    }
                    int32_t pulsos = (m == 0 ? Encoders::esquerdo() : Encoders::direito()) - p0;
                    Motores::parar();
                    console.printf("%-8s  %-7s  %-6s %7ld   %16.0f   %11.0f\n", m == 0 ? "esquerdo" : "direito",
                                   sentido > 0 ? "frente" : "tras", lento ? "lento" : "rapido", (long)pulsos,
                                   (antes - menor) * 1000, maiorCorrente);
                }
        Motores::usarDecaimentoLento(false);
        console.println(">>> diagnostico concluido");
        break;
    }
    case 'v':
    {
        // Varredura de 5 a 6 V (máximo do N20), supondo o DRV8833 com a tensão
        // da bateria: no pior caso o motor recebe 6 V. Para no primeiro degrau
        // que girar e volta o teto para 5 V.
        if (!bateria.presente())
        {
            console.println(">>> sem bateria, motores bloqueados");
            break;
        }
        console.printf("\n>>> VARREDURA 5 a 6 V (rodas no ar!) bateria %.2f V\n", bateria.tensao());
        const float degraus[] = {5.0f, 5.3f, 5.6f, 6.0f};
        for (int m = 0; m < 2; m++)
        {
            for (float v : degraus)
            {
                Motores::definirTensaoMaxima(v);
                int32_t p0 = m == 0 ? Encoders::esquerdo() : Encoders::direito();
                Motores::definir(m == 0 ? 1 : 0, m == 1 ? 1 : 0);
                esperarAtualizando(1500);
                int32_t pulsos = (m == 0 ? Encoders::esquerdo() : Encoders::direito()) - p0;
                console.printf("%-8s %.1f V (PWM %.0f%%): %ld pulsos\n", m == 0 ? "esquerdo" : "direito", v,
                               Motores::pwmMaximo() * 100, (long)pulsos);
                if (labs(pulsos) > 20)
                    break;
            }
            Motores::parar();
            esperarAtualizando(300);
        }
        Motores::definirTensaoMaxima(Motores::TENSAO_MAX_MOTOR_V);
        console.printf(">>> varredura concluida, teto de volta em %.1f V\n", Motores::tensaoMaxima());
        break;
    }
    case 'q':
    {
        // Frequência do PWM a 5 V: mesma tensão média, rendimento diferente.
        if (!bateria.presente())
        {
            console.println(">>> sem bateria, motores bloqueados");
            break;
        }
        console.printf("\n>>> FREQUENCIA DO PWM a 5 V (rodas no ar!) bateria %.2f V\n", bateria.tensao());
        const uint32_t freqs[] = {20000, 5000, 1000, 200};
        for (int m = 0; m < 2; m++)
            for (uint32_t f : freqs)
            {
                Motores::definirFrequencia(f);
                int32_t p0 = m == 0 ? Encoders::esquerdo() : Encoders::direito();
                Motores::definir(m == 0 ? 1 : 0, m == 1 ? 1 : 0);
                esperarAtualizando(1500);
                int32_t pulsos = (m == 0 ? Encoders::esquerdo() : Encoders::direito()) - p0;
                Motores::parar();
                console.printf("%-8s %5u Hz: %ld pulsos\n", m == 0 ? "esquerdo" : "direito", f, (long)pulsos);
                esperarAtualizando(300);
            }
        Motores::definirFrequencia(20000);
        console.println(">>> teste de frequencia concluido (de volta a 20 kHz)");
        break;
    }
    case 'k':
    {
        // Os dois lados do driver juntos, nas 4 combinações de sentido, a 5 V.
        // Um motor ligado entre saídas dos dois lados (OUT1-OUT3, por ex.) só
        // gira quando os dois lados são acionados com polaridades diferentes.
        if (!bateria.presente())
        {
            console.println(">>> sem bateria, motores bloqueados");
            break;
        }
        console.println("\n>>> DOIS LADOS JUNTOS a 5 V (rodas no ar!), 2 s cada");
        console.println("lado E (D25/D26)  lado D (D27/D33)   pulsos E   pulsos D");
        const int combos[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
        for (auto &c : combos)
        {
            int32_t e0 = Encoders::esquerdo(), d0 = Encoders::direito();
            Motores::definir(c[0], c[1]);
            esperarAtualizando(2000);
            Motores::parar();
            console.printf("    %-14s   %-14s   %8ld   %8ld\n", c[0] > 0 ? "+" : "-", c[1] > 0 ? "+" : "-",
                           (long)(Encoders::esquerdo() - e0), (long)(Encoders::direito() - d0));
            esperarAtualizando(400);
        }
        console.println(">>> teste concluido");
        break;
    }
    case 'g':
    {
        // PWM subindo em degraus com TRAVA POR VELOCIDADE: a rotação da roda é
        // proporcional à tensão no motor. Corta assim que passar de LIMITE_RPM
        // (abaixo do que o motor faz a 5 V), então mesmo com o DRV8833 na
        // bateria inteira o motor só passa de 5 V por uma fração de segundo.
        if (!bateria.presente())
        {
            console.println(">>> sem bateria, motores bloqueados");
            break;
        }
        const float LIMITE_RPM = 40;
        const float limitePps = LIMITE_RPM / 60.0f * Encoders::PULSOS_POR_VOLTA;
        console.printf("\n>>> PWM EM DEGRAUS COM TRAVA (rodas no ar!) bateria %.2f V, corta acima de %.0f RPM\n",
                       bateria.tensao(), LIMITE_RPM);
        console.println("motor     PWM%   equivale a (se VCC = bateria)   RPM maximo no degrau");
        for (int m = 0; m < 2; m++)
        {
            bool cortou = false;
            for (int passo = 0; passo <= 8 && !cortou; passo++)
            {
                float pwm = 0.62f + passo * 0.05f;
                if (pwm > 1)
                    pwm = 1;
                Motores::pwmBruto(m == 0 ? pwm : 0, m == 1 ? pwm : 0);
                float maxRpm = 0;
                int32_t anterior = m == 0 ? Encoders::esquerdo() : Encoders::direito();
                uint32_t t0 = millis(), tJanela = t0;
                while (millis() - t0 < 500)
                {
                    delay(20);
                    sensores.atualizar();
                    uint32_t agora = millis();
                    if (agora - tJanela >= 60)
                    {
                        int32_t atual = m == 0 ? Encoders::esquerdo() : Encoders::direito();
                        float pps = fabsf((float)(atual - anterior)) * 1000.0f / (agora - tJanela);
                        float rpm = pps / Encoders::PULSOS_POR_VOLTA * 60;
                        if (rpm > maxRpm)
                            maxRpm = rpm;
                        anterior = atual;
                        tJanela = agora;
                        if (pps > limitePps)
                        {
                            Motores::parar();
                            cortou = true;
                            break;
                        }
                    }
                }
                console.printf("%-8s %5.0f   %5.1f V                         %6.1f%s\n", m == 0 ? "esquerdo" : "direito",
                               pwm * 100, pwm * bateria.tensao(), maxRpm, cortou ? "  <- CORTOU (trava)" : "");
                if (pwm >= 1)
                    break;
            }
            Motores::parar();
            delay(400);
        }
        console.println(">>> teste concluido, de volta ao limite de 5 V");
        break;
    }
    case 'w':
    {
        // Monitor ao vivo para ajustar o regulador: motor esquerdo no limite
        // normal (5 V) por 90 s, rotação a cada 0,5 s. 'p' interrompe.
        if (!bateria.presente())
        {
            console.println(">>> sem bateria, motores bloqueados");
            break;
        }
        console.println("\n>>> MONITOR AO VIVO: motor esquerdo no limite de 5 V por 90 s ('p' para)");
        Motores::definir(1, 0);
        for (int i = 0; i < 180; i++)
        {
            esperarAtualizando(500);
            float rpm = fabsf(Encoders::pulsosPorSegundoEsquerda()) / Encoders::PULSOS_POR_VOLTA * 60;
            console.printf("    %5.1fs  roda esquerda %5.1f RPM  bateria %.2f V\n", (i + 1) / 2.0f, rpm,
                           sensores.leitura().tensao_v);
            int c = console.ler();
            if (c == 'p')
                break;
        }
        Motores::parar();
        console.println(">>> monitor encerrado, motor parado");
        break;
    }
    case 'n':
    {
        // O INA226 mede IN+ - IN- (as pontas do shunt) em passos de 2,5 uV.
        // Com os motores zumbindo: se o driver é alimentado depois do shunt e o
        // shunt está ruim, a diferença dispara; se fica em zero, a corrente dos
        // motores não passa pelo INA226.
        auto escrever = [](uint8_t reg, uint16_t v) {
            Wire.beginTransmission(0x40);
            Wire.write(reg);
            Wire.write(v >> 8);
            Wire.write(v & 0xFF);
            return Wire.endTransmission() == 0;
        };
        auto ler = [](uint8_t reg, int32_t &v) {
            Wire.beginTransmission(0x40);
            Wire.write(reg);
            if (Wire.endTransmission(false) != 0 || Wire.requestFrom((uint8_t)0x40, (uint8_t)2) != 2)
                return false;
            v = (int16_t)((Wire.read() << 8) | Wire.read());
            return true;
        };
        auto medir = [&](const char *rotulo, uint32_t ms) {
            int32_t menor = 32767, maior = -32768, bus = 0, v = 0;
            uint32_t t0 = millis();
            while (millis() - t0 < ms)
            {
                Motores::atualizar(millis());
                if (ler(0x01, v))
                {
                    menor = v < menor ? v : menor;
                    maior = v > maior ? v : maior;
                }
                delay(30);
            }
            ler(0x02, bus);
            console.printf("    %-28s shunt %+6ld a %+6ld (%+.3f a %+.3f mV)  barramento %.3f V\n", rotulo,
                           (long)menor, (long)maior, menor * 0.0025f, maior * 0.0025f, (uint16_t)bus * 0.00125f);
        };
        int32_t config = 0;
        ler(0x00, config);
        console.printf("\n>>> INA226: config 0x%04X, media de 128 leituras\n", (uint16_t)config);
        escrever(0x00, 0x4927); // AVG=128, conversão de 1,1 ms, contínuo
        delay(300);
        medir("motores parados", 1500);
        if (bateria.presente())
        {
            Motores::definir(1, 1);
            medir("dois motores zumbindo (5 V)", 5000);
            Motores::parar();
        }
        medir("motores parados de novo", 1500);
        escrever(0x00, 0x4127); // padrão
        console.println(">>> INA226 de volta ao padrao");
        break;
    }
    case 'T': autoTeste(); break;
    case 'E':
    {
        // Liga/desliga só o pino SLEEP (EEP) do DRV8833, sem motor: mostra se
        // o LED do módulo acompanha o SLEEP ou a alimentação.
        static bool acordado = false;
        Motores::parar(); // motores parados de qualquer jeito
        acordado = !acordado;
        digitalWrite(PINO_DRV_SLEEP, acordado ? HIGH : LOW);
        console.printf(">>> DRV8833 %s (pino D32 = %d), motores parados\n",
                       acordado ? "ACORDADO" : "dormindo", digitalRead(PINO_DRV_SLEEP));
        break;
    }
    case 'I':
    {
        // Ligação ESP -> entradas do DRV8833. O DRV8833 puxa cada IN para 0 V
        // (~100 kΩ). Com o pull-up interno da ESP (~45 kΩ), o pino lê 0 se o
        // fio estiver ligado ao driver e 1 se estiver aberto (solda solta).
        Motores::parar();
        const int pinos[] = {PINO_MOTOR_E_IN1, PINO_MOTOR_E_IN2, PINO_MOTOR_D_IN1, PINO_MOTOR_D_IN2};
        const char *nomes[] = {"D25 -> IN1 (esq. frente)", "D26 -> IN2 (esq. tras)", "D27 -> IN3 (dir. frente)",
                               "D33 -> IN4 (dir. tras)"};
        console.println("\n>>> LIGACAO DOS PINOS ATE O DRV8833 (pull-up da ESP x pull-down do driver)");
        for (int p : pinos)
            ledcDetachPin(p);
        for (int i = 0; i < 4; i++)
        {
            pinMode(pinos[i], INPUT_PULLUP);
            delay(5);
            int leituras = 0;
            for (int k = 0; k < 20; k++)
                leituras += digitalRead(pinos[i]);
            pinMode(pinos[i], INPUT_PULLDOWN);
            console.printf("    %-26s le %s (%d/20 em 1)\n", nomes[i],
                           leituras == 0 ? "0: ligado ao driver" : (leituras == 20 ? "1: ABERTO?" : "instavel: mau contato?"),
                           leituras);
        }
        Motores::iniciar();
        // No USB: motor esquerdo para trás (IN2) e para frente (IN1).
        if (!bateria.presente() && sensores.leitura().tensao_v >= 3.5f)
        {
            Motores::informarTensaoBateria(8.0f); // libera só neste teste
            const float sentidos[] = {-1, 1};
            for (float s : sentidos)
            {
                int32_t p0 = Encoders::esquerdo();
                Motores::pwmBruto(s, 0);
                delay(1500);
                Motores::parar();
                Motores::informarTensaoBateria(8.0f);
                console.printf("    motor esquerdo %-14s %6ld pulsos\n", s < 0 ? "tras (IN2):" : "frente (IN1):",
                               (long)(Encoders::esquerdo() - p0));
                delay(300);
            }
            Motores::informarTensaoBateria(0);
        }
        console.println(">>> fim");
        break;
    }
    case 'W':
    {
        // SÓ NO USB: motores ligados por 90 s, alternando a cada 2 s, para
        // escutar e procurar mau contato apertando de leve a placa. 'p' para.
        if (bateria.presente() || sensores.leitura().tensao_v < 3.5f)
        {
            console.println(">>> teste 'W' e so no USB, sem bateria");
            break;
        }
        console.println("\n>>> MOTORES NO USB POR 90 s, alternando a cada 2 s ('p' para)");
        Motores::informarTensaoBateria(8.0f); // libera só neste teste
        bool parar = false;
        for (int i = 0; i < 45 && !parar; i++)
        {
            int m = i % 2;
            int32_t p0 = m == 0 ? Encoders::esquerdo() : Encoders::direito();
            float v0 = sensores.leitura().tensao_v, menor = v0;
            Motores::pwmBruto(m == 0 ? 1 : 0, m == 1 ? 1 : 0);
            for (uint32_t t0 = millis(); millis() - t0 < 2000;)
            {
                delay(30);
                sensores.atualizar();
                float v = sensores.leitura().tensao_v;
                menor = v < menor ? v : menor;
                if (console.ler() == 'p')
                {
                    parar = true;
                    break;
                }
            }
            int32_t pulsos = (m == 0 ? Encoders::esquerdo() : Encoders::direito()) - p0;
            console.printf("    %4.0fs %-8s %6ld pulsos (%4.0f RPM)  trilha caiu %3.0f mV%s\n", (i + 1) * 2.0f,
                           m == 0 ? "esquerdo" : "direito", (long)pulsos,
                           fabsf((float)pulsos) / Encoders::PULSOS_POR_VOLTA * 30, (v0 - menor) * 1000,
                           labs(pulsos) > 100 ? "  <-- GIROU" : "");
        }
        Motores::parar();
        Motores::informarTensaoBateria(0);
        console.println(">>> fim do teste W");
        break;
    }
    case 'y':
    {
        // SÓ NO USB: variações de modo e frequência do PWM a 100% (o motor
        // recebe no máximo a tensão do driver, < 5 V), 3 s cada.
        if (bateria.presente() || sensores.leitura().tensao_v < 3.5f)
        {
            console.println(">>> teste 'y' e so no USB, sem bateria");
            break;
        }
        console.println("\n>>> VARIACOES NO USB (100% de PWM, 3 s cada)");
        const uint32_t freqs[] = {20000, 1000, 200};
        Motores::informarTensaoBateria(8.0f); // libera só neste teste
        for (int m = 0; m < 2; m++)
            for (int lento = 0; lento < 2; lento++)
                for (uint32_t f : freqs)
                {
                    Motores::usarDecaimentoLento(lento);
                    Motores::definirFrequencia(f);
                    int32_t p0 = m == 0 ? Encoders::esquerdo() : Encoders::direito();
                    float v0 = sensores.leitura().tensao_v, menor = v0;
                    Motores::pwmBruto(m == 0 ? 1 : 0, m == 1 ? 1 : 0);
                    for (uint32_t t0 = millis(); millis() - t0 < 3000;)
                    {
                        delay(30);
                        sensores.atualizar();
                        float v = sensores.leitura().tensao_v;
                        menor = v < menor ? v : menor;
                    }
                    Motores::parar();
                    int32_t pulsos = (m == 0 ? Encoders::esquerdo() : Encoders::direito()) - p0;
                    console.printf("    %-8s %-6s %5u Hz: %6ld pulsos (%4.0f RPM), trilha caiu %3.0f mV\n",
                                   m == 0 ? "esquerdo" : "direito", lento ? "lento" : "rapido", f, (long)pulsos,
                                   fabsf((float)pulsos) / Encoders::PULSOS_POR_VOLTA * 20, (v0 - menor) * 1000);
                    Motores::informarTensaoBateria(8.0f); // parar() não desliga a liberação
                    delay(300);
                }
        Motores::usarDecaimentoLento(false);
        Motores::definirFrequencia(20000);
        Motores::informarTensaoBateria(0);
        console.println(">>> variacoes concluidas");
        break;
    }
    case 'o':
    {
        // Sinais dos encoders: conta as mudanças de cada fio (A e B) por 8 s
        // enquanto alguém gira a roda. Fio que quase não muda = solto/defeito.
        const int pinos[] = {PINO_ENC_E_A, PINO_ENC_E_B, PINO_ENC_D_A, PINO_ENC_D_B};
        const char *nomes[] = {"esquerdo A (VP)", "esquerdo B (VN)", "direito A (D34)", "direito B (D35)"};
        uint32_t mudancas[4] = {0, 0, 0, 0};
        int anterior[4];
        for (int i = 0; i < 4; i++)
            anterior[i] = digitalRead(pinos[i]);
        console.println("\n>>> SINAIS DOS ENCODERS: gire a roda nos proximos 8 s");
        for (uint32_t t0 = millis(); millis() - t0 < 8000;)
        {
            for (int i = 0; i < 4; i++)
            {
                int v = digitalRead(pinos[i]);
                if (v != anterior[i])
                {
                    mudancas[i]++;
                    anterior[i] = v;
                }
            }
            delayMicroseconds(50);
            if ((millis() - t0) % 1000 == 0)
                vTaskDelay(1); // deixa o watchdog em paz
        }
        for (int i = 0; i < 4; i++)
            console.printf("    %-18s %6u mudancas (agora em %d)\n", nomes[i], mudancas[i], anterior[i]);
        console.println(">>> fim");
        break;
    }
    case 'u':
    {
        // Teste SÓ NO USB (sem bateria): os 5 V do USB voltam pelo MP1584 até a
        // trilha de 7,4 V e chegam ao DRV8833 com ~4-5 V. Libera os motores só
        // aqui, 1,5 s cada, com o PWM no máximo: o motor recebe no máximo a
        // tensão do driver (< 5 V). Mostra se driver e motores funcionam por
        // outro caminho de energia.
        if (bateria.presente())
        {
            console.println(">>> teste 'u' e so no USB: desligue a bateria e ligue a ESP no USB/carregador");
            break;
        }
        float vLinha = sensores.leitura().tensao_v;
        console.printf("\n>>> TESTE NO USB: trilha da bateria em %.2f V (vinda do USB)\n", vLinha);
        if (vLinha < 3.5f)
        {
            console.println(">>> trilha abaixo de 3,5 V: nao ha tensao para testar");
            break;
        }
        Motores::informarTensaoBateria(8.0f); // libera só neste teste (bloqueio volta no próximo laço)
        for (int m = 0; m < 2; m++)
        {
            int32_t p0 = m == 0 ? Encoders::esquerdo() : Encoders::direito();
            float menor = vLinha;
            Motores::pwmBruto(m == 0 ? 1 : 0, m == 1 ? 1 : 0);
            for (uint32_t t0 = millis(); millis() - t0 < 1500;)
            {
                delay(30);
                sensores.atualizar();
                float v = sensores.leitura().tensao_v;
                menor = v < menor ? v : menor;
            }
            Motores::parar();
            int32_t pulsos = (m == 0 ? Encoders::esquerdo() : Encoders::direito()) - p0;
            console.printf("    motor %-8s %6ld pulsos (%.0f RPM)  trilha caiu de %.2f para %.2f V\n",
                           m == 0 ? "esquerdo" : "direito", (long)pulsos,
                           fabsf((float)pulsos) / Encoders::PULSOS_POR_VOLTA * 60 / 1.5f, vLinha, menor);
            delay(500);
        }
        Motores::informarTensaoBateria(0);
        console.println(">>> teste no USB concluido");
        break;
    }
    case 'x':
    {
        // Curto entre os pinos do driver (ponte de solda), sem multímetro: com o
        // DRV8833 dormindo, põe um pino em 1 e lê os outros (com pull-down).
        Motores::parar();
        const int pinos[] = {PINO_MOTOR_E_IN1, PINO_MOTOR_E_IN2, PINO_MOTOR_D_IN1, PINO_MOTOR_D_IN2};
        const char *nomes[] = {"D25 (IN1)", "D26 (IN2)", "D27 (IN3)", "D33 (IN4)"};
        console.println("\n>>> CURTO ENTRE PINOS DO DRIVER (motores desligados)");
        int curtos = 0;
        for (int p : pinos)
            ledcDetachPin(p);
        for (int i = 0; i < 4; i++)
        {
            for (int p : pinos)
                pinMode(p, INPUT_PULLDOWN);
            pinMode(pinos[i], OUTPUT);
            digitalWrite(pinos[i], HIGH);
            delay(5);
            for (int j = 0; j < 4; j++)
                if (j != i && digitalRead(pinos[j]) == HIGH)
                {
                    console.printf("    %s em 1 -> %s tambem em 1: CURTO\n", nomes[i], nomes[j]);
                    curtos++;
                }
            digitalWrite(pinos[i], LOW);
        }
        Motores::iniciar(); // devolve os pinos ao PWM
        console.printf(">>> %s\n", curtos ? "ha curto entre pinos do driver" : "nenhum curto entre os pinos do driver");
        break;
    }
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
