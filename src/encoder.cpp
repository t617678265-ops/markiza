#include "encoder.h"
#include <Arduino.h>

// Физический пин датчика Холла на плате ESP32-C3
#define PIN_HALL 4 

volatile long window_pulses = 0;
int pulse_direction = 1; // 1 - открытие, -1 - закрытие

// Быстрая функция прерывания, выполняемая в оперативной памяти (IRAM)
void IRAM_ATTR onHallPulse() {
    window_pulses += pulse_direction;
}

void encoder_init() {
    // Включаем встроенную подтяжку, чтобы нога не ловила воздух
    pinMode(PIN_HALL, INPUT_PULLUP);
    
    // Привязываем прерывание к спаду уровня (FALLING) на GPIO 4
    attachInterrupt(digitalPinToInterrupt(PIN_HALL), onHallPulse, FALLING);
}

void encoder_set_direction(int dir) {
    pulse_direction = dir;
}
