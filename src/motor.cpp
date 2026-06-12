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

// Внутренний маркер направления движения ИМЕННО для режима слайдера
 int slider_direction = 0; // 0 - стоп, 1 - едем вверх, -1 - едем вниз

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
        // Удерживаем ключи в жестком силовом книге во время паузы
        ledcWrite(CH_IN1_HIGH, 0);
        ledcWrite(CH_IN2_HIGH, 0);
        digitalWrite(PIN_IN1_LOW, LOW);
        digitalWrite(PIN_IN2_LOW, LOW);

        if (millis() - reverse_pause_timer >= 1000) {
            is_reverse_paused = false;
            
            // Сбрасываем фильтры пускового тока для нового направления
            protection_init();
            
            // Направление для слайдера или ручного режима задается здесь
            if (target_motor_state == GOTO_POSITION) {
                if (slider_direction == 1) encoder_set_direction(1);
                else if (slider_direction == -1) encoder_set_direction(-1);
            } else {
                if (target_motor_state == MAN_OPEN) encoder_set_direction(1);
                else if (target_motor_state == MAN_CLOSE) encoder_set_direction(-1);
            }
            
            Serial.println("[DRV] Пауза реверса завершена. Вал замер. Направление PCNT обновлено.");
        } else {
            return; 
        }
    }

    // --- 2. АВТОМАТИЧЕСКОЕ КООРДИНАТНОЕ ВЕДЕНИЕ ОКНА ПО СЛАЙДЕРУ (GOTO_POSITION) ---
    if (target_motor_state == GOTO_POSITION) {
        // Защитный коридор упреждения останова в 10 шагов для гашения инерции редуктора
        if (abs(window_pulses - motor_target_steps) <= 10) {
            // Цель достигнута, полностью обесточиваем мост
            ledcWrite(CH_IN1_HIGH, 0);
            ledcWrite(CH_IN2_HIGH, 0);
            digitalWrite(PIN_IN1_LOW, LOW);
            digitalWrite(PIN_IN2_LOW, LOW);
            
            slider_direction = 0;
            current_motor_state = MAN_STOP;
            target_motor_state = MAN_STOP;
            
            Serial.printf("[DRV] Целевая координата слайдера достигнута: %ld (Цель: %ld). Остановка.\n", 
                          window_pulses, motor_target_steps);
            
            // Фиксируем финальную точку в EEPROM
            calibrator_save_current_position();
            return;
        } 
        
        // Окно ниже цели — нужно ехать ВВЕРХ
        if (window_pulses < motor_target_steps) {
            if (slider_direction != 1 && !is_reverse_paused) {
                if (slider_direction == -1 || current_motor_state == MAN_CLOSE) {
                    is_reverse_paused = true;
                    reverse_pause_timer = millis();
                    is_protect_triggered = false;
                    slider_direction = 1; 
                    current_motor_state = MAN_STOP;
                    Serial.println("[DRV] Слайдер запросил реверс движения ВВЕРХ. Включаю паузу 1 сек...");
                    return;
                }
                
                // ВАЖНОЕ ИСПРАВЛЕНИЕ: взводим 100 мс слепой зоны для слайдера перед стартом!
                protection_init();
                
                encoder_set_direction(1);
                digitalWrite(PIN_IN1_LOW, LOW);
                digitalWrite(PIN_IN2_LOW, HIGH);
                ledcWrite(CH_IN2_HIGH, 0);
                ledcWrite(CH_IN1_HIGH, 128); // 50% мощности
                
                slider_direction = 1;
                current_motor_state = GOTO_POSITION; 
                Serial.printf("[DRV] Авто-слайдер: старт движения ВВЕРХ к %ld шагам\n", motor_target_steps);
            }
        } 
        // Окно выше цели — нужно ехать ВНИЗ
        else if (window_pulses > motor_target_steps) {
            if (slider_direction != -1 && !is_reverse_paused) {
                if (slider_direction == 1 || current_motor_state == MAN_OPEN) {
                    is_reverse_paused = true;
                    reverse_pause_timer = millis();
                    is_protect_triggered = false;
                    slider_direction = -1; 
                    current_motor_state = MAN_STOP;
                    Serial.println("[DRV] Слайдер запросил реверс движения ВНИЗ. Включаю паузу 1 сек...");
                    return;
                }
                
                // ВАЖНОЕ ИСПРАВЛЕНИЕ: взводим 100 мс слепой зоны для слайдера перед стартом!
                protection_init();
                
                encoder_set_direction(-1);
                digitalWrite(PIN_IN2_LOW, LOW);
                digitalWrite(PIN_IN1_LOW, HIGH);
                ledcWrite(CH_IN1_HIGH, 0);
                ledcWrite(CH_IN2_HIGH, 128); // 50% мощности
                
                slider_direction = -1;
                current_motor_state = GOTO_POSITION; 
                Serial.printf("[DRV] Авто-слайдер: старт движения ВНИЗ к %ld шагам\n", motor_target_steps);
            }
        }
        return; 
    }
     // --- 3. АВТОМАТИЧЕСКАЯ ЗАЩИТА ОТ ПОВТОРНОГО ПУСКА В УПОРЫ ПО ПЛАВАЮЩЕЙ БАЗЕ ---
    if (target_motor_state == MAN_CLOSE && is_window_fully_closed) {
        target_motor_state = MAN_STOP;
        Serial.printf("[DRV] Блокировка пуска: окно уже находится в раме низа (%ld шагов)!\n", config_pos_closed);
    }
    if (target_motor_state == MAN_OPEN && is_window_fully_open) {
        target_motor_state = MAN_STOP;
        Serial.printf("[DRV] Блокировка пуска: окно уже находится в упоре верха (%ld шагов)!\n", config_pos_opened);
    }

    // --- 4. ЖЕСТКАЯ СИЛОВАЯ ОТСЕЧКА ПРИ РУЧНОМ ИЛИ АВАРИЙНОМ СТОПЕ ---
    if (target_motor_state == MAN_STOP) {
        ledcWrite(CH_IN1_HIGH, 0);
        ledcWrite(CH_IN2_HIGH, 0);
        digitalWrite(PIN_IN1_LOW, LOW);
        digitalWrite(PIN_IN2_LOW, LOW);
        
        slider_direction = 0; // Очищаем маркер слайдера при любом стопе
        
        // Перехват перехода в состояние останова
        if (current_motor_state != MAN_STOP) {
            Serial.println("[DRV] Аппаратный PCNT-ШИМ: МОТОР ОСТАНОВЛЕН");
            
            if (is_protect_triggered) {
                calibrator_check_stop_event(current_motor_state);
                is_protect_triggered = false; 
            } else {
                Serial.println("[DRV] Ручной останов пользователем с сайта. Сохранение позиции...");
                calibrator_save_current_position();
            }
            
            current_motor_state = MAN_STOP;
        }
        return;
    }

    // --- 5. ОБРАБОТКА ЗАПУСКА РУЧНОГО ДВИЖЕНИЯ ПРИ СМЕНЕ РЕЖИМОВ (ОТ КНОПОК) ---
    if (target_motor_state != current_motor_state) {

        // ДЕТЕКЦИЯ ПРЯМОГО РЕВЕРСА НА ЛЕТУ ПРИ НАЖАТИИ РУЧНЫХ КНОПОК С САЙТА
        if (current_motor_state != MAN_STOP && target_motor_state != MAN_STOP) {
            is_reverse_paused = true;
            reverse_pause_timer = millis();
            
            is_protect_triggered = false; 

            ledcWrite(CH_IN1_HIGH, 0);
            ledcWrite(CH_IN2_HIGH, 0);
            digitalWrite(PIN_IN1_LOW, LOW);
            digitalWrite(PIN_IN2_LOW, LOW);
            
            current_motor_state = MAN_STOP; 
            Serial.println("[DRV] ВНИМАНИЕ: Обнаружен ручной реверс на лету! Логика PCNT заморожена на время выбега ротора...");
            return;
        }

        // Чистый старт из глубокого стопа от ручных кнопок
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