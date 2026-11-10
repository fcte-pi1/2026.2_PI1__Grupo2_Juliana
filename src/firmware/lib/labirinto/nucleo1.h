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
// IRAM_ATTR dá uma seção por função. Não use em construtor ou destrutor
// inline de header: o compilador gera duas cópias com apelido e recusa
// seções diferentes ("section of alias must match").
#define NUCLEO1 IRAM_ATTR
#define NUCLEO1_DADOS DRAM_ATTR
#else
#define NUCLEO1
#define NUCLEO1_DADOS
#endif
