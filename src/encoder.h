#pragma once
#include <Arduino.h>

// Глобальный счетчик импульсов, обновляемый аппаратным модулем PCNT
extern volatile long window_pulses;

void encoder_init();
void encoder_set_direction(int dir);
void encoder_update_count(); // Делает функцию видимой для motor.cpp
