#include "log_erros.h"

#include <Arduino.h>
#include <stdarg.h>

#include "console.h"

namespace
{
struct Registro
{
    uint32_t ms;
    bool fatal;
    char texto[60];
};
const uint8_t TAMANHO = 32;
Registro registros[TAMANHO];
uint8_t proximo = 0, quantidade = 0;
uint16_t contadorFatais = 0;
} // namespace

namespace LogErros
{
void registrar(bool fatal, const char *formato, ...)
{
    Registro &r = registros[proximo];
    r.ms = millis();
    r.fatal = fatal;
    va_list args;
    va_start(args, formato);
    vsnprintf(r.texto, sizeof(r.texto), formato, args);
    va_end(args);
    proximo = (uint8_t)((proximo + 1) % TAMANHO);
    if (quantidade < TAMANHO)
        quantidade++;
    if (fatal)
        contadorFatais++;
    console.printf(">>> %s %s\n", fatal ? "[ERRO]" : "[aviso]", r.texto);
}

void mostrar(Print &saida)
{
    saida.printf("--- log de erros (%u registros, %u fatais) ---\n", quantidade, contadorFatais);
    for (uint8_t i = 0; i < quantidade; i++)
    {
        const Registro &r = registros[(proximo + TAMANHO - quantidade + i) % TAMANHO];
        saida.printf("%7.1fs %s %s\n", r.ms / 1000.0f, r.fatal ? "[ERRO] " : "[aviso]", r.texto);
    }
}

uint16_t fatais() { return contadorFatais; }
void limparFatais() { contadorFatais = 0; }
} // namespace LogErros
