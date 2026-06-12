#include "motor.h"
#include "encoder.h"   // Подключаем для живого реверса аппаратного PCNT
#include "calibrator.h" // Подключаем калибратор памяти для фиксации упоров
#include "protection.h" // Подключаем для сброса слепой зоны при реверсе
#include <Arduino.h>

// Назначаем новые свободные GPIO платы Wemos D1 Mini ESP32
#define PIN_IN1_HIGH 16  // Upper left (PWM)
#define PIN_IN2_HIGH 17  // Upper right (PWM)
#define PIN_IN1_LOW  18  // Lower left (Digital)
#define PIN_IN2_LOW  19  // Lower right (Digital)

// Параметры ШИМ LEDC под классический двухъядерный чип ESP32
#define PWM_FREQ      25000  // Частота 25 кГц
#define PWM_RES       8      // 8 бит (разрешение 0 - 255)
#define CH_IN1_HIGH   0      
#define CH_IN2_HIGH   1      

// Импортируем флаг из protection.cpp для фильтрации ложных калибровок от кнопки СТОП
extern bool is_protect_triggered;

// Импортируем новые плавающие координаты границ из calibrator.cpp
extern long config_pos_closed;
extern long config_pos_opened;

// Импортируем глобальные флаги блокировки из calibrator.cpp
extern bool is_window_fully_closed;
extern bool is_window_fully_open;

MotorControlState target_motor_state = MAN_STOP;
static MotorControlState current_motor_state = MAN_STOP;

// Переменные для неблокирующей задержки безопасного реверса
static unsigned long reverse_pause_timer = 0;
static bool is_reverse_paused = false;

void motor_init() {
    // Настройка нижних ключей
    pinMode(PIN_IN1_LOW, OUTPUT);
    pinMode(PIN_IN2_LOW, OUTPUT);
    digitalWrite(PIN_IN1_LOW, LOW);
    digitalWrite(PIN_IN2_LOW, LOW);

    // Конфигурация ШИМ-каналов для классического ядра ESP32 2.х
    ledcSetup(CH_IN1_HIGH, PWM_FREQ, PWM_RES);
    ledcSetup(CH_IN2_HIGH, PWM_FREQ, PWM_RES);

    ledcAttachPin(PIN_IN1_HIGH, CH_IN1_HIGH);
    ledcAttachPin(PIN_IN2_HIGH, CH_IN2_HIGH);

    ledcWrite(CH_IN1_HIGH, 0);
    ledcWrite(CH_IN2_HIGH, 0);
}

void motor_tick() {
    // Безопасно вычитываем значение аппаратного регистра PCNT в цикле Ядра 1
    encoder_update_count();

    // --- 1. ОБРАБОТКА НЕБЛОКИРУЮЩЕЙ ПАУЗЫ БЕЗОПАСНОГО РЕВЕРСА (1000 мс) ---
    if (is_reverse_paused) {
        // Удерживаем ключи в жестком силовом нуле во время паузы
        ledcWrite(CH_IN1_HIGH, 0);
        ledcWrite(CH_IN2_HIGH, 0);
        digitalWrite(PIN_IN1_LOW, LOW);
        digitalWrite(PIN_IN2_LOW, LOW);

        if (millis() - reverse_pause_timer >= 1000) {
            is_reverse_paused = false;
            
            // Сбрасываем фильтры пускового тока для нового направления
            protection_init();
            
            // Переключаем кремний PCNT на новое направление строго ПОСЛЕ полной остановки вала!
            if (target_motor_state == MAN_OPEN) {
                encoder_set_direction(1);
            } else if (target_motor_state == MAN_CLOSE) {
                encoder_set_direction(-1);
            }
            
            Serial.println("[DRV] Пауза реверса завершена. Вал замер. Направление PCNT обновлено.");
        } else {
            return; 
        }
    }

    // --- 2. АВТОМАТИЧЕСКАЯ ЗАЩИТА ОТ ПОВТОРНОГО ПУСКА В УПОРЫ ПО ПЛАВАЮЩЕЙ БАЗЕ ---
    if (target_motor_state == MAN_CLOSE && is_window_fully_closed) {
        target_motor_state = MAN_STOP;
        Serial.printf("[DRV] Блокировка пуска: окно уже находится в раме низа (%ld шагов)!\n", config_pos_closed);
    }
    if (target_motor_state == MAN_OPEN && is_window_fully_open) {
        target_motor_state = MAN_STOP;
        Serial.printf("[DRV] Блокировка пуска: окно уже находится в упоре верха (%ld шагов)!\n", config_pos_opened);
    }

    // --- 3. ЖЕСТКАЯ СИЛОВАЯ ОТСЕЧКА ПРИ СТОПЕ ---
    if (target_motor_state == MAN_STOP) {
        ledcWrite(CH_IN1_HIGH, 0);
        ledcWrite(CH_IN2_HIGH, 0);
        digitalWrite(PIN_IN1_LOW, LOW);
        digitalWrite(PIN_IN2_LOW, LOW);
        
        // Перехват перехода в состояние останова
        if (current_motor_state != MAN_STOP) {
            Serial.println("[DRV] Аппаратный PCNT-ШИМ: МОТОР ОСТАНОВЛЕН");
            
            if (is_protect_triggered) {
                calibrator_check_stop_event(current_motor_state);
                is_protect_triggered = false; 
            } else {
                Serial.println("[DRV] Ручной останов пользователем. Перезапись памяти заблокирована.");
                
                // Фиксируем текущую позицию во Flash без изменения крайних эталонов
                calibrator_save_current_position();
            }
            
            current_motor_state = MAN_STOP;
        }
        return;
    }

    // --- 4. ОБРАБОТКА ЗАПУСКА ДВИЖЕНИЯ ПРИ СМЕНЕ РЕЖИМОВ ---
    if (target_motor_state != current_motor_state) {

        // ДЕТЕКЦИЯ ПРЯМОГО РЕВЕРСА НА ЛЕТУ
        if (current_motor_state != MAN_STOP && target_motor_state != MAN_STOP) {
            is_reverse_paused = true;
            reverse_pause_timer = millis();
            
            is_protect_triggered = false; 

            ledcWrite(CH_IN1_HIGH, 0);
            ledcWrite(CH_IN2_HIGH, 0);
            digitalWrite(PIN_IN1_LOW, LOW);
            digitalWrite(PIN_IN2_LOW, LOW);
            
            current_motor_state = MAN_STOP; 
            Serial.println("[DRV] ВНИМАНИЕ: Обнаружен реверс на лету! Логика PCNT заморожена на время выбега ротора...");
            return;
        }

        // Чистый старт из глубокого стопа
        ledcWrite(CH_IN1_HIGH, 0);
        ledcWrite(CH_IN2_HIGH, 0);
        digitalWrite(PIN_IN1_LOW, LOW);
        digitalWrite(PIN_IN2_LOW, LOW);
        delayMicroseconds(10); 

        switch (target_motor_state) {
            case MAN_OPEN:
                encoder_set_direction(1); 
                digitalWrite(PIN_IN2_LOW, HIGH);
                ledcWrite(CH_IN1_HIGH, 128); 
                Serial.println("[DRV] Аппаратный PCNT-ШИМ: ОТКРЫТИЕ (50%)");
                break;

            case MAN_CLOSE:
                encoder_set_direction(-1); 
                digitalWrite(PIN_IN1_LOW, HIGH);
                ledcWrite(CH_IN2_HIGH, 128); 
                Serial.println("[DRV] Аппаратный PCNT-ШИМ: ЗАКРЫТИЕ (50%)");
                break;
                
            default:
                break;
        }

        current_motor_state = target_motor_state;
    }
}
