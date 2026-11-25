#pragma once

#include <Print.h>

// Monitor serial pelo Wi-Fi: a bateria e o USB não podem ficar ligados ao mesmo
// tempo, então com a bateria o console é por TCP na porta 23 (telnet). Tudo o
// que é escrito aqui vai para a serial e para quem estiver conectado; os
// comandos chegam pelos dois.
//
// No computador: python console_wifi.py <IP do robô>  (ou telnet <IP> 23)
class Console : public Print
{
public:
    void iniciarRede(); // chamar quando o Wi-Fi conectar
    void atender();     // chamar no laço: aceita conexão nova
    int ler();          // próximo comando (serial ou rede), -1 se nada
    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buf, size_t n) override;
};

extern Console console;
