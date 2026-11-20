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

Adafruit_BNO08x bno(PINO_BNO_RST);

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
    Wire.setClock(400000);
    Wire.setTimeOut(50);

    uint8_t ok = 0;
    for (auto &s : distancia)
    {
        pinMode(s.xshut, OUTPUT);
        digitalWrite(s.xshut, LOW);
    }
    delay(20);
    for (auto &s : distancia)
    {
        digitalWrite(s.xshut, HIGH);
        delay(50); // o VL53L0X precisa desse tempo depois do XSHUT
        s.dev.setTimeout(100);
        s.ok = s.dev.init();
        if (s.ok)
        {
            s.dev.setAddress(s.endereco);
            s.dev.startContinuous();
            ok++;
        }
    }

    leitura_.imuOk = bno.begin_I2C(0x4A, &Wire) && bno.enableReport(SH2_GAME_ROTATION_VECTOR, 20000);
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
    if (!leitura_.imuOk)
        return;
    if (bno.wasReset())
        bno.enableReport(SH2_GAME_ROTATION_VECTOR, 20000);
    sh2_SensorValue_t v;
    while (bno.getSensorEvent(&v))
    {
        if (v.sensorId != SH2_GAME_ROTATION_VECTOR)
            continue;
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
