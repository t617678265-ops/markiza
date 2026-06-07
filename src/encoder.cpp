#include "encoder.h"

// Наш аппаратный пин датчика Холла
#define PIN_HALL D7  

// Выделяем память под переменные
volatile long window_pulses = 0; 
static volatile int pulse_direction = 1; // По умолчанию знак плюс (на открытие)

// АППАРАТНАЯ ФУНКЦИЯ ПРЕРЫВАНИЯ (вызывается из кремния чипа при каждом импульсе)
// Она полностью игнорирует состояние мотора, позволяя досчитать инерцию после СТОПа
void IRAM_ATTR onHallPulse() {
    window_pulses += pulse_direction; // Крутит счётчик в сторону последнего движения
}

void encoder_init() {
    // Включаем встроенный подтягивающий резистор к 3.3В, чтобы пин не ловил наводки
    pinMode(PIN_HALL, INPUT_PULLUP); 
    
    // Привязываем прерывание: ловим спад сигнала из HIGH в LOW (FALLING)
    attachInterrupt(digitalPinToInterrupt(PIN_HALL), onHallPulse, FALLING);
}

void encoder_set_direction(int dir) {
    // Меняет знак счёта (1 или -1). Вызывается сервером при нажатии кнопок хода.
    if (dir == 1 || dir == -1) {
        pulse_direction = dir;
    }
}
