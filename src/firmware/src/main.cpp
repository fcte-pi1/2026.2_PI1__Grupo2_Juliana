// Firmware do Micromouse (ESP32-DEVKIT-V1).
//
// Lê os sensores, monitora a bateria, aciona os motores (limitados a 5 V) e
// envia a telemetria pelo Wi-Fi. Na corrida automática, o modo automático
// (Flood Fill) decide o caminho e o controle de movimento anda célula a célula
// (lib/movimento). Sem corrida, o núcleo 1 recalcula o Flood Fill do robô parado.
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
//   L  calibra os limiares de parede (mede com e sem parede; salva na flash)
//   Q / G / H  derruba o Wi-Fi por 5 s / 60 s / 12 min (enche o buffer)
//
// Corrida automática e movimento (ROBÔ NO CHÃO, bateria ligada):
//   A  corrida automática: explora, volta e faz a corrida rápida (RF13-S)
//      BOOT segurado por 1 s faz o mesmo; durante a corrida, BOOT, p ou a param
//   M  tamanho do labirinto (células à frente x ao lado na largada)
//   B  bordas do canto na largada: atrás e à esquerda / atrás e à direita /
//      à frente e à direita / à frente e à esquerda (robô virado para a borda)
//   F  anda 1 célula      3  anda 3 células       (CT-SE05)
//   <  gira 90° à esquerda   >  90° à direita   U  meia-volta   (CT-SE06)
//   #  gira 360° no próprio eixo (4 x 90° à esquerda): volta para onde estava
//   ( / )  largura do eixo -5 / +5 mm (girou menos que o pedido: aumente)
//   O  liga/desliga o log do controle (CSV a cada 50 ms, CT-SE07)
//   K  parâmetros do controle e referências da calibração das paredes
//   + / -  PWM máximo do movimento (0 a 255, passos de 5; padrão 20)
//   Y  calibra os encoders: anda ~20 cm para a parede da frente e compara os
//      pulsos com a distância medida (pulsos por mm e sentido; salva na flash)
//   R  calibra o giro sem BNO085: de frente para uma parede, gira ~1,5 volta
//      e mede a largura efetiva do eixo pelo sensor da frente (salva na flash)
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <esp_system.h>
#include <stdarg.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include "console.h"
#include "controle_movimento.h"
#include "calibracao_giro.h"
#include "filtro_distancia.h"
#include "labirinto.h"
#include "log_erros.h"
#include "modo_automatico.h"
#include "monitor_bateria.h"
#include "motores.h"
#include "navegacao_nucleo1.h"
#include "pacote.h"
#include "pinos.h"
#include "robo_fisico.h"
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

// ---------- corrida automática (#210) e controle de movimento ----------
ControleMovimento controle;
// Pulsos por mm de cada roda (comando Y). Os encoders desta placa não contam
// igual (bancada, 23/11/2026: esquerda 1,0, direita 1,6): o controle recebe as
// contagens já na escala da média, como se as duas rodas fossem iguais.
float ppmEsquerda = 0, ppmDireita = 0;
Labirinto mapaCorrida;
bool automatico = false;        // corrida com o robô andando sozinho
bool cancelarMovimento = false; // pedido para parar o movimento atual (console, botão, fim da corrida)
bool logMovimento = false;      // CSV do controle no console (CT-SE07)
// Labirinto da corrida, como a equipe vê o robô na largada (labirinto.h).
uint8_t celulasAFrente = 4, celulasAoLado = 4;
LadoBorda borda = BORDA_ESQUERDA;

void servicoDuranteMovimento();

// O hardware para o RoboFisico. ler() faz o que o laço principal faria
// (sensores, bateria, telemetria, console, OTA) enquanto o robô se move.
class HardwareReal : public HardwareRobo
{
public:
    EntradaMovimento ler() override
    {
        sensores.permitirReinicioImu(false);
        servicoDuranteMovimento();
        const LeituraSensores &l = sensores.leitura();
        EntradaMovimento e = {};
        e.agora_ms = millis();
        float media = controle.parametros().pulsosPorMm;
        e.encoderEsquerdo = ppmEsquerda > 0 ? (int32_t)lroundf(Encoders::esquerdo() * media / ppmEsquerda)
                                            : Encoders::esquerdo();
        e.encoderDireito = ppmDireita > 0 ? (int32_t)lroundf(Encoders::direito() * media / ppmDireita)
                                          : Encoders::direito();
        // Só yaw novo: logo depois de um reset o BNO085 diz que está ok, mas o
        // yaw fica parado até a primeira leitura e o robô pareceria não girar.
        e.yawOk = l.imuOk && millis() - l.imuUltimaLeituraMs < 100;
        e.yawGraus = l.yawGraus;
        for (uint8_t i = 0; i < 3; i++)
        {
            e.distancia_mm[i] = l.distancia_mm[i];
            e.parede[i] = l.parede[i];
        }
        return e;
    }

    void aplicar(const SaidaMovimento &s) override
    {
        if (s.frear)
            Motores::frear();
        else
            Motores::definir(s.esquerdo, s.direito);
        ultimaSaida_ = s;
    }

    bool sensoresOk() override
    {
        const LeituraSensores &l = sensores.leitura();
        return !l.defeito[LADO_ESQUERDO] && !l.defeito[LADO_FRONTAL] && !l.defeito[LADO_DIREITO];
    }

    bool abortado() override { return cancelarMovimento || !bateria.presente(); }

    // CT-SE07: uma linha a cada 50 ms com o erro lateral e o sinal de controle.
    void observar(const ControleMovimento &c) override
    {
        static uint32_t ultimo = 0;
        if (!logMovimento || millis() - ultimo < 50)
            return;
        ultimo = millis();
        // + pulsos e potência aplicada de cada roda e a frente: mostra roda travada ou robô encostado.
        const LeituraSensores &l = sensores.leitura();
        console.printf("mov,%lu,%s,%.0f,%.1f,%d,%.1f,%.3f,%ld,%ld,%.2f,%.2f,%u\n", (unsigned long)ultimo,
                       nomeMovimento(c.estado()), c.percorridoMm(), c.erroLateralMm(), c.temReferenciaLateral(), c.rumo(),
                       c.sinalControle(), (long)Encoders::esquerdo(), (long)Encoders::direito(), ultimaSaida_.esquerdo,
                       ultimaSaida_.direito, l.distancia_mm[LADO_FRONTAL]);
    }

private:
    SaidaMovimento ultimaSaida_ = {0, 0, false};
};

HardwareReal hardware;
RoboFisico roboFisico(hardware, controle);
ModoAutomatico modo(roboFisico, mapaCorrida);

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
        LogErros::registrar(false, "BNO085 sem resposta: rumo pelos encoders e paredes");
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
    automatico = false;
    cancelarMovimento = true; // bateria, console ou botão no meio de um movimento: o controle para já
    console.printf("\n>>> corrida %u encerrada (%s)\n", c, concluida ? "concluida" : "abandonada");
}

void enviarTelemetria()
{
    // Na corrida automática, pose, paredes, Flood Fill e estado vêm do modo
    // automático; com o robô parado, da navegação do núcleo 1.
    Navegacao::Estado n = Navegacao::estado();
    if (automatico)
    {
        uint8_t d = modo.floodFill().valorAtual(mapaCorrida);
        n.x = mapaCorrida.x();
        n.y = mapaCorrida.y();
        n.orientacao = mapaCorrida.orientacao();
        n.paredes = mapaCorrida.paredesDe(n.x, n.y);
        n.valorFlood = d == INFINITO ? -1 : d;
        n.estadoRobo = modo.estado();
    }
    MensagemTelemetria tel = criarTelemetria(
        0, 0, millis() - inicioCorrida, (int8_t)n.x, (int8_t)n.y, letra(n.orientacao), n.paredes,
        fabsf(Encoders::velocidadeMs()), tensaoContrato(), n.valorFlood, estadoContrato(n.estadoRobo));
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
    console.printf("         navegacao: (%u,%u) %c paredes=%2u %s nucleo=%u flood=%d ciclos=%u tempo=%u/%u us "
                   "corridas simuladas ok=%u erradas=%u\n",
                   n.x, n.y, letra(n.orientacao), n.paredes, estadoContrato(n.estadoRobo), n.nucleo, n.valorFlood,
                   n.ciclos, n.tempoMedioUs, n.tempoMaxUs, n.corridasOk, n.corridasErradas);
    if (automatico)
        console.printf("         corrida automatica: (%u,%u) %c %s passos=%u viagens=%u rumo=%.1f desvio=(%.0f,%.0f) mm "
                       "recuperacoes=%u\n",
                       mapaCorrida.x(), mapaCorrida.y(), letra(mapaCorrida.orientacao()), nomeEstado(modo.estado()),
                       modo.passos(), modo.viagens(), controle.rumo(), roboFisico.desvioX(), roboFisico.desvioY(),
                       roboFisico.recuperacoes());
}

// ---------- limiares de parede (#279) ----------

const char *const NOME_LADO[3] = {"esquerdo", "frontal", "direito"};

// Distância de um lado com o robô no centro da célula e a parede ali: o
// controle de movimento centraliza e para por ela.
void definirReferencia(uint8_t lado, uint16_t mm)
{
    ParametrosMovimento &p = controle.parametros();
    (lado == LADO_ESQUERDO ? p.esquerdaCentroMm : lado == LADO_FRONTAL ? p.frontalCentroMm : p.direitaCentroMm) = mm;
}

// Limiares e referências calibrados ficam na flash (Preferences) e valem a
// partir do boot. Sem calibração, ficam os padrões do sensores.cpp e o
// controle de movimento anda só pelos encoders.
void carregarLimiares()
{
    for (uint8_t i = 0; i < 3; i++)
    {
        char chave[4] = {'p', 'l', (char)('0' + i), 0};
        uint32_t v = preferencias.getUInt(chave, 0); // liga << 16 | desliga
        if (v)
            sensores.configurarLimiares((LadoSensor)i, v >> 16, v & 0xFFFF);
        // Sem calibração, os lados usam 95/115 mm: parede ao lado lê 58-71 mm no
        // centro (até ~90 fora dele) e o poste da ponta de uma parede vizinha
        // leu 112 mm e virava parede falsa com o padrão de 120 (23/11/2026).
        else if (i != LADO_FRONTAL)
            sensores.configurarLimiares((LadoSensor)i, 95, 115);
        chave[1] = 'c';
        definirReferencia(i, preferencias.getUShort(chave, 0));
    }
}

// Média de cada lado em ms milissegundos (distância já filtrada pela mediana).
void medirLados(uint32_t ms, uint16_t media[3])
{
    uint32_t soma[3] = {0, 0, 0}, n = 0;
    for (uint32_t t0 = millis(); millis() - t0 < ms; n++)
    {
        esperarAtualizando(40); // um VL53L0X mede a cada ~33 ms
        for (uint8_t i = 0; i < 3; i++)
            soma[i] += sensores.leitura().distancia_mm[i];
    }
    for (uint8_t i = 0; i < 3; i++)
        media[i] = n ? soma[i] / n : 0;
}

// Comando 'L': o robô parado no centro de uma célula. Mede os 3 lados com
// parede e depois sem parede, calcula os limiares de cada lado e salva.
void calibrarLimiares()
{
    uint16_t com[3], sem[3];
    console.println("\n>>> CALIBRACAO DAS PAREDES (#279), robo parado no centro de uma celula");
    console.println("    1/2: paredes nos 3 lados (esquerda, frente, direita). Medindo em 5 s...");
    esperarAtualizando(5000);
    medirLados(2000, com);
    console.printf("    com parede: E=%u F=%u D=%u mm\n", com[0], com[1], com[2]);
    console.println("    2/2: TIRE as 3 paredes. Medindo em 8 s...");
    esperarAtualizando(8000);
    medirLados(2000, sem);
    console.printf("    sem parede: E=%u F=%u D=%u mm\n", sem[0], sem[1], sem[2]);
    for (uint8_t i = 0; i < 3; i++)
    {
        // O centro da célula vem só da 1ª fase: vale mesmo se a 2ª falhar (no
        // labirinto, 23/11/2026, não deu tempo de tirar o robô e o centro errado,
        // aprendido na primeira parede, fazia ele parar 6 cm antes).
        char chaveCentro[4] = {'p', 'c', (char)('0' + i), 0};
        if (com[i] > 0 && com[i] < 150)
        {
            preferencias.putUShort(chaveCentro, com[i]);
            definirReferencia(i, com[i]);
            console.printf("    %-8s centro a %u mm (salvo)\n", NOME_LADO[i], com[i]);
        }
        LimiaresParede l = calcularLimiares(com[i], sem[i]);
        if (!l.ok)
        {
            console.printf("    %-8s NAO calibrado (com %u, sem %u mm): mantem %u/%u\n", NOME_LADO[i], com[i], sem[i],
                           sensores.limiarLiga((LadoSensor)i), sensores.limiarDesliga((LadoSensor)i));
            continue;
        }
        sensores.configurarLimiares((LadoSensor)i, l.liga, l.desliga);
        char chave[4] = {'p', 'l', (char)('0' + i), 0};
        preferencias.putUInt(chave, ((uint32_t)l.liga << 16) | l.desliga);
        console.printf("    %-8s parede abaixo de %u mm, livre acima de %u mm (salvo)\n", NOME_LADO[i], l.liga,
                       l.desliga);
    }
}

// ---------- queda de Wi-Fi provocada (#282) ----------

// Desliga o rádio por alguns segundos com o robô funcionando, para conferir
// que a navegação segue, que o buffer guarda as leituras e que, na volta, o
// evento reconexao e os pendentes chegam ao servidor. Comandos Q, G e H.
uint32_t quedaFim = 0;     // millis() em que o Wi-Fi volta; 0 = sem queda
uint32_t quedaVolta = 0;   // millis() em que o rádio foi religado
bool quedaRelatar = false; // falta registrar o resultado no log

void provocarQueda(uint32_t segundos)
{
    if (!wifiAtivo)
    {
        console.println(">>> Wi-Fi desligado neste firmware (sem credenciais.h)");
        return;
    }
    console.printf(">>> QUEDA DE WI-FI por %lu s (%s). O console Wi-Fi cai: acompanhe pela serial ou "
                   "reconecte depois e mande 'l'\n",
                   (unsigned long)segundos, emCorrida ? "corrida em andamento" : "sem corrida: 'i' gera telemetria");
    delay(100);
    WiFi.mode(WIFI_OFF);
    quedaFim = millis() + segundos * 1000;
    if (quedaFim == 0)
        quedaFim = 1;
    quedaRelatar = true;
}

void acompanharQueda(uint32_t agora)
{
    if (quedaFim && (int32_t)(agora - quedaFim) >= 0)
    {
        quedaFim = 0;
        quedaVolta = agora;
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_SENHA);
    }
    if (!quedaRelatar || quedaFim)
        return;
    // Relata quando o buffer esvaziar com o servidor de volta, ou 60 s depois
    // de religar o rádio (servidor desligado, por exemplo).
    EstatisticasTelemetria e = TelemetriaWifi::estatisticas();
    bool descarregou = e.servidor && e.pendentes == 0;
    if (!descarregou && agora - quedaVolta < 60000)
        return;
    quedaRelatar = false;
    console.printf(">>> fim da queda: %s, pendentes %lu, ocupacao maxima %lu, descartes %lu, heap livre %u B "
                   "(minimo desde o boot %u B)\n",
                   descarregou ? "buffer descarregado" : "servidor nao voltou em 60 s", (unsigned long)e.pendentes,
                   (unsigned long)e.ocupacaoMaxima, (unsigned long)e.descartes, ESP.getFreeHeap(), ESP.getMinFreeHeap());
    // O log guarda 60 caracteres: só os números, para ler depois com 'l'.
    LogErros::registrar(false, "queda: pend %lu max %lu desc %lu heap %u", (unsigned long)e.pendentes,
                        (unsigned long)e.ocupacaoMaxima, (unsigned long)e.descartes, ESP.getMinFreeHeap());
}

// ---------- laço: o que roda parado e durante o movimento ----------

void verificarBotao(bool podeIniciar);
void iniciarOta();

// Bateria, telemetria, status e rede. Os sensores e os motores já foram
// atualizados por quem chama.
void tarefasPeriodicas(uint32_t agora)
{
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
    acompanharQueda(agora);
    if (wifiAtivo && WiFi.status() == WL_CONNECTED)
    {
        if (!otaIniciado)
            iniciarOta();
        ArduinoOTA.handle();
        console.atender();
    }
}

// Um ciclo enquanto o robô se move (HardwareReal::ler). Do console só valem
// os comandos de parar, o status e o log; o botão BOOT também para.
void servicoDuranteMovimento()
{
    uint32_t agora = millis();
    sensores.atualizar();
    Motores::atualizar(agora);
    Encoders::atualizar(agora);
    tarefasPeriodicas(agora);
    for (int c = console.ler(); c >= 0; c = console.ler())
    {
        if (c == 'p' || c == 'a')
        {
            cancelarMovimento = true;
            console.println("\n>>> movimento interrompido pelo console");
            if (emCorrida)
                encerrarCorrida(false);
        }
        else if (c == 's')
            mostrarStatus();
        else if (c == 'O')
            logMovimento = !logMovimento;
    }
    verificarBotao(false);
    vTaskDelay(1); // watchdog do núcleo 0
}

// ---------- labirinto da corrida ----------

void carregarLabirinto()
{
    celulasAFrente = preferencias.getUChar("lf", 4);
    celulasAoLado = preferencias.getUChar("ll", 4);
    uint8_t b = preferencias.getUChar("lb", BORDA_ESQUERDA);
    borda = b <= BORDA_FRENTE_ESQUERDA ? (LadoBorda)b : BORDA_ESQUERDA;
}

void mostrarLabirinto()
{
    Labirinto teste;
    bool ok = teste.iniciarNaLargada(borda, celulasAFrente, celulasAoLado);
    const char *nomes[] = {"atras e a esquerda", "atras e a direita", "a frente e a direita", "a frente e a esquerda"};
    console.printf(">>> labirinto: %u celulas a frente x %u ao lado, borda %s", celulasAFrente, celulasAoLado,
                   nomes[borda]);
    if (ok)
        console.printf(" -> mapa %ux%u, largada virada para %c, objetivo (%u,%u)\n", teste.largura(), teste.altura(),
                       letra(teste.orientacaoInicial()), teste.largura() - 1, teste.altura() - 1);
    else
        console.println(" -> tamanho fora do TAP");
}

// 'M': passa pelos tamanhos do TAP, vistos da largada.
void trocarTamanho()
{
    const uint8_t tamanhos[][2] = {{4, 4}, {4, 8}, {8, 4}, {4, 12}, {12, 4}};
    const uint8_t n = sizeof(tamanhos) / sizeof(tamanhos[0]);
    uint8_t i = 0;
    while (i < n && !(tamanhos[i][0] == celulasAFrente && tamanhos[i][1] == celulasAoLado))
        i++;
    i = (i + 1) % n;
    celulasAFrente = tamanhos[i][0];
    celulasAoLado = tamanhos[i][1];
    preferencias.putUChar("lf", celulasAFrente);
    preferencias.putUChar("ll", celulasAoLado);
    mostrarLabirinto();
}

// 'B': onde ficam as bordas do canto, vistas pelo robô na largada.
void trocarBorda()
{
    borda = (LadoBorda)((borda + 1) % (BORDA_FRENTE_ESQUERDA + 1));
    preferencias.putUChar("lb", borda);
    mostrarLabirinto();
}

// ---------- velocidade: PWM máximo de 0 a 255 ----------

// Teto do PWM (0 a 255, como no analogWrite) para tudo o que o controle faz:
// andar, girar e o reforço da roda parada. Pedido da equipe em 23/11/2026:
// devagar, 20. Ajuste com + e - (passos de 5); fica salvo na flash.
uint8_t pwmMaximo255 = 20;

// O controle trabalha em fração dos 5 V do motor; o PWM de verdade é essa
// fração vezes Motores::pwmMaximo() (5 V / bateria). Converte o teto para a
// tensão de agora: chamar antes de cada movimento.
void aplicarPwmMaximo()
{
    ParametrosMovimento &p = controle.parametros();
    float teto = (pwmMaximo255 / 255.0f) / Motores::pwmMaximo();
    teto = teto > 1 ? 1 : teto;
    p.potenciaMaxima = p.potenciaGiroMaxima = p.potenciaArranqueMaxima = teto;
    ParametrosMovimento padrao;
    p.potenciaMinima = padrao.potenciaMinima < teto ? padrao.potenciaMinima : teto;
    p.potenciaGiroMinima = padrao.potenciaGiroMinima < teto ? padrao.potenciaGiroMinima : teto;
}

void mudarPwmMaximo(int passo)
{
    int v = pwmMaximo255 + passo;
    pwmMaximo255 = (uint8_t)(v < 5 ? 5 : (v > 255 ? 255 : v));
    preferencias.putUChar("pwm", pwmMaximo255);
    aplicarPwmMaximo();
    console.printf(">>> PWM maximo %u de 255 (%.0f%%, ~%.2f V no motor com a bateria em %.2f V)\n", pwmMaximo255,
                   pwmMaximo255 / 2.55f, pwmMaximo255 / 255.0f * bateria.tensao(), bateria.tensao());
}

// ---------- calibração dos encoders pelo sensor da frente (comando Y) ----------

// Pulsos por mm de cada roda, medidos no robô e salvos na flash; 0 = o valor
// do motores.h. O controle usa a média; HardwareReal::ler corrige cada roda.
void aplicarPulsosPorMm(float esquerda, float direita)
{
    if (esquerda <= 0 || direita <= 0)
        return;
    ppmEsquerda = esquerda;
    ppmDireita = direita;
    float media = (esquerda + direita) / 2;
    controle.parametros().pulsosPorMm = media;
    Encoders::definirPulsosPorVolta(media * 3.14159265f * Encoders::DIAMETRO_RODA_MM);
}

// Robô parado num corredor, com a parede do fim dele a 15-45 cm na frente (de
// perto: a 50-60 cm o VL53L0X lê ~15% curto e a escala saía errada, 23/11/2026).
// Anda devagar em linha reta uns 20 cm e compara o quanto a distância da
// frente diminuiu com os pulsos de cada encoder: dá os pulsos por mm de
// verdade e mostra se algum encoder ou motor está invertido (bancada,
// 23/11/2026: o robô andou 31 cm e os encoders contaram ~1,5 cm).
void calibrarEncoders()
{
    if (emCorrida || !bateria.presente())
    {
        console.println(emCorrida ? ">>> durante uma corrida, nao" : ">>> sem bateria, motores bloqueados");
        return;
    }
    uint16_t ini[3], fim[3];
    medirLados(600, ini);
    if (ini[LADO_FRONTAL] < 150 || ini[LADO_FRONTAL] > 450)
    {
        console.printf(">>> calibracao dos encoders: precisa de parede a 15-45 cm na frente (agora %u mm)\n",
                       ini[LADO_FRONTAL]);
        return;
    }
    aplicarPwmMaximo();
    float p = controle.parametros().potenciaMaxima;
    console.printf("\n>>> CALIBRACAO DOS ENCODERS: andando ~20 cm em linha reta (PWM %u/255, 'p' para)\n", pwmMaximo255);
    Encoders::zerar();
    uint32_t t0 = millis(), ultimoAumento = t0;
    bool parou = false;
    Motores::definir(p, p);
    while (millis() - t0 < 5000)
    {
        esperarAtualizando(10);
        const LeituraSensores &l = sensores.leitura();
        if (l.distancia_mm[LADO_FRONTAL] < 80)
            break;
        if (console.ler() == 'p')
        {
            parou = true;
            break;
        }
        // Não saiu do lugar: sobe a potência aos poucos (no máximo 0,65 = ~3 V).
        // Um pulso solto (vibração) não é andar: só conta a partir de 20.
        bool andou = labs(Encoders::esquerdo()) > 20 || labs(Encoders::direito()) > 20 ||
                     ini[LADO_FRONTAL] - l.distancia_mm[LADO_FRONTAL] > 15;
        if (!andou && millis() - ultimoAumento > 300 && p < 0.65f)
        {
            p += 0.03f;
            ultimoAumento = millis();
            Motores::definir(p, p);
        }
    }
    Motores::parar();
    esperarAtualizando(400);
    medirLados(600, fim);
    int32_t pe = Encoders::esquerdo(), pd = Encoders::direito();
    int dF = (int)ini[LADO_FRONTAL] - fim[LADO_FRONTAL];
    // Lado sem parede perto (chão aberto, 23/11/2026: móveis a 40-70 cm) varia à toa: só conta lado com parede.
    // Parede que some no caminho (corredor abrindo) também não conta.
    int dE = ini[LADO_ESQUERDO] < 200 && fim[LADO_ESQUERDO] < 200 ? (int)fim[LADO_ESQUERDO] - ini[LADO_ESQUERDO] : 0;
    int dD = ini[LADO_DIREITO] < 200 && fim[LADO_DIREITO] < 200 ? (int)fim[LADO_DIREITO] - ini[LADO_DIREITO] : 0;
    console.printf(">>> frente %u -> %u mm (andou %d mm), lados E %u->%u D %u->%u, encoders E=%ld D=%ld, potencia %.2f%s\n",
                   ini[LADO_FRONTAL], fim[LADO_FRONTAL], dF, ini[LADO_ESQUERDO], fim[LADO_ESQUERDO], ini[LADO_DIREITO],
                   fim[LADO_DIREITO], (long)pe, (long)pd, p, parou ? " (interrompido)" : "");
    if (dF < -40)
        console.println(">>> o robo andou para TRAS com os dois motores 'para frente': motores invertidos (INVERTE_* em motores.h)");
    if (abs(dE) > 40 || abs(dD) > 40)
        console.println(">>> as paredes dos lados mudaram muito: o robo girou (um motor invertido ou muito mais fraco)");
    if (dF > 40 && pe < 0)
        console.println(">>> encoder ESQUERDO conta para tras andando para frente: inverta Encoders::INVERTE_ESQUERDO");
    if (dF > 40 && pd < 0)
        console.println(">>> encoder DIREITO conta para tras andando para frente: inverta Encoders::INVERTE_DIREITO");
    if (dF < 60 || abs(dE) > 40 || abs(dD) > 40 || pe == 0 || pd == 0)
    {
        console.println(">>> calibracao NAO salva: o robo precisa andar pelo menos 6 cm em linha reta");
        return;
    }
    // Cada roda pela sua contagem. O desvio lateral medido (as paredes dos
    // lados) é pequeno perto do avanço, então as duas andaram quase o mesmo.
    float ppmE = labs(pe) / (float)dF, ppmD = labs(pd) / (float)dF;
    console.printf(">>> pulsos por mm: E=%.2f D=%.2f (era %.2f nas duas) -> %.0f e %.0f pulsos por volta da roda (salvo)\n",
                   ppmE, ppmD, controle.parametros().pulsosPorMm, ppmE * 3.14159265f * Encoders::DIAMETRO_RODA_MM,
                   ppmD * 3.14159265f * Encoders::DIAMETRO_RODA_MM);
    preferencias.putFloat("ppmE", ppmE);
    preferencias.putFloat("ppmD", ppmD);
    aplicarPulsosPorMm(ppmE, ppmD);
}

// ---------- calibração do giro sem BNO085 (comando R) ----------

// Robô parado no centro da célula da largada (beco: paredes atrás e dos dois
// lados). Gira devagar no lugar ~1,5 volta, guardando a diferença dos encoders
// e as 3 distâncias; o desenho das paredes se repete a cada volta de verdade e
// dá a largura efetiva do eixo (calibracao_giro.h). Salva na flash.
void calibrarGiro()
{
    if (emCorrida || !bateria.presente())
    {
        console.println(emCorrida ? ">>> durante uma corrida, nao" : ">>> sem bateria, motores bloqueados");
        return;
    }
    static float diferenca[1024];
    static uint16_t dist[1024][3];
    ParametrosMovimento &par = controle.parametros();
    const float nominal = par.larguraEixoMm;
    const float total = giroNecessarioMm(nominal);
    aplicarPwmMaximo();
    console.printf("\n>>> CALIBRACAO DO GIRO: 6 giros de 90 graus a esquerda no lugar (largura atual %.1f mm, 'p' "
                   "para)\n", nominal);
    cancelarMovimento = false;
    EntradaMovimento e0 = hardware.ler();
    controle.reiniciar(e0, 0);
    uint16_t n = 0;
    float d = 0;
    uint32_t t0 = millis();
    // Gira pelo controle de movimento: ele dá reforço à roda que trava e faz a
    // outra esperar, senão o robô pivota numa roda e sai do lugar (1ª tentativa
    // na bancada, 23/11/2026: a esquerda não saiu e o robô pivotou nela).
    EstadoMovimento fim = MOV_CONCLUIDO;
    // Algum sensor a menos de 15 mm: para antes de encostar (fios, cantos). Era 30: no beco,
    // 9 mm fora do centro, a frente passa a ~29 mm da parede do lado sem sair do lugar (24/11/2026).
    bool pertoDemais = false;
    for (int quarto = 1; quarto <= 6 && d < total && fim == MOV_CONCLUIDO && !cancelarMovimento && !pertoDemais;
         quarto++)
    {
        controle.girar(quarto * 90.0f);
        while (!controle.terminou() && n < 1024 && !cancelarMovimento)
        {
            EntradaMovimento e = hardware.ler();
            for (uint8_t i = 0; i < 3; i++)
                if (e.distancia_mm[i] > 0 && e.distancia_mm[i] < 15 && !pertoDemais)
                {
                    pertoDemais = true;
                    console.printf(">>> PAROU: sensor %s a %u mm de algo\n",
                                   i == 0 ? "esquerdo" : i == 1 ? "frontal" : "direito", e.distancia_mm[i]);
                }
            if (pertoDemais)
                break;
            hardware.aplicar(controle.atualizar(e));
            d = ((e.encoderDireito - e0.encoderDireito) - (e.encoderEsquerdo - e0.encoderEsquerdo)) / par.pulsosPorMm;
            if (n == 0 || d >= diferenca[n - 1] + total / 1000) // ~1000 amostras no giro todo
            {
                diferenca[n] = d;
                for (uint8_t i = 0; i < 3; i++)
                    dist[n][i] = e.distancia_mm[i];
                n++;
            }
        }
        fim = controle.estado();
    }
    controle.parar();
    Motores::frear();
    esperarAtualizando(300);
    Motores::parar();
    if (fim != MOV_CONCLUIDO)
        console.printf(">>> giro parou: %s (reforco E=%.2f D=%.2f)\n", nomeMovimento(fim), controle.reforcoEsquerdo(),
                       controle.reforcoDireito());
    console.printf(">>> encoders E=%ld D=%ld\n", (long)(Encoders::esquerdo()), (long)(Encoders::direito()));
    float erro = -1;
    float largura = d >= total ? larguraEixoPeloGiro(diferenca, dist, n, nominal, &erro) : 0;
    console.printf(">>> girou %.0f mm de diferenca dos encoders (%.0f graus pela largura atual), %u amostras, %.1f s%s\n",
                   d, d / nominal * 57.2958f, n, (millis() - t0) / 1000.0f, cancelarMovimento ? " (interrompido)" : "");
    if (largura <= 0)
    {
        console.printf(">>> calibracao do giro NAO salva (erro entre as voltas %.1f mm): de frente para uma parede a ~10 cm, "
                       "celula, o robo precisa girar sem sair do lugar\n", erro);
        // A curva para analisar no computador: giro,<diferenca mm>,<frente mm>
        for (uint16_t i = 0; i < n; i++)
            console.printf("giro,%.1f,%u\n", diferenca[i], dist[i][LADO_FRONTAL]);
        return;
    }
    console.printf(">>> largura efetiva do eixo %.1f mm (era %.1f; erro entre as voltas %.1f mm) -> 90 graus = %.0f mm "
                   "de diferenca dos encoders (salvo). Reposicione o robo.\n",
                   largura, nominal, erro, largura * 1.5708f);
    par.larguraEixoMm = largura;
    preferencias.putFloat("eixo", largura);
}

// ---------- corrida automática (#210, RF13-S) ----------

const char *nomeFalha(MotivoFalha m)
{
    switch (m)
    {
    case FALHA_CALIBRACAO: return "sensor de distancia sem resposta na largada";
    case FALHA_SEM_CAMINHO: return "mapa sem caminho ate o destino";
    case FALHA_MOVIMENTO: return "robo nao conseguiu andar (colisao?)";
    case FALHA_LIMITE: return "passos demais";
    default: return "sem falha";
    }
}

// O contrato só tem estes motivos: colisão e tempo esgotado são os mais
// próximos; o motivo exato vai para o log de erros.
MotivoAbandono motivoContrato(MotivoFalha m)
{
    return m == FALHA_MOVIMENTO ? MOTIVO_COLISAO : (m == FALHA_LIMITE ? MOTIVO_TEMPO_ESGOTADO : MOTIVO_MANUAL);
}

// Sem a calibração 'L', usa o que os sensores veem na largada (o robô parado
// no beco): com as duas paredes dos lados, a média delas vira a referência
// dos dois lados (centraliza entre elas); com parede na frente, a distância
// de agora. Vale só para esta corrida: a flash continua sem calibração.
void referenciasDaLargada()
{
    ParametrosMovimento &p = controle.parametros();
    carregarLimiares(); // volta ao calibrado (ou a 0) antes de completar
    const LeituraSensores &l = sensores.leitura();
    bool pe = l.parede[LADO_ESQUERDO], pf = l.parede[LADO_FRONTAL], pd = l.parede[LADO_DIREITO];
    if (p.esquerdaCentroMm == 0 && p.direitaCentroMm == 0 && pe && pd)
        p.esquerdaCentroMm = p.direitaCentroMm =
            (l.distancia_mm[LADO_ESQUERDO] + l.distancia_mm[LADO_DIREITO]) / 2;
    if (p.frontalCentroMm == 0 && pf)
        p.frontalCentroMm = l.distancia_mm[LADO_FRONTAL];
    console.printf(">>> paredes na largada: E=%s F=%s D=%s; referencias do centro: frente %u, esquerda %u, direita %u mm%s\n",
                   pe ? "sim" : "nao", pf ? "sim" : "nao", pd ? "sim" : "nao", p.frontalCentroMm, p.esquerdaCentroMm,
                   p.direitaCentroMm, preferencias.getUShort("pc1", 0) ? " (calibradas)" : " (da largada, sem L)");
}

void iniciarCorridaAutomatica()
{
    if (emCorrida)
        return;
    if (!bateria.presente())
    {
        console.println(">>> corrida automatica: sem bateria, motores bloqueados");
        return;
    }
    if (!mapaCorrida.iniciarNaLargada(borda, celulasAFrente, celulasAoLado))
    {
        console.println(">>> corrida automatica: tamanho do labirinto invalido (comandos M e B)");
        return;
    }
    roboFisico.definirOrientacaoInicial(mapaCorrida.orientacaoInicial());
    iniciarCorrida(); // calibra, zera o yaw e manda o evento 'inicio'
    if (!emCorrida)
        return;
    mostrarLabirinto();
    referenciasDaLargada();
    aplicarPwmMaximo();
    console.printf(">>> PWM maximo %u de 255 (+ e - ajustam)\n", pwmMaximo255);
    console.println(">>> CORRIDA AUTOMATICA em 1 s: tire a mao do robo (p, a ou BOOT param)");
    cancelarMovimento = false;
    automatico = true;
    for (uint32_t t0 = millis(); millis() - t0 < 1000 && emCorrida;)
        servicoDuranteMovimento();
    if (!emCorrida || cancelarMovimento)
        return;
    modo.pararNoObjetivo(true); // chegou ao centro da chegada: para (a chegada é um beco)
    modo.iniciar();
}

// Uma etapa do modo automático (a calibração ou uma célula). Chamado pelo
// laço enquanto a corrida automática estiver ativa.
void passoAutomatico()
{
    EstadoRobo antes = modo.estado();
    EstadoRobo agora = modo.passo();
    if (!emCorrida || !automatico)
    {
        // Parada pelo console, pelo botão ou pela bateria no meio do movimento.
        Motores::parar();
        automatico = false;
        return;
    }
    if (agora != antes)
        console.printf(">>> modo automatico: %s -> %s\n", nomeEstado(antes), nomeEstado(agora));
    console.printf(">>> (%u,%u) %c %s passos=%u %s\n", mapaCorrida.x(), mapaCorrida.y(), letra(mapaCorrida.orientacao()),
                   nomeEstado(agora), modo.passos(), nomeMovimento(roboFisico.ultimoResultado()));
    if (agora == ESTADO_PARADO)
    {
        console.printf(">>> OBJETIVO: rota rapida de %u celulas, %u passos no total, %u viagens de exploracao\n",
                       modo.passosRapido(), modo.passos(), modo.viagens());
        encerrarCorrida(true);
    }
    else if (agora == ESTADO_FALHA)
    {
        LogErros::registrar(true, "corrida automatica: %s", nomeFalha(modo.motivo()));
        encerrarCorrida(false, motivoContrato(modo.motivo()));
    }
}

// ---------- movimentos soltos na bancada (CT-SE05 a CT-SE07) ----------

// O robô parado num corredor: o rumo de agora, arredondado para 90°, é o eixo.
void movimentoBancada(char c)
{
    if (emCorrida)
    {
        console.println(">>> durante uma corrida, nao");
        return;
    }
    if (!bateria.presente())
    {
        console.println(">>> sem bateria, motores bloqueados");
        return;
    }
    cancelarMovimento = false;
    aplicarPwmMaximo();
    EntradaMovimento e = hardware.ler();
    controle.reiniciar(e, e.yawOk ? e.yawGraus : 0);
    float eixo = roundf(controle.rumo() / 90) * 90;
    bool ok = true;
    if (c == 'F' || c == '3')
        for (int i = 0; i < (c == '3' ? 3 : 1) && ok; i++)
            ok = roboFisico.andarMm(CELULA_MM, eixo);
    else if (c == '#') // 360°: 4 giros de 90 à esquerda, no próprio eixo
        for (int k = 1; k <= 4 && ok; k++)
            ok = roboFisico.girarPara(eixo + k * 90);
    else
        ok = roboFisico.girarPara(eixo + (c == '<' ? 90 : c == '>' ? -90 : 180));
    Motores::parar();
    console.printf(">>> %s: %s, andou %.0f mm, rumo %.1f graus (%s), erro lateral %.1f mm, recuperacoes %u\n",
                   c == 'F' ? "1 celula" : c == '3' ? "3 celulas" : c == 'U' ? "meia-volta" : c == '#' ? "giro de 360" : "giro de 90",
                   ok ? "ok" : nomeMovimento(roboFisico.ultimoResultado()), controle.percorridoMm(), controle.rumo(),
                   e.yawOk ? "BNO085" : "encoders", controle.lateralMm(), roboFisico.recuperacoes());
}

void mostrarParametros()
{
    aplicarPwmMaximo();
    const ParametrosMovimento &p = controle.parametros();
    console.printf("\n>>> PWM MAXIMO %u de 255 (+ e - ajustam)\n", pwmMaximo255);
    console.printf("\n>>> CONTROLE DE MOVIMENTO (lib/movimento/controle_movimento.h)\n"
                   "    celula %u mm, %.2f pulsos/mm, largura do eixo %.1f mm (so sem BNO085)\n"
                   "    andar: potencia %.2f a %.2f, ganhos distancia %.4f rumo %.4f lateral %.2f\n"
                   "    girar: potencia %.2f a %.2f, ganho %.4f, tolerancia %.1f graus\n"
                   "    centro da celula (calibracao L): frente %u, esquerda %u, direita %u mm%s\n",
                   CELULA_MM, p.pulsosPorMm, p.larguraEixoMm, p.potenciaMinima, p.potenciaMaxima, p.ganhoDistancia,
                   p.ganhoRumo, p.ganhoLateral, p.potenciaGiroMinima, p.potenciaGiroMaxima, p.ganhoGiro,
                   p.toleranciaGiroGraus, p.frontalCentroMm, p.esquerdaCentroMm, p.direitaCentroMm,
                   p.frontalCentroMm ? "" : " (NAO calibrado: comando L)");
    mostrarLabirinto();
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
    case 'b':
    {
        // Diagnóstico do BNO085 (16/11/2026): conta as quedas do INT (D19, vai a
        // 0 com leitura pronta) e as leituras que chegam pelo I2C em 3 s.
        // INT caindo sem leituras = I2C perde; INT parado = o sensor não mede.
        static volatile uint32_t quedas = 0;
        attachInterrupt(PINO_BNO_INT, [] { quedas++; }, FALLING);
        quedas = 0;
        LeituraSensores a = sensores.leitura();
        for (uint32_t t0 = millis(); millis() - t0 < 3000;)
        {
            sensores.atualizar();
            delay(2);
        }
        detachInterrupt(PINO_BNO_INT);
        const LeituraSensores &l = sensores.leitura();
        // Quem responde no I2C nos dois endereços possíveis do BNO085 (AD0).
        for (uint8_t end : {0x4A, 0x4B})
        {
            Wire.beginTransmission(end);
            console.printf(">>> I2C 0x%02X: %s\n", end, Wire.endTransmission() == 0 ? "responde" : "nada");
        }
        console.printf(">>> BNO085 em 3 s: INT caiu %lu vezes, %lu leituras I2C, %u resets, imuOk=%d, INT agora=%d\n",
                       (unsigned long)quedas, (unsigned long)(l.imuEventos - a.imuEventos),
                       l.imuReinicios - a.imuReinicios, l.imuOk, digitalRead(PINO_BNO_INT));
        break;
    }
    case '6':
        // Alterna o teto dos motores entre 5 V (padrão) e 6 V (máximo do N20),
        // para rodar h, j, m... a 6 V. Volta para 5 V ao reiniciar.
        Motores::definirTensaoMaxima(Motores::tensaoMaxima() < 5.5f ? Motores::TENSAO_ABSOLUTA_MOTOR_V
                                                                    : Motores::TENSAO_MAX_MOTOR_V);
        console.printf(">>> teto dos motores: %.1f V (PWM max %.0f%%)\n", Motores::tensaoMaxima(),
                       Motores::pwmMaximo() * 100);
        break;
    case 'V':
    {
        // Cada motor direto a 6 V (máximo do N20), sem rampa, por 6 s. PWM bruto
        // calculado pela bateria: no motor nunca passa de 6 V.
        if (!bateria.presente())
        {
            console.println(">>> sem bateria, motores bloqueados");
            break;
        }
        float pwm = Motores::TENSAO_ABSOLUTA_MOTOR_V / bateria.tensao();
        pwm = pwm > 1 ? 1 : pwm;
        console.printf("\n>>> 6 V DIRETO por 6 s cada (rodas no ar!) bateria %.2f V, PWM %.0f%%\n", bateria.tensao(),
                       pwm * 100);
        for (int m = 0; m < 2; m++)
        {
            int32_t p0 = m == 0 ? Encoders::esquerdo() : Encoders::direito();
            Motores::pwmBruto(m == 0 ? pwm : 0, m == 1 ? pwm : 0);
            for (int s = 0; s < 6; s++)
            {
                // Sem Motores::atualizar: a rampa zeraria o PWM bruto.
                for (uint32_t t0 = millis(); millis() - t0 < 1000;)
                {
                    sensores.atualizar();
                    Encoders::atualizar(millis());
                    delay(5);
                }
                console.printf("    %-8s %ds: %5.0f pulsos/s\n", m == 0 ? "esquerdo" : "direito", s + 1,
                               m == 0 ? Encoders::pulsosPorSegundoEsquerda() : Encoders::pulsosPorSegundoDireita());
            }
            Motores::parar();
            console.printf("    %-8s total: %ld pulsos\n", m == 0 ? "esquerdo" : "direito",
                           (long)((m == 0 ? Encoders::esquerdo() : Encoders::direito()) - p0));
            esperarAtualizando(500);
        }
        console.println(">>> teste 6 V concluido");
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
    case 'A': iniciarCorridaAutomatica(); break;
    case 'M': trocarTamanho(); break;
    case 'B': trocarBorda(); break;
    case 'F':
    case '3':
    case '<':
    case '>':
    case 'U':
    case '#': movimentoBancada(c); break;
    case 'O':
        logMovimento = !logMovimento;
        console.printf(">>> log do controle %s (mov,ms,estado,mm,lateral,parede,rumo,sinal,encE,encD,potE,potD,frente)\n",
                       logMovimento ? "ligado" : "desligado");
        break;
    case 'K': mostrarParametros(); break;
    case 'Y': calibrarEncoders(); break;
    case 'R': calibrarGiro(); break;
    case '[':
    case ']':
    {
        // Ajuste fino da proporção das rodas (1% na direita por toque): o Y feito
        // com o robô desviando exagera a diferença e o robô pende para um lado.
        // Pende para a direita: ']' (direita conta mais por mm, o controle vê o desvio).
        float d = ppmDireita * (c == ']' ? 1.01f : 0.99f);
        preferencias.putFloat("ppmD", d);
        aplicarPulsosPorMm(ppmEsquerda, d);
        console.printf(">>> pulsos por mm: E=%.2f D=%.2f (salvo)\n", ppmEsquerda, d);
        break;
    }
    case '=':
    {
        // As duas rodas na mesma escala (a média): mesmo motor e encoder; o Y
        // feito com o robô desviando separa as duas à toa (23/11/2026: 5,07 e
        // 6,08 deixavam o rumo dos encoders errado e o robô ia para a esquerda).
        float media = (ppmEsquerda + ppmDireita) / 2;
        preferencias.putFloat("ppmE", media);
        preferencias.putFloat("ppmD", media);
        aplicarPulsosPorMm(media, media);
        console.printf(">>> pulsos por mm iguais nas duas rodas: %.2f (salvo)\n", media);
        break;
    }
    case '(':
    case ')':
    {
        // Ajuste à mão da largura do eixo (giro sem BNO085): girou menos que o
        // pedido, aumente; girou mais, diminua. Fica salvo na flash.
        float &l = controle.parametros().larguraEixoMm;
        l = fminf(fmaxf(l + (c == ')' ? 5 : -5), 50), 400);
        preferencias.putFloat("eixo", l);
        console.printf(">>> largura do eixo %.0f mm (salvo)\n", l);
        break;
    }
    case '+': mudarPwmMaximo(5); break;
    case '-': mudarPwmMaximo(-5); break;
    case 'L': calibrarLimiares(); break;     // limiares de parede (#279)
    case 'Q': provocarQueda(5); break;       // queda curta (#282)
    case 'G': provocarQueda(60); break;      // queda longa
    case 'H': provocarQueda(12 * 60); break; // enche o buffer: 3200 leituras = 10 min 40 s a 5 Hz
    case 'z': sensores.zerarYaw(); console.println(">>> yaw zerado"); break;
    case 's': mostrarStatus(); break;
    case 'r': TelemetriaWifi::zerarRtt(); console.println(">>> RTT zerado"); break;
    default: break;
    }
}

// BOOT sem corrida: toque curto inicia uma corrida com o robô parado (só
// telemetria, como antes); segurado por 1 s, a corrida automática, ao soltar.
// Com corrida, apertar encerra: concluída na parada, abandonada na automática.
// Durante um movimento (podeIniciar = false), o botão só para.
void verificarBotao(bool podeIniciar)
{
    // O pino começa como "apertado" para não disparar se o reset pelo USB
    // deixar o BOOT em nível baixo.
    static bool anterior = false;
    static uint32_t ultimaMudanca = 0;
    static uint32_t apertouEm = 0; // 0 = não está esperando soltar
    if (millis() < 3000)
        return;
    bool agora = digitalRead(PINO_BOTAO_BOOT);
    if (agora == anterior || millis() - ultimaMudanca <= 50)
        return;
    ultimaMudanca = millis();
    anterior = agora;
    if (!agora) // apertou
    {
        apertouEm = 0;
        if (emCorrida)
            encerrarCorrida(!automatico);
        else if (podeIniciar)
            apertouEm = millis() | 1;
        else
            cancelarMovimento = true; // movimento da bancada
    }
    else if (apertouEm) // soltou
    {
        bool longo = millis() - apertouEm >= 1000;
        apertouEm = 0;
        longo ? iniciarCorridaAutomatica() : iniciarCorrida();
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
    carregarLimiares();
    carregarLabirinto();
    pwmMaximo255 = preferencias.getUChar("pwm", 20);
    // Padrão: o medido com o comando Y em 23/11/2026 (esquerda 1,00, direita 1,60).
    // Sem o Y: as duas rodas iguais (no ar a 5 V contam quase o mesmo, 23/11/2026:
    // 5911 e 5753 pulsos em 2 s), na média dos valores antigos (1,0 e 1,6).
    aplicarPulsosPorMm(preferencias.getFloat("ppmE", 1.30f), preferencias.getFloat("ppmD", 1.30f));
    controle.parametros().larguraEixoMm = preferencias.getFloat("eixo", controle.parametros().larguraEixoMm);
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
    console.println("Comandos: i f a c l s m e p z r, corrida automatica A (veja o topo do main.cpp)");
    mostrarLabirinto();
}

#ifdef TESTE_AO_LIGAR
// Teste ao ligar (pedido de 16/11/2026): 10 s depois do boot, os dois motores
// a 6 V por 15 s, com os VL53L0X e o BNO085 lidos ao mesmo tempo. Na bateria o
// PWM é 6 V / tensão da bateria; no USB o driver só recebe ~4,8 V, então vai a
// 100 % (fica abaixo de 6 V). RODAS NO AR. 'p' pelo console interrompe.
const uint32_t TESTE_AO_LIGAR_ATRASO_MS = 10000;
const uint32_t TESTE_AO_LIGAR_DURACAO_MS = 15000;

void testeAoLigar()
{
    bool usb = !bateria.presente();
    float vAlim = usb ? sensores.leitura().tensao_v : bateria.tensao();
    float pwm = usb ? 1.0f : Motores::TENSAO_ABSOLUTA_MOTOR_V / vAlim;
    pwm = pwm > 1 ? 1 : pwm;
    console.printf("\n>>> TESTE AO LIGAR: motores a %.1f V por %lu s (%s %.2f V, PWM %.0f%%), RODAS NO AR\n",
                   pwm * vAlim, TESTE_AO_LIGAR_DURACAO_MS / 1000, usb ? "USB" : "bateria", vAlim, pwm * 100);
    if (usb)
        Motores::informarTensaoBateria(8.0f); // libera os motores só neste teste
    Motores::pwmBruto(pwm, pwm);
    console.println("  tempo | dist E   F   D (mm) | yaw graus | pulsos/s E    D | BNO leituras/s resets");
    uint32_t t0 = millis(), ultimo = 0, eventosAntes = sensores.leitura().imuEventos;
    while (millis() - t0 < TESTE_AO_LIGAR_DURACAO_MS)
    {
        // Sem Motores::atualizar: a rampa zeraria o PWM bruto.
        sensores.atualizar();
        Encoders::atualizar(millis());
        if (millis() - ultimo >= 500)
        {
            ultimo = millis();
            const LeituraSensores &l = sensores.leitura();
            console.printf("  %4.1fs | %5u %5u %5u   | %8.1f%s | %6.0f %6.0f | %6lu %5u\n", (ultimo - t0) / 1000.0f,
                           l.distancia_mm[LADO_ESQUERDO], l.distancia_mm[LADO_FRONTAL], l.distancia_mm[LADO_DIREITO],
                           l.yawGraus, l.imuOk ? "" : " (BNO sem resposta)", Encoders::pulsosPorSegundoEsquerda(),
                           Encoders::pulsosPorSegundoDireita(), (unsigned long)(l.imuEventos - eventosAntes) * 2,
                           l.imuReinicios);
            eventosAntes = l.imuEventos;
        }
        if (wifiAtivo && WiFi.status() == WL_CONNECTED)
            console.atender();
        if (console.ler() == 'p')
        {
            console.println(">>> interrompido ('p')");
            break;
        }
        delay(5);
    }
    Motores::parar();
    if (usb)
        Motores::informarTensaoBateria(0);
    console.println(">>> teste ao ligar concluido, motores parados");
}

#endif

void loop()
{
    uint32_t agora = millis();
#ifdef TESTE_AO_LIGAR
    static bool testeFeito = false;
    if (!testeFeito && agora >= TESTE_AO_LIGAR_ATRASO_MS)
    {
        testeFeito = true;
        testeAoLigar();
        agora = millis();
    }
#endif
    sensores.permitirReinicioImu(!automatico);
    sensores.atualizar();
    Motores::atualizar(agora);
    Encoders::atualizar(agora);
    tarefasPeriodicas(agora);
    for (int c = console.ler(); c >= 0; c = console.ler())
        tratarComando((char)c);
    verificarBotao(true);
    // Corrida automática: uma célula por volta do laço. Durante o movimento,
    // o HardwareReal mantém a telemetria, a bateria e o console em dia.
    if (automatico && emCorrida)
        passoAutomatico();
    // O loop roda no núcleo 0 (defeito no núcleo 1), onde o watchdog vigia a
    // tarefa ociosa: sem ceder a vez, um laço rápido derruba a ESP32.
    vTaskDelay(1);
}
