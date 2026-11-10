// Teste de bancada 3.5: contagem dos encoders girando a roda com a mao.
// So USB, sem bateria. Gravar: pio run -e teste_encoder -t upload
// Comando pela serial: 'z' zera as contagens.
#include <Arduino.h>

const int PIN_DRV_SLEEP = 32;
const int ENC_E_A = 34, ENC_E_B = 35;  // roda esquerda
const int ENC_D_A = 36, ENC_D_B = 39;  // roda direita (VP / VN)

volatile long contE = 0, contD = 0;

// Conta toda borda do canal A; o canal B diz o sentido.
void IRAM_ATTR isrE() { contE += (digitalRead(ENC_E_A) == digitalRead(ENC_E_B)) ? 1 : -1; }
void IRAM_ATTR isrD() { contD += (digitalRead(ENC_D_A) == digitalRead(ENC_D_B)) ? 1 : -1; }

void setup() {
  pinMode(PIN_DRV_SLEEP, OUTPUT);
  digitalWrite(PIN_DRV_SLEEP, LOW);
  Serial.begin(115200);
  delay(500);
  for (int p : {ENC_E_A, ENC_E_B, ENC_D_A, ENC_D_B}) pinMode(p, INPUT);
  attachInterrupt(ENC_E_A, isrE, CHANGE);
  attachInterrupt(ENC_D_A, isrD, CHANGE);
  Serial.println("\n=== Teste encoders ===");
  Serial.println("Marque a roda, de 1 volta completa e anote o numero. 'z' zera.");
}

void loop() {
  if (Serial.available() && Serial.read() == 'z') { contE = 0; contD = 0; }
  Serial.printf("Esquerda: %7ld   Direita: %7ld   (pinos A/B: E=%d%d D=%d%d)\n",
                contE, contD, digitalRead(ENC_E_A), digitalRead(ENC_E_B),
                digitalRead(ENC_D_A), digitalRead(ENC_D_B));
  delay(300);
}
