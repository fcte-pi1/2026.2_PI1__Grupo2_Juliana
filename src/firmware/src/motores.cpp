#include "motores.h"

#include <Arduino.h>

#include "pinos.h"

namespace
{
// ---------- motores ----------
const uint32_t FREQ_PWM = 20000; // acima do audível
const uint8_t BITS_PWM = 10;
const uint32_t PWM_CHEIO = (1u << BITS_PWM) - 1;
// Canais do LEDC: entradas IN1/IN2 de cada motor.
const uint8_t CANAL_E1 = 0, CANAL_E2 = 1, CANAL_D1 = 2, CANAL_D2 = 3;

float alvoE = 0, alvoD = 0;   // pedido, -1 a 1 (fração dos 5 V)
float atualE = 0, atualD = 0; // depois da rampa
float tensaoBateria = 0;
uint32_t ultimaRampa = 0;
bool ligado = false;
float aplicadoE = 0, aplicadoD = 0;

float limitar(float v, float lim) { return v > lim ? lim : (v < -lim ? -lim : v); }

float aproximar(float atual, float alvo, float passo)
{
    if (alvo > atual)
        return atual + passo > alvo ? alvo : atual + passo;
    return atual - passo < alvo ? alvo : atual - passo;
}

bool decaimentoLento = false;
float tetoMotor = Motores::TENSAO_MAX_MOTOR_V;

// Decaimento rápido: PWM numa entrada e 0 na outra (motor solto no intervalo).
// Decaimento lento: 1 numa entrada e PWM invertido na outra (motor em curto no
// intervalo): mais força em baixa rotação. A tensão média é a mesma nos dois.
void aplicar(uint8_t canal1, uint8_t canal2, float pwm)
{
    uint32_t duty = (uint32_t)(fabsf(pwm) * PWM_CHEIO + 0.5f);
    if (pwm == 0)
    {
        ledcWrite(canal1, 0);
        ledcWrite(canal2, 0);
    }
    else if (decaimentoLento)
    {
        ledcWrite(canal1, pwm > 0 ? PWM_CHEIO : PWM_CHEIO - duty);
        ledcWrite(canal2, pwm < 0 ? PWM_CHEIO : PWM_CHEIO - duty);
    }
    else
    {
        ledcWrite(canal1, pwm > 0 ? duty : 0);
        ledcWrite(canal2, pwm < 0 ? duty : 0);
    }
}

void escrever()
{
    float maximo = Motores::pwmMaximo();
    aplicadoE = atualE * maximo * (Motores::INVERTE_ESQUERDO ? -1 : 1);
    aplicadoD = atualD * maximo * (Motores::INVERTE_DIREITO ? -1 : 1);
    aplicar(CANAL_E1, CANAL_E2, aplicadoE);
    aplicar(CANAL_D1, CANAL_D2, aplicadoD);
}

// ---------- encoders ----------
volatile int32_t contE = 0, contD = 0;
int32_t anteriorE = 0, anteriorD = 0;
uint32_t ultimaVelocidade = 0;
float ppsE = 0, ppsD = 0;

// Conta as duas bordas do canal A; o canal B diz o sentido.
void IRAM_ATTR isrEsquerdo() { contE += (digitalRead(PINO_ENC_E_A) == digitalRead(PINO_ENC_E_B)) ? 1 : -1; }
void IRAM_ATTR isrDireito() { contD += (digitalRead(PINO_ENC_D_A) == digitalRead(PINO_ENC_D_B)) ? 1 : -1; }
} // namespace

namespace Motores
{
void iniciar()
{
    pinMode(PINO_DRV_SLEEP, OUTPUT);
    digitalWrite(PINO_DRV_SLEEP, LOW);
    const uint8_t canais[] = {CANAL_E1, CANAL_E2, CANAL_D1, CANAL_D2};
    const int pinos[] = {PINO_MOTOR_E_IN1, PINO_MOTOR_E_IN2, PINO_MOTOR_D_IN1, PINO_MOTOR_D_IN2};
    for (int i = 0; i < 4; i++)
    {
        ledcSetup(canais[i], FREQ_PWM, BITS_PWM);
        ledcAttachPin(pinos[i], canais[i]);
        ledcWrite(canais[i], 0);
    }
    ligado = false;
}

void informarTensaoBateria(float volts)
{
    tensaoBateria = volts;
    if (!bateriaPresente() && ligado)
        parar();
}

// Faixa de uma LiPo 2S. Fora dela (só USB, sem leitura) o motor não liga: no
// USB os 5 V voltam pelo MP1584 até o DRV8833 e o motor puxaria do USB do PC.
bool bateriaPresente() { return tensaoBateria >= 6.0f && tensaoBateria <= 9.0f; }

float pwmMaximo()
{
    // Tensão fora da faixa de uma 2S (sem bateria, só USB, leitura ruim):
    // usa o pior caso para nunca passar de 5 V no motor.
    float v = bateriaPresente() ? tensaoBateria : TENSAO_PIOR_CASO_V;
    float m = tetoMotor / v;
    return m > 1 ? 1 : m;
}

void definir(float esquerdo, float direito)
{
    if (!bateriaPresente())
    {
        alvoE = alvoD = 0;
        return;
    }
    alvoE = limitar(esquerdo, 1);
    alvoD = limitar(direito, 1);
    if ((alvoE != 0 || alvoD != 0) && !ligado)
    {
        digitalWrite(PINO_DRV_SLEEP, HIGH);
        ligado = true;
        ultimaRampa = millis();
    }
}

void atualizar(uint32_t agora)
{
    if (!ligado)
        return;
    float passo = RAMPA_POR_SEGUNDO * (agora - ultimaRampa) / 1000.0f;
    ultimaRampa = agora;
    atualE = aproximar(atualE, alvoE, passo);
    atualD = aproximar(atualD, alvoD, passo);
    escrever();
}

void frear()
{
    alvoE = alvoD = atualE = atualD = 0;
    aplicadoE = aplicadoD = 0;
    for (uint8_t c : {CANAL_E1, CANAL_E2, CANAL_D1, CANAL_D2})
        ledcWrite(c, PWM_CHEIO);
}

void parar()
{
    frear();
    delay(50);
    for (uint8_t c : {CANAL_E1, CANAL_E2, CANAL_D1, CANAL_D2})
        ledcWrite(c, 0);
    digitalWrite(PINO_DRV_SLEEP, LOW);
    ligado = false;
}

bool habilitado() { return ligado; }
void usarDecaimentoLento(bool lento) { decaimentoLento = lento; }
void definirTensaoMaxima(float volts)
{
    tetoMotor = volts > TENSAO_ABSOLUTA_MOTOR_V ? TENSAO_ABSOLUTA_MOTOR_V : volts;
}
float tensaoMaxima() { return tetoMotor; }
void pwmBruto(float esquerdo, float direito)
{
    if (!bateriaPresente())
        return;
    if (!ligado)
    {
        digitalWrite(PINO_DRV_SLEEP, HIGH);
        ligado = true;
    }
    alvoE = alvoD = atualE = atualD = 0; // a rampa não mexe enquanto isso
    aplicadoE = limitar(esquerdo, 1) * (INVERTE_ESQUERDO ? -1 : 1);
    aplicadoD = limitar(direito, 1) * (INVERTE_DIREITO ? -1 : 1);
    aplicar(CANAL_E1, CANAL_E2, aplicadoE);
    aplicar(CANAL_D1, CANAL_D2, aplicadoD);
}
void definirFrequencia(uint32_t hz)
{
    for (uint8_t c : {CANAL_E1, CANAL_E2, CANAL_D1, CANAL_D2})
        ledcChangeFrequency(c, hz, BITS_PWM);
}
float pwmEsquerdo() { return aplicadoE; }
float pwmDireito() { return aplicadoD; }
} // namespace Motores

namespace Encoders
{
void iniciar()
{
    for (int p : {PINO_ENC_E_A, PINO_ENC_E_B, PINO_ENC_D_A, PINO_ENC_D_B})
        pinMode(p, INPUT);
    attachInterrupt(PINO_ENC_E_A, isrEsquerdo, CHANGE);
    attachInterrupt(PINO_ENC_D_A, isrDireito, CHANGE);
}

void zerar()
{
    noInterrupts();
    contE = contD = 0;
    interrupts();
    anteriorE = anteriorD = 0;
}

int32_t esquerdo() { return INVERTE_ESQUERDO ? -contE : contE; }
int32_t direito() { return INVERTE_DIREITO ? -contD : contD; }

void atualizar(uint32_t agora)
{
    uint32_t dt = agora - ultimaVelocidade;
    if (dt < 100)
        return;
    int32_t e = esquerdo(), d = direito();
    ppsE = (e - anteriorE) * 1000.0f / dt;
    ppsD = (d - anteriorD) * 1000.0f / dt;
    anteriorE = e;
    anteriorD = d;
    ultimaVelocidade = agora;
}

float pulsosPorSegundoEsquerda() { return ppsE; }
float pulsosPorSegundoDireita() { return ppsD; }

float velocidadeMs()
{
    const float metrosPorPulso = 3.14159265f * DIAMETRO_RODA_MM / 1000.0f / PULSOS_POR_VOLTA;
    return (ppsE + ppsD) / 2.0f * metrosPorPulso;
}
} // namespace Encoders
