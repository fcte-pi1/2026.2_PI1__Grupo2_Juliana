#pragma once

// Pinagem da placa (docs/4.3 e esquema da placa perfurada).

const int PINO_SDA = 21;
const int PINO_SCL = 22;

const int PINO_XSHUT_ESQUERDO = 17;
const int PINO_XSHUT_FRONTAL = 16;
const int PINO_XSHUT_DIREITO = 4;

const int PINO_BNO_INT = 19;
const int PINO_BNO_RST = 18;

const int PINO_MOTOR_E_IN1 = 25;
const int PINO_MOTOR_E_IN2 = 26;
const int PINO_MOTOR_D_IN1 = 27;
const int PINO_MOTOR_D_IN2 = 33;
const int PINO_DRV_SLEEP = 32;

const int PINO_ENC_E_A = 34;
const int PINO_ENC_E_B = 35;
const int PINO_ENC_D_A = 36; // VP
const int PINO_ENC_D_B = 39; // VN

const int PINO_BOTAO_BOOT = 0; // botão BOOT da placa: inicia/encerra a corrida
const int PINO_LED = 2;
