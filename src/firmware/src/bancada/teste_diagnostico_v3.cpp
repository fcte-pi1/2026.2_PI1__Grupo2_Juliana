// Super diagnóstico V3 (anti-TG1WDT): BNO085 com reset pelo D18, tensão do
// INA226 e encoders. Só serial (115200), sem Wi-Fi.
// Gravar pelo USB: pio run -e teste_diagnostico_v3 -t upload
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BNO08x.h>
#include <math.h>

#define I2C_SDA 21
#define I2C_SCL 22
#define BNO_INT 19
#define BNO_RST 18
#define INA226_ADDR 0x40

#define ENC_D_A 36
#define ENC_D_B 39
#define ENC_E_A 34
#define ENC_E_B 35
#define MOT_SLEEP 32

Adafruit_BNO08x bno08x(-1); // Controlaremos o RST manualmente no pino 18 para ser infalível
sh2_SensorValue_t sensorValue;

bool bnoOk = false;
bool inaOk = false;

float yawGraus = 0.0, pitchGraus = 0.0, rollGraus = 0.0;
float tensaoVBS = 0.0, corrente_mA = 0.0;

volatile long pulsosEsq = 0;
volatile long pulsosDir = 0;
volatile uint32_t ultimoPulsoEsqUs = 0;
volatile uint32_t ultimoPulsoDirUs = 0;

unsigned long ultimoPrint = 0;
unsigned long ultimaLeituraBNO = 0;
unsigned long ultimaLeituraINA = 0;

// Filtro de 500us nas interrupções: impede que o toque da mão gere TG1WDT_SYS_RESET!
void IRAM_ATTR lerEncoderEsq() {
  uint32_t agora = micros();
  if (agora - ultimoPulsoEsqUs > 500) {
    ultimoPulsoEsqUs = agora;
    if (digitalRead(ENC_E_B) == HIGH) pulsosEsq++;
    else pulsosEsq--;
  }
}

void IRAM_ATTR lerEncoderDir() {
  uint32_t agora = micros();
  if (agora - ultimoPulsoDirUs > 500) {
    ultimoPulsoDirUs = agora;
    if (digitalRead(ENC_D_B) == HIGH) pulsosDir++;
    else pulsosDir--;
  }
}

void quaternionParaGraus(float qr, float qi, float qj, float qk) {
  float sqr = sq(qr), sqi = sq(qi), sqj = sq(qj), sqk = sq(qk);
  float yawRad   = atan2(2.0 * (qi * qj + qk * qr), (sqi - sqj - sqk + sqr));
  float pitchRad = asin(-2.0 * (qi * qk - qj * qr) / (sqi + sqj + sqk + sqr));
  float rollRad  = atan2(2.0 * (qj * qk + qi * qr), (-sqi - sqj + sqk + sqr));

  yawGraus = yawRad * 180.0 / PI;
  if (yawGraus < 0) yawGraus += 360.0;
  pitchGraus = pitchRad * 180.0 / PI;
  rollGraus  = rollRad * 180.0 / PI;
}

int16_t lerRegistradorINA(uint8_t reg) {
  Wire.beginTransmission(INA226_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return 0;
  Wire.requestFrom(INA226_ADDR, (uint8_t)2);
  if (Wire.available() == 2) {
    return (int16_t)((Wire.read() << 8) | Wire.read());
  }
  return 0;
}

void resetarEIniciarBNO() {
  // Dá um reset físico no BNO085 pelo pino D18 (nunca mais trava em 0.0°!)
  pinMode(BNO_RST, OUTPUT);
  digitalWrite(BNO_RST, LOW);
  delay(30);
  digitalWrite(BNO_RST, HIGH);
  delay(200);

  if (bno08x.begin_I2C(0x4A, &Wire)) {
    bnoOk = true;
    bno08x.enableReport(SH2_GAME_ROTATION_VECTOR, 25000);
    ultimaLeituraBNO = millis();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== SUPER DIAGNOSTICO V3 (ANTI-TG1WDT) ===");

  pinMode(MOT_SLEEP, OUTPUT);
  digitalWrite(MOT_SLEEP, LOW);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(25);

  Wire.beginTransmission(INA226_ADDR);
  if (Wire.endTransmission() == 0) {
    inaOk = true;
    Serial.println("[OK] INA226 pronto (0x40)");
  }

  pinMode(BNO_INT, INPUT_PULLUP);
  resetarEIniciarBNO();
  if (bnoOk) Serial.println("[OK] BNO085 pronto (0x4A)");

  pinMode(ENC_E_A, INPUT);
  pinMode(ENC_E_B, INPUT);
  pinMode(ENC_D_A, INPUT);
  pinMode(ENC_D_B, INPUT);
  attachInterrupt(digitalPinToInterrupt(ENC_E_A), lerEncoderEsq, RISING);
  attachInterrupt(digitalPinToInterrupt(ENC_D_A), lerEncoderDir, RISING);
}

void loop() {
  if (bnoOk && bno08x.wasReset()) {
    bno08x.enableReport(SH2_GAME_ROTATION_VECTOR, 25000);
  }

  if (bnoOk && bno08x.getSensorEvent(&sensorValue)) {
    if (sensorValue.sensorId == SH2_GAME_ROTATION_VECTOR) {
      ultimaLeituraBNO = millis();
      quaternionParaGraus(
        sensorValue.un.gameRotationVector.real,
        sensorValue.un.gameRotationVector.i,
        sensorValue.un.gameRotationVector.j,
        sensorValue.un.gameRotationVector.k
      );

      if (inaOk && (millis() - ultimaLeituraINA >= 500)) {
        ultimaLeituraINA = millis();
        tensaoVBS   = lerRegistradorINA(0x02) * 0.00125;
        corrente_mA = (lerRegistradorINA(0x01) * 0.0025) / 0.1;
      }
    }
  }

  // Se ficar 600ms sem atualizar (ex: mexeu no cabo), recupera em 0.2s sem travar em 0.0°!
  if (millis() - ultimaLeituraBNO > 600) {
    resetarEIniciarBNO();
  }

  if (millis() - ultimoPrint >= 250) {
    ultimoPrint = millis();
    Serial.print("🧭 Yaw: ");
    Serial.print(yawGraus, 1);
    Serial.print("° | Pitch: ");
    Serial.print(pitchGraus, 1);
    Serial.print("° | Roll: ");
    Serial.print(rollGraus, 1);
    Serial.print("°  ||  ⚡ VBS: ");
    Serial.print(tensaoVBS, 2);
    Serial.print(" V  ||  ⚙️ Enc Esq: ");
    Serial.print(pulsosEsq);
    Serial.print(" | Dir: ");
    Serial.println(pulsosDir);
  }
}
