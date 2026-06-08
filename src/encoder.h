#pragma once
#include <Arduino.h>

// Делаем переменные и функции видимыми для других файлов проекта
extern volatile long window_pulses;
extern int pulse_direction;

void encoder_init();
void encoder_set_direction(int dir);
