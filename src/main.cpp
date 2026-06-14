#include <Arduino.h>
#include <EEPROM.h>      // Подключаем для очистки памяти Wi-Fi при сбросе кнопки
#include <WiFi.h>        // Подключаем для проверки статуса соединения в Watchdog
#include "wifi_config.h" 
#include "web_server.h"  
#include "motor.h"       
#include "encoder.h"     
#include "current.h"     
#include "protection.h"  // Подключаем изолированный модуль защиты
#include "calibrator.h"  // Подключаем модуль калибровки памяти

extern volatile long window_pulses;

// Хэндл фоновой задачи для изоляции физики мотора
TaskHandle_t WindowPhysicsTask;

// Выполнение кода на ЯДРЕ 1 (Монопольный контроль привода и энкодера)
void WindowPhysicsCoreCode(void * pvParameters) {
    Serial.printf("[CORE] Задача физики привода запущена на ядре: %d\n", xPortGetCoreID());
    
    // Инициализируем таймеры модуля защиты перед входом в цикл опроса
    protection_init();
    
    for(;;) {
        // Шаг 1: Сначала опрашиваем мотор и обновляем счетчик импульсов энкодера
        motor_tick(); 
        
        // Шаг 2: Теперь проверяем заклинивание вала на основе свежих данных шагов
        protection_tick(); 
        
        // Шаг 3: Динамически обновляем и сбрасываем флаги блокировок при выходе из упоров
        calibrator_update_flags();
        
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
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[SYS] Старт Window Driver на двухъядерной архитектуре ESP32...");

    // Инициализация низкоуровневого железа привода
    motor_init();   
    encoder_init(); 
    current_init(); 

    // Настройка физической кнопки сброса Wi-Fi на свободный пин GPIO 22
    pinMode(22, INPUT_PULLUP);

    // Считываем сохраненный эталон длины окна из Flash-памяти Preferences
    calibrator_init();

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

    // --- 1. НЕБЛОКИРУЮЩИЙ ОПРОС ФИЗИЧЕСКОЙ КНОПКИ СБРОСА WI-FI НА GPIO 22 ---
    static unsigned long button_press_start = 0;
    static bool is_pressed = false;

    if (digitalRead(22) == LOW) { // Кнопка притянута к минусу (зажата)
        if (!is_pressed) {
            button_press_start = millis();
            is_pressed = true;
            Serial.println("[BUTTON] Обнаружено нажатие кнопки сброса. Начинаю отсчет 5 секунд...");
        }
        
        // Защита от случайного клика: проверяем удержание ровно 5000 мс
        if (millis() - button_press_start >= 5000) {
            Serial.println("[BUTTON] Кнопка удержана 5 секунд! Очищаю память Wi-Fi и перезагружаюсь...");
            
            EEPROM.begin(512);
            EEPROM.write(0, 0xFF); // Стираем маркер валидности сети, как в роутере
            EEPROM.commit();
            
            delay(1000);
            ESP.restart(); // Жесткий рестарт платы
        }
    } 
    else { // Кнопка отпущена
        if (is_pressed) {
            Serial.println("[BUTTON] Кнопка отпущена раньше времени. Сброс таймера.");
            is_pressed = false;
        }
    }

    // --- 2. СЕТЕВОЙ СТОРОЖЕВОЙ ТАЙМЕР (WI-FI WATCHDOG) НА ЯДРЕ 0 ---
    static unsigned long wifi_lost_timer = 0;
    static bool is_wifi_lost = false;

    // Проверяем сторожа только если контроллер переведен в режим клиента домашней сети (STA)
    if (WiFi.getMode() == WIFI_STA) {
        if (WiFi.status() != WL_CONNECTED) {
            if (!is_wifi_lost) {
                wifi_lost_timer = millis();
                is_wifi_lost = true;
                Serial.println("[WATCHDOG] Wi-Fi соединение потеряно! Запуск защитного таймера на 5 минут...");
            }
            
            // Если сеть лежит непрерывно более 5 минут (300 000 миллисекунд)
            if (millis() - wifi_lost_timer >= 300000) {
                Serial.println("[WATCHDOG] Сеть не восстановилась за 5 минут. Автоматический тихий рестарт чипа...");
                delay(500);
                ESP.restart(); 
            }
        } 
        else {
            // Если сеть в порядке, сбрасываем сторожа в исходное состояние
            if (is_wifi_lost) {
                Serial.println("[WATCHDOG] Wi-Fi успешно восстановился самостоятельно. Таймер сброшен.");
                is_wifi_lost = false;
            }
        }
    }

    yield();
}
