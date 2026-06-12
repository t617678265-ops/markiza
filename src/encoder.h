#pragma once
#include <Arduino.h>

// Глобальный счетчик импульсов, обновляемый аппаратным модулем PCNT
extern volatile long window_pulses;

void encoder_init();
void encoder_set_direction(int dir);
void encoder_update_count(); // Делает функцию видимой для motor.cpp

/**
 * @brief Установка фиксированного стартового смещения (офсета) для аппаратного регистра.
 *        Вызывается один раз при старте платы из calibrator.cpp после чтения Flash-памяти.
 * @param offset_val Значение сохраненной координаты, с которой нужно продолжить счет
 */
void encoder_set_offset(long offset_val);
