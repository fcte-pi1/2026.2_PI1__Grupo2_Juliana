#pragma once

// Código que roda no núcleo 1 da ESP32.
//
// A ESP32 da placa tem um bit da cache do núcleo 1 grudado em 0 (docs/7.3,
// testes 3.11 e 3.18): código e constantes lidos da flash por esse núcleo
// podem vir com um bit trocado. Código na RAM interna (IRAM) não passa pela
// cache, então funciona sempre. NUCLEO1 põe a função na IRAM e NUCLEO1_DADOS
// põe uma constante na RAM de dados. No computador (testes nativos) não fazem
// nada.
#if defined(ESP_PLATFORM) || defined(ESP32)
#include <esp_attr.h>
// Nome de seção fixo (o IRAM_ATTR usa um nome por uso, o que quebra os
// construtores e destrutores, gerados em duas cópias com apelido).
// O linker do ESP32 manda .iram1.* para a IRAM.
#define NUCLEO1 __attribute__((section(".iram1.nucleo1")))
#define NUCLEO1_DADOS DRAM_ATTR
#else
#define NUCLEO1
#define NUCLEO1_DADOS
#endif
