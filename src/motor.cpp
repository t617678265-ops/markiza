#include "motor.h"

// Привязка пинов к нашей карте под FAN7380
#define PIN_HIN1 D5  // Верхний левый ключ (ШИМ)
#define PIN_HIN2 D6  // Верхний правый ключ (ШИМ)
#define PIN_LIN1 D1  // Нижний левый ключ (Цифровой)
#define PIN_LIN2 D2  // Нижний правый ключ (Цифровой)

// Фиксированная мощность ШИМ (25% по твоему заданию для бутстрепа)
const int MOTOR_PWM_SPEED = 25; 

// Глобальная переменная статуса, изначально в СТОП
MotorControlState target_motor_state = MAN_STOP;

// Внутренний флаг для исключения спама переключений в цикле loop
static MotorControlState last_motor_state = MAN_STOP;

void motor_init() {
    pinMode(PIN_LIN1, OUTPUT);
    pinMode(PIN_LIN2, OUTPUT);
    pinMode(PIN_HIN1, OUTPUT);
    pinMode(PIN_HIN2, OUTPUT);

    // БЕЗОПАСНАЯ НАСТРОЙКА ЧАСТОТЫ ШИМ ДЛЯ ЯДРА ESP8266
    analogWriteFreq(20000); // 20 кГц — строго за порогом человеческого слуха, без писка
    analogWriteRange(100);  // Шкала ШИМ теперь ровно от 0 до 100%

    // Намертво глушим все 4 ключа при старте платы
    digitalWrite(PIN_LIN1, LOW);
    digitalWrite(PIN_LIN2, LOW);
    analogWrite(PIN_HIN1, 0);
    analogWrite(PIN_HIN2, 0);
}

void motor_tick() {
    // Если состояние кнопок на телефоне не менялось — ничего не делаем
    if (target_motor_state == last_motor_state) return;

    // Фиксируем новое состояние, чтобы выполнить коммутацию ключей строго один раз
    last_motor_state = target_motor_state;

    if (target_motor_state == MAN_OPEN) {
        // 1. Сначала намертво гасим всё противоположное плечо (Программный Dead Time)
        analogWrite(PIN_HIN2, 0);
        digitalWrite(PIN_LIN1, LOW);
        delayMicroseconds(50); // Пауза, чтобы транзисторы правого плеча железно закрылись

        // 2. Включаем ручной ход на ОТКРЫТИЕ
        digitalWrite(PIN_LIN2, HIGH);               // Нижний правый ключ открыт на 100%
        analogWrite(PIN_HIN1, MOTOR_PWM_SPEED);     // На верхний левый выдаем честный ШИМ 25% (20 кГц)
        Serial.println("[LOOP_DRV] Честный ШИМ: ОТКРЫТИЕ (25%)");
    } 
    else if (target_motor_state == MAN_CLOSE) {
        // 1. Сначала намертво гасим всё противоположное плечо (Программный Dead Time)
        analogWrite(PIN_HIN1, 0);
        digitalWrite(PIN_LIN2, LOW);
        delayMicroseconds(50); // Пауза, чтобы транзисторы левого плеча железно закрылись

        // 2. Включаем ручной ход на ЗАКРЫТИЕ
        digitalWrite(PIN_LIN1, HIGH);               // Нижний левый ключ открыт на 100%
        analogWrite(PIN_HIN2, MOTOR_PWM_SPEED);     // На верхний правый выдаем честный ШИМ 25% (20 кГц)
        Serial.println("[LOOP_DRV] Честный ШИМ: ЗАКРЫТИЕ (25%)");
    } 
    else {
        // Если пришла команда СТОП — сажаем все входы драйверов FAN7380 в глухой ноль
        digitalWrite(PIN_LIN1, LOW);
        digitalWrite(PIN_LIN2, LOW);
        analogWrite(PIN_HIN1, 0);
        analogWrite(PIN_HIN2, 0);
        Serial.println("[LOOP_DRV] Честный ШИМ: СТОП");
    }
}
