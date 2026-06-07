#pragma once
#include <Arduino.h>

// Перечисление для трех состояний ручного хода мотора
enum MotorControlState {
    MAN_STOP,
    MAN_OPEN,
    MAN_CLOSE
};

// Делаем переменную статуса видимой для веб-сервера
extern MotorControlState target_motor_state;

void motor_init();  // Настройка пинов
void motor_tick();  // Силовое переключение ключей в loop() вне прерываний Wi-Fi
