// Teste de bancada 3.3: angulo (yaw/pitch/roll) do BNO085.
// So USB, sem bateria. Gravar: pio run -e teste_bno -t upload
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BNO08x.h>

const int PIN_DRV_SLEEP = 32;
const int PIN_BNO_RST = 18;

Adafruit_BNO08x bno(PIN_BNO_RST);
sh2_SensorValue_t valor;

void setup() {
  pinMode(PIN_DRV_SLEEP, OUTPUT);
  digitalWrite(PIN_DRV_SLEEP, LOW);
  Serial.begin(115200);
  delay(500);
  Wire.begin(21, 22);
  Wire.setTimeOut(50);

  Serial.println("\n=== Teste BNO085 ===");
  if (!bno.begin_I2C(0x4A, &Wire)) {
    Serial.println("BNO085 NAO respondeu no 0x4A.");
    while (true) delay(1000);
  }
  // Game Rotation Vector: giro + acelerometro, sem bussola (bussola atrapalha perto de motor).
  bno.enableReport(SH2_GAME_ROTATION_VECTOR, 20000);  // 50 Hz
  Serial.println("BNO085 OK. Gire a placa na mao (yaw = giro em cima da mesa).");
}

void loop() {
  if (bno.wasReset()) bno.enableReport(SH2_GAME_ROTATION_VECTOR, 20000);
  if (!bno.getSensorEvent(&valor) || valor.sensorId != SH2_GAME_ROTATION_VECTOR) return;

  float r = valor.un.gameRotationVector.real, i = valor.un.gameRotationVector.i;
  float j = valor.un.gameRotationVector.j, k = valor.un.gameRotationVector.k;
  float yaw   = atan2f(2 * (r * k + i * j), 1 - 2 * (j * j + k * k)) * RAD_TO_DEG;
  float pitch = asinf(constrain(2 * (r * j - k * i), -1.0f, 1.0f)) * RAD_TO_DEG;
  float roll  = atan2f(2 * (r * i + j * k), 1 - 2 * (i * i + j * j)) * RAD_TO_DEG;

  static uint32_t ultimo = 0;
  if (millis() - ultimo >= 200) {
    ultimo = millis();
    Serial.printf("yaw %7.1f   pitch %6.1f   roll %6.1f\n", yaw, pitch, roll);
  }
}
