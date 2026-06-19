#include "motor.h"
#include "encoder.h"   // Подключаем для живого реверса аппаратного PCNT
#include "calibrator.h" // Подключаем калибратор памяти для фиксации упоров
#include "protection.h" // Подключаем для сброса слепой зоны при реверсе
#include <Arduino.h>
#include <Preferences.h> // ИНТЕГРАЦИЯ: Прямое чтение параметров из Flash перед пуском

// ИНТЕГРАЦИЯ: Импортируем живые переменные для модулей MOTOR и PROTECTION
extern int config_pwm_speed;
extern float config_protection_sens; 
extern int config_abs_stop_adc;     
extern int config_motor_inv;         

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

// ИСПРАВЛЕНО: Переменные для реализации НЕБЛОКИРУЮЩЕГО плавного пуска (Soft-Start)
static unsigned long soft_start_timer = 0;
static int current_soft_pwm = 0;

void motor_init() {
    Preferences hw_prefs;
    hw_prefs.begin("hw_cfg", true); // Режим ReadOnly
    config_pwm_speed = hw_prefs.getInt("pwm", 128);
    config_protection_sens = hw_prefs.getFloat("sens", 5.0f); 
    config_abs_stop_adc = hw_prefs.getInt("stop", 1500);      
    config_motor_inv = hw_prefs.getInt("inv", 0);
    hw_prefs.end();

    ledcDetachPin(pin_in1_high);
    ledcDetachPin(pin_in2_high);

    if (config_motor_inv == 1) {
        pin_in1_high = BASE_IN2_HIGH; 
        pin_in2_high = BASE_IN1_HIGH;
        pin_in1_low  = BASE_IN2_LOW;  
        pin_in2_low  = BASE_IN1_LOW;
        Serial.println("[DRV] Аппаратная инверсия мотора АКТИВИРОВАНА в коде");
    } else {
        pin_in1_high = BASE_IN1_HIGH;
        pin_in2_high = BASE_IN2_HIGH;
        pin_in1_low  = BASE_IN1_LOW;
        pin_in2_low  = BASE_IN2_LOW;
        Serial.println("[DRV] Аппаратная инверсия мотора ОТКЛЮЧЕНА (Прямое вращение)");
    }

    pinMode(pin_in1_low, OUTPUT);
    pinMode(pin_in2_low, OUTPUT);
    digitalWrite(pin_in1_low, LOW);
    digitalWrite(pin_in2_low, LOW);

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
        ledcWrite(CH_IN1_HIGH, 0);
        ledcWrite(CH_IN2_HIGH, 0);
        digitalWrite(pin_in1_low, LOW);
        digitalWrite(pin_in2_low, LOW);

        if (millis() - reverse_pause_timer >= 1000) {
            is_reverse_paused = false;
            protection_init();
            
            if (target_motor_state == GOTO_POSITION) {
                soft_start_timer = millis(); // Перезапуск таймера для слайдера
                current_soft_pwm = 0;
                if (slider_direction == 1) {
                    encoder_set_direction(1);
                    digitalWrite(pin_in1_low, LOW);
                    digitalWrite(pin_in2_low, HIGH);
                    ledcWrite(CH_IN2_HIGH, 0);
                    current_motor_state = GOTO_POSITION;
                }
                else if (slider_direction == -1) {
                    encoder_set_direction(-1);
                    digitalWrite(pin_in2_low, LOW);
                    digitalWrite(pin_in1_low, HIGH);
                    ledcWrite(CH_IN1_HIGH, 0);
                    current_motor_state = GOTO_POSITION;
                }
            } else {
                if (target_motor_state == MAN_OPEN) encoder_set_direction(1);
                else if (target_motor_state == MAN_CLOSE) encoder_set_direction(-1);
            }
            Serial.println("[DRV] Пауза реверса завершена. Готовность к плавному пуску.");
        } else {
            return; 
        }
    }

    // --- НЕБЛОКИРУЮЩИЙ АВТОРАЗГОН (SOFT-START) НА КАЖДОМ ТИКЕ ЯДРА ---
    if (current_motor_state != MAN_STOP && current_soft_pwm < config_pwm_speed) {
        if (millis() - soft_start_timer >= 15) { // Приращение каждые 15 мс
            soft_start_timer = millis();
            current_soft_pwm += 10; // Шаг нарастания мощности
            if (current_soft_pwm > config_pwm_speed) current_soft_pwm = config_pwm_speed;

            if (current_motor_state == MAN_OPEN || (current_motor_state == GOTO_POSITION && slider_direction == 1)) {
                ledcWrite(CH_IN1_HIGH, current_soft_pwm);
            } 
            else if (current_motor_state == MAN_CLOSE || (current_motor_state == GOTO_POSITION && slider_direction == -1)) {
                ledcWrite(CH_IN2_HIGH, current_soft_pwm);
            }
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
            current_soft_pwm = 0;
            
            Serial.printf("[DRV] Слайдер: координата достигнута %ld. Стоп.\n", window_pulses);
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
                    return;
                }
                
                protection_init();
                encoder_set_direction(1);
                digitalWrite(pin_in1_low, LOW);
                digitalWrite(pin_in2_low, HIGH);
                ledcWrite(CH_IN2_HIGH, 0);
                
                soft_start_timer = millis(); // Инициализация разгона
                current_soft_pwm = 0;
                ledcWrite(CH_IN1_HIGH, 0);
                
                slider_direction = 1;
                current_motor_state = GOTO_POSITION; 
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
                    return;
                }

                protection_init();
                encoder_set_direction(-1);
                digitalWrite(pin_in2_low, LOW);
                digitalWrite(pin_in1_low, HIGH);
                ledcWrite(CH_IN1_HIGH, 0);
                
                soft_start_timer = millis(); // Инициализация разгона
                current_soft_pwm = 0;
                ledcWrite(CH_IN2_HIGH, 0);
                
                slider_direction = -1;
                current_motor_state = GOTO_POSITION; 
            }
        }
        return; 
    }

    if (target_motor_state == MAN_CLOSE && is_window_fully_closed) target_motor_state = MAN_STOP;
    if (target_motor_state == MAN_OPEN && is_window_fully_open) target_motor_state = MAN_STOP;

    if (target_motor_state == MAN_STOP) {
        ledcWrite(CH_IN1_HIGH, 0);
        ledcWrite(CH_IN2_HIGH, 0);
        digitalWrite(pin_in1_low, LOW);
        digitalWrite(pin_in2_low, LOW);
        slider_direction = 0;
        current_soft_pwm = 0;
        
        if (current_motor_state != MAN_STOP) {
            if (is_protect_triggered) {
                calibrator_check_stop_event(current_motor_state);
                is_protect_triggered = false; 
            } else {
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
            current_motor_state = MAN_STOP; 
            return;
        }

        ledcWrite(CH_IN1_HIGH, 0);
        ledcWrite(CH_IN2_HIGH, 0);
        digitalWrite(pin_in1_low, LOW);
        digitalWrite(pin_in2_low, LOW);
        delayMicroseconds(10); 

        soft_start_timer = millis(); // Запуск таймера разгона для ручного режима
        current_soft_pwm = 0;

        switch (target_motor_state) {
            case MAN_OPEN:
                encoder_set_direction(1); 
                digitalWrite(pin_in1_low, LOW);
                digitalWrite(pin_in2_low, HIGH);
                ledcWrite(CH_IN2_HIGH, 0);
                ledcWrite(CH_IN1_HIGH, 0); 
                break;

            case MAN_CLOSE:
                encoder_set_direction(-1); 
                digitalWrite(pin_in2_low, LOW);
                digitalWrite(pin_in1_low, HIGH);
                ledcWrite(CH_IN1_HIGH, 0);
                ledcWrite(CH_IN2_HIGH, 0); 
                break;
                
            default:
                break;
        }

        current_motor_state = target_motor_state;
    }
}
