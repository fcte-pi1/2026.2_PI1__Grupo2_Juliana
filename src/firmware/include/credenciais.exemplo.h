#pragma once

// Copie este arquivo para include/credenciais.h e preencha.
// O credenciais.h fica fora do git (.gitignore): nunca versione a senha.
// Sem o credenciais.h o firmware compila com o Wi-Fi desligado.

#define WIFI_SSID "nome-da-rede"
#define WIFI_SENHA "senha-da-rede"

// IP do computador que roda o backend (uvicorn --host 0.0.0.0 --port 8000).
// O ESP32 e o computador precisam estar na mesma rede.
#define SERVIDOR_HOST "192.168.0.2"
#define SERVIDOR_PORTA 8000

// Senha do OTA (atualização do firmware pelo Wi-Fi, RF22-S).
#define OTA_SENHA "troque-esta-senha"
