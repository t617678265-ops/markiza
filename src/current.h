#pragma once
#include <Arduino.h>

// Физический аналоговый пин шунта на плате Wemos D1 ESP32 (Вход ADC1_CH6)
#define PIN_SHUNT 34

void current_init();
int current_get_raw();
