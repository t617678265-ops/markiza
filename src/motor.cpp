#include "motor.h"
#include "encoder.h" // Подключаем для живого реверса аппаратного PCNT
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

MotorControlState target_motor_state = MAN_STOP;
static MotorControlState current_motor_state = MAN_STOP;

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

    if (target_motor_state == current_motor_state) return;

    // Сброс всех выходов в безопасный ноль
    ledcWrite(CH_IN1_HIGH, 0);
    ledcWrite(CH_IN2_HIGH, 0);
    digitalWrite(PIN_IN1_LOW, LOW);
    digitalWrite(PIN_IN2_LOW, LOW);
    delayMicroseconds(10); // Аппаратный Dead-time против сквозного тока

    switch (target_motor_state) {
        case MAN_OPEN:
            encoder_set_direction(1); // Переключаем логику кремния PCNT на инкремент
            digitalWrite(PIN_IN2_LOW, HIGH);
            ledcWrite(CH_IN1_HIGH, 128); // 50% мощности
            Serial.println("[DRV] Аппаратный PCNT-ШИМ: ОТКРЫТИЕ (50%)");
            break;

        case MAN_CLOSE:
            encoder_set_direction(-1); // Переключаем логику кремния PCNT на декремент
            digitalWrite(PIN_IN1_LOW, HIGH);
            ledcWrite(CH_IN2_HIGH, 128); // 50% мощности
            Serial.println("[DRV] Аппаратный PCNT-ШИМ: ЗАКРЫТИЕ (50%)");
            break;

        case MAN_STOP:
            Serial.println("[DRV] Аппаратный PCNT-ШИМ: МОТОР ОСТАНОВЛЕН");
            break;
    }

    current_motor_state = target_motor_state;
}
