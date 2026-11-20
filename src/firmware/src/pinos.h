#pragma once

// Pinagem da placa (docs/4.3 e esquema da placa perfurada).

const int PINO_SDA = 21;
const int PINO_SCL = 22;

const int PINO_XSHUT_ESQUERDO = 17;
const int PINO_XSHUT_FRONTAL = 16;
const int PINO_XSHUT_DIREITO = 4;

const int PINO_BNO_INT = 19;
const int PINO_BNO_RST = 18;

// Motores como no docs/4.3. Encoders trocados em relação ao 4.3 na placa
// montada (teste com a bateria em 11/11/2026, roda empurrada à mão: o motor
// de D25/D26 gira a roda ESQUERDA, vista de trás, e o encoder dela está em VP/VN).
const int PINO_MOTOR_E_IN1 = 25; // IN1 -> OUT1/OUT2
const int PINO_MOTOR_E_IN2 = 26; // IN2
const int PINO_MOTOR_D_IN1 = 27; // IN3 -> OUT3/OUT4
const int PINO_MOTOR_D_IN2 = 33; // IN4
const int PINO_DRV_SLEEP = 32;

const int PINO_ENC_E_A = 36; // VP
const int PINO_ENC_E_B = 39; // VN
const int PINO_ENC_D_A = 34;
const int PINO_ENC_D_B = 35;

const int PINO_BOTAO_BOOT = 0; // botão BOOT da placa: inicia/encerra a corrida
const int PINO_LED = 2;
