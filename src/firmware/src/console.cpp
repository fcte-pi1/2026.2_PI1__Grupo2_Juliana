#include "console.h"

#include <Arduino.h>
#include <WiFi.h>

Console console;

namespace
{
WiFiServer servidor(23);
WiFiClient cliente;
bool rede = false;
} // namespace

void Console::iniciarRede()
{
    if (rede)
        return;
    servidor.begin();
    servidor.setNoDelay(true);
    rede = true;
}

void Console::atender()
{
    if (!rede || !servidor.hasClient())
        return;
    if (cliente && cliente.connected())
        cliente.stop(); // uma pessoa por vez: a nova conexão ganha
    cliente = servidor.available();
    cliente.printf("Micromouse: console pelo Wi-Fi (%s)\r\n", WiFi.localIP().toString().c_str());
}

int Console::ler()
{
    if (Serial.available())
        return Serial.read();
    if (cliente && cliente.connected() && cliente.available())
        return cliente.read();
    return -1;
}

size_t Console::write(uint8_t c) { return write(&c, 1); }

size_t Console::write(const uint8_t *buf, size_t n)
{
    Serial.write(buf, n);
    if (cliente && cliente.connected())
        cliente.write(buf, n);
    return n;
}
