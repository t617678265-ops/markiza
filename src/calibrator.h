#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "motor.h"

// Глобальные переменные, доступные для слайдера и пресетов в main.cpp и web_server.cpp
extern long max_window_steps; // Полная длина хода окна в шагах энкодера

/**
 * @brief Инициализация калибратора, чтение сохраненных границ из Preferences при старте платы
 */
void calibrator_init();

/**
 * @brief Перехват аварийного останова от модуля защиты и динамическое переобучение крайних точек.
 *        Вызывается в motor.cpp в момент перехода мотора в состояние MAN_STOP.
 * @param last_moving_state Направление, в котором двигался мотор до сработки автостопа (MAN_OPEN или MAN_CLOSE)
 */
void calibrator_check_stop_event(MotorControlState last_moving_state);
