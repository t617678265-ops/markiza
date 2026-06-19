#include "motor.h"
#include "encoder.h"   // Подключаем для живого реверса аппаратного PCNT
#include "calibrator.h" // Подключаем калибратор памяти для фиксации упоров
#include "protection.h" // Подключаем для сброса слепой зоны при реверсе
#include <Arduino.h>
#include <Preferences.h> // ИНТЕГРАЦИЯ: Прямое чтение параметров из Flash перед пуском

// ИНТЕГРАЦИЯ: Импортируем живые переменные для модулей MOTOR и PROTECTION
extern int config_pwm_speed;
extern float config_protection_sens; // ИСПРАВЛЕНО: Связываем глобальную переменную защиты
extern int config_abs_stop_adc;     // ИСПРАВЛЕНО: Связываем глобальную переменную аварийного АЦП
extern int config_motor_inv;         // ИНТЕГРАЦИЯ: Импортируем флаг инверсии

// Новые свободные GPIO платы Wemos D1 Mini ESP32
#define BASE_IN1_HIGH 16  // Upper left (PWM)
#define BASE_IN2_HIGH 17  // Upper right (PWM)
#define BASE_IN1_LOW  18  // Lower left (Digital)
#define BASE_IN2_LOW  19  // Lower right (Digital)

// Локальные переменные пинов для реализации динамической инверсии
static int pin_in1_high = BASE_IN1_HIGH;
static int pin_in2_high = BASE_IN2_HIGH;
static int pin_in1_low  = BASE_IN1_LOW;
static int pin_in2_low  = BASE_IN2_LOW;

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
    // ИСПРАВЛЕНО: Теперь при старте вычитываем абсолютно все параметры железа из Flash, а не только ШИМ
    Preferences hw_prefs;
    hw_prefs.begin("hw_cfg", true); // Режим ReadOnly
    config_pwm_speed = hw_prefs.getInt("pwm", 128);
    config_protection_sens = hw_prefs.getFloat("sens", 5.0f); // Восстановлено чтение float
    config_abs_stop_adc = hw_prefs.getInt("stop", 1500);      // Восстановлено чтение int
    config_motor_inv = hw_prefs.getInt("inv", 0);
    hw_prefs.end();

    // АППАРАТНЫЙ СБРОС: Освобождаем порты в GPIO Matrix перед новой привязкой ШИМ
    ledcDetachPin(pin_in1_high);
    ledcDetachPin(pin_in2_high);

    // Программная инверсия пинов на основе свежепрочитанного флага
    if (config_motor_inv == 1) {
        pin_in1_high = BASE_IN2_HIGH; // Меняем местами ШИМ-пины 16 и 17
        pin_in2_high = BASE_IN1_HIGH;
        pin_in1_low  = BASE_IN2_LOW;  // Меняем местами цифровые пины 18 и 19
        pin_in2_low  = BASE_IN1_LOW;
        Serial.println("[DRV] Аппаратная инверсия мотора АКТИВИРОВАНА в коде");
    } else {
        pin_in1_high = BASE_IN1_HIGH;
        pin_in2_high = BASE_IN2_HIGH;
        pin_in1_low  = BASE_IN1_LOW;
        pin_in2_low  = BASE_IN2_LOW;
        Serial.println("[DRV] Аппаратная инверсия мотора ОТКЛЮЧЕНА (Прямое вращение)");
    }

    // Настройка нижних ключей через динамические переменные пинов
    pinMode(pin_in1_low, OUTPUT);
    pinMode(pin_in2_low, OUTPUT);
    digitalWrite(pin_in1_low, LOW);
    digitalWrite(pin_in2_low, LOW);

    // Конфигурация ШИМ-каналов для классического ядра ESP32 2.х
    ledcSetup(CH_IN1_HIGH, PWM_FREQ, PWM_RES);
    ledcSetup(CH_IN2_HIGH, PWM_FREQ, PWM_RES);

    ledcAttachPin(pin_in1_high, CH_IN1_HIGH);
    ledcAttachPin(pin_in2_high, CH_IN2_HIGH);

    ledcWrite(CH_IN1_HIGH, 0);
    ledcWrite(CH_IN2_HIGH, 0);
}

void motor_tick() {
    // Безопасно вычитываем значение аппаратного регистра PCNT в цикле Ядра 1
    encoder_update_count();

    // --- 1. ОБРАБОТКА НЕБЛОКИРУЮЩЕЙ ПАУЗЫ БЕЗОПАСНОГО РЕВЕРСА (1000 мс) ---
    if (is_reverse_paused) {
        // Удерживаем ключи в жестком силовом нулю во время паузы
        ledcWrite(CH_IN1_HIGH, 0);
        ledcWrite(CH_IN2_HIGH, 0);
        digitalWrite(pin_in1_low, LOW);
        digitalWrite(pin_in2_low, LOW);

        if (millis() - reverse_pause_timer >= 1000) {
            is_reverse_paused = false;
            
            // Сбрасываем фильтры пускового тока для нового направления
            protection_init();
            
            if (target_motor_state == GOTO_POSITION) {
                if (slider_direction == 1) {
                    encoder_set_direction(1);
                    digitalWrite(pin_in1_low, LOW);
                    digitalWrite(pin_in2_low, HIGH);
                    ledcWrite(CH_IN2_HIGH, 0);
                    ledcWrite(CH_IN1_HIGH, config_pwm_speed); 
                    current_motor_state = GOTO_POSITION;
                }
                else if (slider_direction == -1) {
                    encoder_set_direction(-1);
                    digitalWrite(pin_in2_low, LOW);
                    digitalWrite(pin_in1_low, HIGH);
                    ledcWrite(CH_IN1_HIGH, 0);
                    ledcWrite(CH_IN2_HIGH, config_pwm_speed); 
                    current_motor_state = GOTO_POSITION;
                }
            } else {
                if (target_motor_state == MAN_OPEN) encoder_set_direction(1);
                else if (target_motor_state == MAN_CLOSE) encoder_set_direction(-1);
            }
            
            Serial.println("[DRV] Пауза реверса завершена. Силовые мосты принудительно перезапущены к новой цели.");
        } else {
            return; 
        }
    }
    // --- 2. АВТОМАТИЧЕСКОЕ КООРДИНАТНОЕ ВЕДЕНИЕ ОКНА ПО СЛАЙДЕРУ (GOTO_POSITION) ---
    if (target_motor_state == GOTO_POSITION) {
        if (abs(window_pulses - motor_target_steps) <= 10) {
            ledcWrite(CH_IN1_HIGH, 0);
            ledcWrite(CH_IN2_HIGH, 0);
            digitalWrite(pin_in1_low, LOW);
            digitalWrite(pin_in2_low, LOW);
            
            slider_direction = 0;
            current_motor_state = MAN_STOP;
            target_motor_state = MAN_STOP;
            
            Serial.printf("[DRV] Целевая координата слайдера достигнута: %ld (Цель: %ld). Остановка.\n", 
                          window_pulses, motor_target_steps);
            
            calibrator_save_current_position();
            return;
        } 
        
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
                
                protection_init();
                
                encoder_set_direction(1);
                digitalWrite(pin_in1_low, LOW);
                digitalWrite(pin_in2_low, HIGH);
                ledcWrite(CH_IN2_HIGH, 0);
                ledcWrite(CH_IN1_HIGH, config_pwm_speed); 
                
                slider_direction = 1;
                current_motor_state = GOTO_POSITION; 
                Serial.printf("[DRV] Авто-слайдер: старт движения ВВЕРХ к %ld шагам\n", motor_target_steps);
            }
        }
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

                protection_init();
                
                encoder_set_direction(-1);
                digitalWrite(pin_in2_low, LOW);
                digitalWrite(pin_in1_low, HIGH);
                ledcWrite(CH_IN1_HIGH, 0);
                ledcWrite(CH_IN2_HIGH, config_pwm_speed); 
                
                slider_direction = -1;
                current_motor_state = GOTO_POSITION; 
                Serial.printf("[DRV] Авто-слайдер: старт движения ВНИЗ к %ld шагам\n", motor_target_steps);
            }
        }
        return; 
    }

    if (target_motor_state == MAN_CLOSE && is_window_fully_closed) {
        target_motor_state = MAN_STOP;
        Serial.printf("[DRV] Блокировка пуска: окно уже находится в раме низа (%ld шагов)!\n", config_pos_closed);
    }
    if (target_motor_state == MAN_OPEN && is_window_fully_open) {
        target_motor_state = MAN_STOP;
        Serial.printf("[DRV] Блокировка пуска: окно уже находится в упоре верха (%ld шагов)!\n", config_pos_opened);
    }

    if (target_motor_state == MAN_STOP) {
        ledcWrite(CH_IN1_HIGH, 0);
        ledcWrite(CH_IN2_HIGH, 0);
        digitalWrite(pin_in1_low, LOW);
        digitalWrite(pin_in2_low, LOW);
        
        slider_direction = 0;
        
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

    if (target_motor_state != current_motor_state) {
        if (current_motor_state != MAN_STOP && target_motor_state != MAN_STOP) {
            is_reverse_paused = true;
            reverse_pause_timer = millis();
            is_protect_triggered = false; 

            ledcWrite(CH_IN1_HIGH, 0);
            ledcWrite(CH_IN2_HIGH, 0);
            digitalWrite(pin_in1_low, LOW);
            digitalWrite(pin_in2_low, LOW);
            
            current_motor_state = MAN_STOP; 
            Serial.println("[DRV] ВНИМАНИЕ: Обнаружен ручной реверс на лету! Логика PCNT заморожена...");
            return;
        }

        ledcWrite(CH_IN1_HIGH, 0);
        ledcWrite(CH_IN2_HIGH, 0);
        digitalWrite(pin_in1_low, LOW);
        digitalWrite(pin_in2_low, LOW);
        delayMicroseconds(10); 

        switch (target_motor_state) {
            case MAN_OPEN:
                encoder_set_direction(1); 
                digitalWrite(pin_in1_low, LOW);
                digitalWrite(pin_in2_low, HIGH);
                ledcWrite(CH_IN2_HIGH, 0);
                ledcWrite(CH_IN1_HIGH, config_pwm_speed); 
                Serial.printf("[DRV] Аппаратный PCNT-ШИМ: ОТКРЫТИЕ (ШИМ: %d)\n", config_pwm_speed);
                break;

            case MAN_CLOSE:
                encoder_set_direction(-1); 
                digitalWrite(pin_in2_low, LOW);
                digitalWrite(pin_in1_low, HIGH);
                ledcWrite(CH_IN1_HIGH, 0);
                ledcWrite(CH_IN2_HIGH, config_pwm_speed); 
                Serial.printf("[DRV] Аппаратный PCNT-ШИМ: ЗАКРЫТИЕ (ШИМ: %d)\n", config_pwm_speed);
                break;
                
            default:
                break;
        }

        current_motor_state = target_motor_state;
    }
}
