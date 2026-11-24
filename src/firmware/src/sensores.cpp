#include "sensores.h"

#include <Adafruit_BNO08x.h>
#include <Arduino.h>
#include <VL53L0X.h>
#include <Wire.h>

#include "filtro_distancia.h"
#include "pinos.h"

namespace
{
struct SensorDistancia
{
    int xshut;
    uint8_t endereco;
    VL53L0X dev;
    bool ok;
    FiltroDistancia filtro;
    DetectorParede detector;
};

// Mesma ordem de LadoSensor. Endereços trocados no boot pelo XSHUT, porque os
// três saem de fábrica no 0x29 (validado na bancada em 10/11/2026).
SensorDistancia distancia[3] = {
    {PINO_XSHUT_ESQUERDO, 0x30, VL53L0X(), false, FiltroDistancia(), DetectorParede()},
    {PINO_XSHUT_FRONTAL, 0x31, VL53L0X(), false, FiltroDistancia(), DetectorParede(150, 170)},
    {PINO_XSHUT_DIREITO, 0x32, VL53L0X(), false, FiltroDistancia(), DetectorParede()},
};

Adafruit_BNO08x bno(-1); // reset feito à mão em iniciarBno()

// Reset pelo RST (D18) e espera o BNO085 iniciar (300 ms) antes de falar com
// ele, como no bancada/teste_diagnostico_v3.cpp. O reset da biblioteca chamava
// cedo demais: o sensor respondia no I2C mas nunca mandava leituras (yaw em 0).
// LIMITE CONHECIDO (16/11/2026): no USB funciona (50 leituras/s); na bateria a
// configuração pelo I2C ainda falha, mesmo sem Wi-Fi e sem os VL53L0X. Suspeita:
// alimentação de 3,3 V vinda do MP1584. Comando 'b' do console mostra o estado.
bool iniciarBno()
{
    pinMode(PINO_BNO_INT, INPUT_PULLUP);
    digitalWrite(PINO_BNO_RST, LOW);
    pinMode(PINO_BNO_RST, OUTPUT);
    delay(30);
    digitalWrite(PINO_BNO_RST, HIGH);
    delay(300); // com 200 ms ainda nao mandava leituras; 300 ms validado no USB em 16/11/2026
    return bno.begin_I2C(0x4A, &Wire) && bno.enableReport(SH2_GAME_ROTATION_VECTOR, 20000);
}

const uint8_t INA226_ENDERECO = 0x40;
const float INA226_SHUNT_OHM = 0.1f; // resistor R100 do módulo

bool lerRegistroIna(uint8_t reg, uint16_t &valor)
{
    Wire.beginTransmission(INA226_ENDERECO);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0)
        return false;
    if (Wire.requestFrom(INA226_ENDERECO, (uint8_t)2) != 2)
        return false;
    valor = (uint16_t)((Wire.read() << 8) | Wire.read());
    return true;
}

float normalizar(float graus)
{
    while (graus > 180)
        graus -= 360;
    while (graus <= -180)
        graus += 360;
    return graus;
}
} // namespace

uint8_t Sensores::iniciar()
{
    Wire.begin(PINO_SDA, PINO_SCL);
    Wire.setClock(100000); // BNO085 nao manda leituras a 400 kHz nesta placa (16/11/2026)
    Wire.setTimeOut(50);

    uint8_t ok = ligarDistancia();

    leitura_.imuOk = iniciarBno();
    ultimoEventoImu_ = millis();
    if (leitura_.imuOk)
        ok++;

    uint16_t fabricante = 0;
    leitura_.inaOk = lerRegistroIna(0xFE, fabricante) && fabricante == 0x5449; // "TI"
    if (leitura_.inaOk)
        ok++;
    return ok;
}

void Sensores::atualizar()
{
    for (uint8_t i = 0; i < 3; i++)
    {
        SensorDistancia &s = distancia[i];
        if (s.ok)
        {
            // Só lê quando há medida nova: não trava o laço esperando o sensor
            // (cada medida leva ~33 ms em modo contínuo).
            uint8_t pronto = s.dev.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07;
            if (s.dev.last_status != 0)
                s.filtro.adicionar(0, true); // erro no I2C: sensor não respondeu
            else if (pronto)
            {
                uint16_t mm = s.dev.readRangeContinuousMillimeters();
                s.filtro.adicionar(mm, s.dev.timeoutOccurred());
            }
        }
        else
            s.filtro.adicionar(0, true);
        leitura_.distancia_mm[i] = s.filtro.valor();
        leitura_.defeito[i] = s.filtro.defeito();
        leitura_.parede[i] = s.filtro.pronto() && s.detector.atualizar(s.filtro.valor());
    }
    lerImu();
    lerIna();
}

void Sensores::lerImu()
{
    uint32_t agora = millis();
    // BNO085 mudo há 1 s (ou falhou no boot): reset pelo pino RST (D18) e liga
    // o relatório de novo. Sem isso o yaw fica congelado no último valor.
    // Se já está falhando, para após 3 tentativas: cada uma bloqueia ~330 ms.
    if (agora - ultimoEventoImu_ > 1000 && (leitura_.imuOk || leitura_.imuReinicios < 3))
    {
        leitura_.imuReinicios++;
        leitura_.imuOk = iniciarBno();
        ultimoEventoImu_ = millis();
        return;
    }
    if (!leitura_.imuOk)
        return;
    if (bno.wasReset())
        bno.enableReport(SH2_GAME_ROTATION_VECTOR, 20000);
    sh2_SensorValue_t v;
    while (bno.getSensorEvent(&v))
    {
        if (v.sensorId != SH2_GAME_ROTATION_VECTOR)
            continue;
        ultimoEventoImu_ = agora;
        leitura_.imuEventos++;
        float r = v.un.gameRotationVector.real, i = v.un.gameRotationVector.i;
        float j = v.un.gameRotationVector.j, k = v.un.gameRotationVector.k;
        yawBruto_ = atan2f(2 * (r * k + i * j), 1 - 2 * (j * j + k * k)) * RAD_TO_DEG;
    }
    leitura_.yawGraus = normalizar(yawBruto_ - yawZero_);
}

void Sensores::lerIna()
{
    uint16_t barramento = 0, shunt = 0;
    leitura_.inaOk = lerRegistroIna(0x02, barramento) && lerRegistroIna(0x01, shunt);
    if (!leitura_.inaOk)
        return;
    leitura_.tensao_v = barramento * 0.00125f;                                // LSB 1,25 mV
    leitura_.corrente_ma = (int16_t)shunt * 0.0025f / INA226_SHUNT_OHM;       // LSB 2,5 uV
}

// Liga os 3 VL53L0X um por vez e troca o endereço de cada um (todos acordam em 0x29).
uint8_t Sensores::ligarDistancia()
{
    for (auto &s : distancia)
    {
        pinMode(s.xshut, OUTPUT);
        digitalWrite(s.xshut, LOW); // XSHUT em 0: o VL53L0X desliga e sai do I2C
        s.ok = false;
    }
    delay(20);
    uint8_t ok = 0;
    for (auto &s : distancia)
    {
        digitalWrite(s.xshut, HIGH);
        delay(50); // o VL53L0X precisa desse tempo depois do XSHUT
        s.dev = VL53L0X(); // acorda no endereço de fábrica (0x29), não no 0x3x de antes
        s.dev.setTimeout(100);
        s.ok = s.dev.init();
        if (s.ok)
        {
            s.dev.setAddress(s.endereco);
            s.dev.startContinuous();
            ok++;
        }
    }
    return ok;
}

void Sensores::zerarYaw()
{
    yawZero_ = yawBruto_;
    leitura_.yawGraus = 0;
}

// O BNO085 mede positivo girando à esquerda (anti-horário visto de cima).
// Largada = Norte; girar 90° à direita = Leste.
Direcao Sensores::orientacao() const
{
    float y = leitura_.yawGraus;
    if (y >= -45 && y <= 45)
        return NORTE;
    if (y < -45 && y >= -135)
        return LESTE;
    if (y > 45 && y <= 135)
        return OESTE;
    return SUL;
}

uint8_t Sensores::mascaraParedes() const
{
    Direcao frente = orientacao();
    uint8_t m = 0;
    if (leitura_.parede[LADO_FRONTAL])
        m |= frente;
    if (leitura_.parede[LADO_ESQUERDO])
        m |= girarEsquerda(frente);
    if (leitura_.parede[LADO_DIREITO])
        m |= girarDireita(frente);
    return m;
}
