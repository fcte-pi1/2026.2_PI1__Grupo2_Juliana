// Teste de bancada 3.1 + 3.3: ESP32 viva + varredura do barramento I2C.
// So USB, sem bateria. Gravar: pio run -e teste_i2c -t upload
#include <Arduino.h>
#include <Wire.h>

const int PIN_SDA = 21;
const int PIN_SCL = 22;
const int PIN_DRV_SLEEP = 32;  // driver dos motores desligado por seguranca
const int PIN_BNO_RST = 18;    // RST do BNO085 em nivel alto = fora de reset

const char* nome(uint8_t addr) {
  switch (addr) {
    case 0x29: return "VL53L0X (endereco de fabrica)";
    case 0x40: return "INA226";
    case 0x4A: return "BNO085 (AD0 no GND)";
    case 0x4B: return "BNO085 (AD0 no 3V3)";
    default:   return "desconhecido";
  }
}

void setup() {
  pinMode(PIN_DRV_SLEEP, OUTPUT);
  digitalWrite(PIN_DRV_SLEEP, LOW);
  pinMode(PIN_BNO_RST, OUTPUT);
  digitalWrite(PIN_BNO_RST, HIGH);

  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== ESP32 OK: %s rev%d, %d MHz, flash %u MB ===\n",
                ESP.getChipModel(), ESP.getChipRevision(),
                ESP.getCpuFreqMHz(), ESP.getFlashChipSize() / (1024 * 1024));

  // Diagnostico: sem nada falando, SDA e SCL devem ficar em ALTO (pull-up dos modulos).
  pinMode(PIN_SDA, INPUT);
  pinMode(PIN_SCL, INPUT);
  delay(10);
  int sda = digitalRead(PIN_SDA), scl = digitalRead(PIN_SCL);
  Serial.printf("Linhas I2C em repouso: SDA=%s SCL=%s\n",
                sda ? "ALTO (ok)" : "BAIXO (travada/curto com GND?)",
                scl ? "ALTO (ok)" : "BAIXO (travada/curto com GND?)");
  if (!sda || !scl) {
    Serial.println("Barramento I2C travado. Varredura cancelada.");
    while (true) delay(1000);
  }
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setTimeOut(50);
}

void loop() {
  Serial.println("\nVarrendo I2C...");
  int achados = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  0x%02X -> %s\n", addr, nome(addr));
      achados++;
    }
  }
  Serial.printf("Total: %d dispositivo(s)\n", achados);
  delay(3000);
}
