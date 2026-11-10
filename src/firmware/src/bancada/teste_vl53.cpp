// Teste de bancada 3.7: 3 sensores VL53L0X com enderecos trocados via XSHUT.
// So USB, sem bateria. Gravar: pio run -e teste_vl53 -t upload
#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>

const int PIN_DRV_SLEEP = 32;
const int PIN_BNO_RST = 18;

struct Sensor {
  const char* nome;
  int xshut;
  uint8_t endereco;
  VL53L0X dev;
  bool ok;
};

Sensor sensores[] = {
  {"Esquerdo", 17, 0x30, VL53L0X(), false},
  {"Frontal",  16, 0x31, VL53L0X(), false},
  {"Direito",   4, 0x32, VL53L0X(), false},
};

// Lista quem responde no I2C (diagnostico do XSHUT).
void varrer(const char* etapa) {
  Serial.printf("  [%s] I2C:", etapa);
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) Serial.printf(" 0x%02X", a);
  }
  Serial.println();
}

void setup() {
  pinMode(PIN_DRV_SLEEP, OUTPUT);
  digitalWrite(PIN_DRV_SLEEP, LOW);
  pinMode(PIN_BNO_RST, OUTPUT);
  digitalWrite(PIN_BNO_RST, HIGH);

  Serial.begin(115200);
  delay(500);
  Wire.begin(21, 22);
  Wire.setTimeOut(50);

  // Desliga todos, depois acorda um por vez e troca o endereco.
  for (auto& s : sensores) {
    pinMode(s.xshut, OUTPUT);
    digitalWrite(s.xshut, LOW);
  }
  delay(20);

  Serial.println("\n=== Teste VL53L0X ===");
  varrer("todos XSHUT em LOW, 0x29 nao deveria aparecer");
  for (auto& s : sensores) {
    digitalWrite(s.xshut, HIGH);
    delay(20);
    varrer(s.nome);
    s.dev.setTimeout(100);
    s.ok = s.dev.init();
    if (s.ok) {
      s.dev.setAddress(s.endereco);
      // medicao sob demanda (um laser por vez) no loop
    }
    Serial.printf("%-9s (XSHUT GPIO%-2d) -> %s 0x%02X\n", s.nome, s.xshut,
                  s.ok ? "OK, endereco" : "FALHOU (nao respondeu)", s.endereco);
  }
  Serial.println("\nDistancias em mm (8190+ = nada na frente / fora de alcance):");
}

void loop() {
  for (auto& s : sensores) {
    if (!s.ok) { Serial.printf("%s: --    ", s.nome); continue; }
    uint16_t mm = s.dev.readRangeSingleMillimeters();
    if (s.dev.timeoutOccurred()) Serial.printf("%s: TIMEOUT    ", s.nome);
    else Serial.printf("%s: %4u mm    ", s.nome, mm);
  }
  Serial.println();
  delay(250);
}
