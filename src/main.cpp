#include <Arduino.h>
#include "wifi_config.h" 
#include "web_server.h"  
#include "motor.h"       
#include "encoder.h"     
#include "current.h"     

extern volatile long window_pulses;

// Хэндл фоновой задачи для изоляции физики мотора
TaskHandle_t WindowPhysicsTask;

// Выполнение кода на ЯДРЕ 1 (Монопольный контроль привода и энкодера)
void WindowPhysicsCoreCode(void * pvParameters) {
    Serial.printf("[CORE] Задача физики привода запущена на ядре: %d\n", xPortGetCoreID());
    
    for(;;) {
        motor_tick(); // Циклический опрос мотора, энкодера и защиты по току
        
        static unsigned long log_timer = 0;
        if (millis() - log_timer >= 300) {
            log_timer = millis();
            if (target_motor_state != MAN_STOP) {
                int current_raw = current_get_raw();
                Serial.printf("[PULSE] Шаги вала: %ld | Ток АЦП: %d\n", window_pulses, current_raw);
            }
        }
        
        // Обязательный квант времени FreeRTOS (1 мс) для разгрузки планировщика задач
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}

void setup() {
    // Встроенный RGB светодиод на плате Wemos ESP32 отсутствует, гасить нечего
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[SYS] Старт Window Driver на двухъядерной архитектуре ESP32...");

    // Инициализация низкоуровневого железа привода
    motor_init();   
    encoder_init(); 
    current_init(); 

    // Создаем изолированный поток на ЯДРЕ 1. Приоритет 3 (высокий)
    xTaskCreatePinnedToCore(
        WindowPhysicsCoreCode,   /* Функция, реализующая задачу */
        "WindowPhysics",         /* Имя задачи для системы */
        4096,                    /* Размер стека задачи в байтах */
        NULL,                    /* Параметры, передаваемые в задачу */
        3,                       /* Приоритет задачи */
        &WindowPhysicsTask,      /* Хэндл задачи */
        1                        /* Идентификатор ядра (Core 1) */
    );

    // Запуск сетевого стека. По умолчанию инициализируется на ЯДРЕ 0
    wifi_config_init();
    web_server_init();

    Serial.println("[SYS] Планировщик FreeRTOS запущен. Система готова.");
}

void loop() {
    // Ядро 0 полностью обрабатывает только сетевые обновления веб-сервера
    web_server_update(); 
    yield();
}
