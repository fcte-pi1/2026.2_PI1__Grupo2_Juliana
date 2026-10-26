#pragma once

#include <stddef.h>
#include <stdint.h>

// CRC-16/CCITT-FALSE, conforme o Contrato de Telemetria (docs/4.4.7).
uint16_t crc16_ccitt(const uint8_t* dados, size_t n);
