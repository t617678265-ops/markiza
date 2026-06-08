#pragma once
#include <Arduino.h>

// Физический аналоговый пин шунта на плате ESP32-C3
#define PIN_SHUNT 4

void current_init();
int current_get_raw();
