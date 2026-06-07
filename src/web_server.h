#pragma once
#include <Arduino.h>

void web_server_init();   // Запуск сервера в домашней сети
void web_server_update(); // Обновление службы mDNS для loop()
