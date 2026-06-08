#include <Arduino.h>
#include "wifi_config.h" 
#include "web_server.h"  
#include "motor.h"       
#include "encoder.h"     

// Открываем доступ к переменной счетчика
extern volatile long window_pulses;

void setup() {
    // Гасим встроенный выжигающий глаза RGB светодиод на старте
    neopixelWrite(RGB_BUILTIN, 0, 0, 0);

    Serial.begin(115200);
    delay(500);
    Serial.println("\n[SYS] Старт Window Driver...");

    wifi_config_init();
    motor_init();   
    encoder_init(); 
    web_server_init();

    Serial.println("[SYS] Система полностью готова к работе.");
}

void loop() {
    web_server_update(); 
    motor_tick();        

    // БЕЗОПАСНЫЙ ВЫВОД ИМПУЛЬСОВ В СВОБОДНОМ ЦИКЛЕ LOOP
    static unsigned long log_timer = 0;
    if (millis() - log_timer >= 300) {
        log_timer = millis();

        // Выводим инфу в консоль только тогда, когда мотор реально находится в движении
        if (target_motor_state != MAN_STOP) {
            Serial.printf("[PULSE] Мотор крутится. Текущий счёт вала: %ld\n", window_pulses);
        }
    }
}
