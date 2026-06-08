#include "motor.h"
#include <Arduino.h>

// Жестко привязываем новые физические GPIO платы ESP32-C3
#define PIN_IN1_HIGH 0  // Верхний левый (ШИМ)
#define PIN_IN2_HIGH 1  // Верхний правый (ШИМ)
#define PIN_IN1_LOW  2  // Нижний левый (Цифра)
#define PIN_IN2_LOW  3  // Нижний правый (Цифра)

// Настройки аппаратного ШИМ-контроллера LEDC под классическое ядро
#define PWM_FREQ      25000  // Частота 25 кГц (ультразвук, полная бесшумность)
#define PWM_RES       8      // Разрядность 8 бит (диапазон мощности от 0 до 255)
#define CH_IN1_HIGH   0      // Железный канал 0 для левого плеча
#define CH_IN2_HIGH   1      // Железный канал 1 для правого плеча

MotorControlState target_motor_state = MAN_STOP;
static MotorControlState current_motor_state = MAN_STOP;

void motor_init() {
    // Настраиваем нижние ключи как обычные цифровые выходы
    pinMode(PIN_IN1_LOW, OUTPUT);
    pinMode(PIN_IN2_LOW, OUTPUT);
    digitalWrite(PIN_IN1_LOW, LOW);
    digitalWrite(PIN_IN2_LOW, LOW);

    // КЛАССИЧЕСКИЙ СИНТАКСИС ESP32: Сначала настраиваем каналы, потом крепим к ним ноги
    ledcSetup(CH_IN1_HIGH, PWM_FREQ, PWM_RES);
    ledcSetup(CH_IN2_HIGH, PWM_FREQ, PWM_RES);

    ledcAttachPin(PIN_IN1_HIGH, CH_IN1_HIGH);
    ledcAttachPin(PIN_IN2_HIGH, CH_IN2_HIGH);

    // Глушим ШИМ при старте (запись нуля в каналы)
    ledcWrite(CH_IN1_HIGH, 0);
    ledcWrite(CH_IN2_HIGH, 0);
}

void motor_tick() {
    if (target_motor_state == current_motor_state) return;

    // Сброс всех выходов в безопасный ноль перед переключением направлений
    ledcWrite(CH_IN1_HIGH, 0);
    ledcWrite(CH_IN2_HIGH, 0);
    digitalWrite(PIN_IN1_LOW, LOW);
    digitalWrite(PIN_IN2_LOW, LOW);
    delayMicroseconds(10); // Железная пауза против сквозного тока

    switch (target_motor_state) {
        case MAN_OPEN:
            // Открытие: ШИМ на верхний левый канал, открываем нижний правый пин
            digitalWrite(PIN_IN2_LOW, HIGH);
            ledcWrite(CH_IN1_HIGH, 64); // Скважность 25% от диапазона 255 (мягкий ход)
            Serial.println("[DRV] Аппаратный ШИМ: ОТКРЫТИЕ (25%)");
            break;

        case MAN_CLOSE:
            // Закрытие: ШИМ на верхний правый канал, открываем нижний левый пин
            digitalWrite(PIN_IN1_LOW, HIGH);
            ledcWrite(CH_IN2_HIGH, 64); // Скважность 25% от диапазона 255
            Serial.println("[DRV] Аппаратный ШИМ: ЗАКРЫТИЕ (25%)");
            break;

        case MAN_STOP:
            Serial.println("[DRV] Аппаратный ШИМ: МОТОР ОСТАНОВЛЕН");
            break;
    }

    current_motor_state = target_motor_state;
}
