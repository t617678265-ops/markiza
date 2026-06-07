#include <Arduino.h>
#include "wifi_config.h" 
#include "web_server.h"  
#include "motor.h"       

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[SYS] Старт Window Driver...");

    wifi_config_init();
    motor_init(); // Запуск пинов
    web_server_init();

    Serial.println("[SYS] Система полностью готова к работе.");
}

void loop() {
    web_server_update(); // Фоновое обновление mDNS имени
    motor_tick();        // <--- ТУТ крутится безопасное управление ключами моста
}
