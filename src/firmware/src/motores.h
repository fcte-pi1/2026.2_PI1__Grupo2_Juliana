#pragma once

#include <stdint.h>

// Motores N20 pelo DRV8833 e encoders de quadratura (#275). Só roda no ESP32.
//
// LIMITE DE 5 V NOS MOTORES (pedido da Eletrônica): o PWM nunca passa de
// TENSAO_MAX_MOTOR_V / tensão da bateria. Com a bateria cheia (8,4 V) o PWM
// máximo é 59,5 %. Sem leitura da bateria, vale o pior caso (8,4 V). A
// potência também sobe em rampa (0 -> máximo em 0,5 s), sem tranco.
//
// Calibração (robô montado, rodas no ar, comando 'm' no console):
//  - INVERTE_*: um motor é montado espelhado; troque se a roda girar para trás
//    com potência positiva, ou se o encoder contar negativo andando para frente.
//  - PULSOS_POR_VOLTA: pulsos do encoder (duas bordas do canal A) em uma volta
//    da RODA. Medido em 11/11/2026 girando a roda 1 volta à mão (3305); marque a
//    roda, gire uma volta à mão e confira com o comando 'e'.
namespace Motores
{
const float TENSAO_MAX_MOTOR_V = 5.0f;  // teto no motor (Eletrônica)
const float TENSAO_ABSOLUTA_MOTOR_V = 6.0f; // máximo do motor N20: nenhum teste passa disso
const float TENSAO_PIOR_CASO_V = 8.4f;  // bateria cheia, se não houver leitura
const float RAMPA_POR_SEGUNDO = 2.0f;   // fração da potência por segundo
const bool INVERTE_ESQUERDO = false;
const bool INVERTE_DIREITO = true;

void iniciar();               // PWM a 20 kHz e driver em sleep (motores soltos)
// Tensão atual da bateria (do INA226). Fora de 6 a 9 V (só USB, sem leitura)
// o motor não liga, e se estiver ligado, para.
void informarTensaoBateria(float volts);
bool bateriaPresente();
float pwmMaximo();            // fração do PWM que dá 5 V no motor agora
// Alvo de -1 (trás) a +1 (frente) em cada roda; 1 = 5 V no motor.
void definir(float esquerdo, float direito);
// Chamar no laço: aplica a rampa e o limite de 5 V.
void atualizar(uint32_t agora_ms);
void frear();                 // as duas entradas em 1: freio ativo
void parar();                 // freia e desliga o driver (SLEEP)
void usarDecaimentoLento(bool lento); // modo do PWM no DRV8833 (padrão: rápido)
bool habilitado();
// Só para teste: muda o teto (limitado a TENSAO_ABSOLUTA_MOTOR_V). Volte para 5 V.
void definirTensaoMaxima(float volts);
float tensaoMaxima();
void definirFrequencia(uint32_t hz); // PWM (padrão 20 kHz)
// SÓ PARA O TESTE COM TRAVA POR VELOCIDADE: PWM direto (0 a 1), sem o limite
// de 5 V e sem rampa. Quem chama precisa vigiar o encoder e parar.
void pwmBruto(float esquerdo, float direito);
float pwmEsquerdo();          // PWM aplicado agora, -1 a 1
float pwmDireito();
} // namespace Motores

namespace Encoders
{
const bool INVERTE_ESQUERDO = true;
const bool INVERTE_DIREITO = false;
const float PULSOS_POR_VOLTA = 3305.0f; // medido: 1 volta da roda à mão (11/11/2026)
const float DIAMETRO_RODA_MM = 34.0f;   // roda N20 de 34 mm (docs/4.1)

void iniciar();
void zerar();
int32_t esquerdo();
int32_t direito();
// Chamar no laço: recalcula a velocidade a cada 100 ms.
void atualizar(uint32_t agora_ms);
float pulsosPorSegundoEsquerda();
float pulsosPorSegundoDireita();
float velocidadeMs(); // média das duas rodas, m/s
} // namespace Encoders
