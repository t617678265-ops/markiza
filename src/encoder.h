#pragma once
#include <Arduino.h>

// Глобальный счётчик импульсов, доступный для чтения всему проекту
// volatile запрещает процессору кэшировать переменную, так как она меняется в прерывании
extern volatile long window_pulses;

void encoder_init();               // Инициализация пина D7 и привязка прерывания
void encoder_set_direction(int dir); // Установка знака счёта: 1 (открытие), -1 (закрытие)
