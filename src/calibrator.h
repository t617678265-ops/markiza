#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "motor.h"

// Новые глобальные координаты краев (доступны для слайдера в web_server.cpp и main.cpp)
extern long config_pos_closed; // Обученная координата закрытого окна (0%)
extern long config_pos_opened; // Обученная координата открытого окна (100%)

// Глобальные флаги блокировки пуска в физические упоры рамы
extern bool is_window_fully_closed;
extern bool is_window_fully_open;

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

/**
 * @brief Динамическое обновление и сброс флагов блокировки при выходе из крайних положений.
 *        Вызывается каждую 1 мс внутри задачи ядра физики WindowPhysicsCoreCode.
 */
void calibrator_update_flags();

/**
 * @brief Бережное сохранение текущей координаты window_pulses во Flash-память.
 *        Вызывается в motor.cpp при абсолютно любом физическом останове вала.
 */
void calibrator_save_current_position();
