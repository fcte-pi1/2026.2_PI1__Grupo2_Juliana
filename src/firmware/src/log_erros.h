#pragma once

#include <Print.h>
#include <stdint.h>

// Log de erros do robô (#213, RF20-S): os últimos 32 registros ficam na RAM e
// cada um aparece na hora no console (serial e Wi-Fi). Comando 'l' lista tudo.
// O Contrato de Telemetria (4.4.7) não tem evento de erro, então o log não vai
// para o servidor; as falhas que encerram a corrida vão no evento 'fim'.
namespace LogErros
{
void registrar(bool fatal, const char *formato, ...);
void mostrar(Print &saida);
uint16_t fatais();
void limparFatais(); // nova calibração
} // namespace LogErros
