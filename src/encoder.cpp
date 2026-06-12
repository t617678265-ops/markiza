#include "encoder.h"
#include <Arduino.h>
#include "driver/pcnt.h" // Официальный железный драйвер для классического ESP32

#define PIN_HALL 23 // Назначаем свободный аппаратный GPIO 23 под датчик Холла

volatile long window_pulses = 0;

// Внутренняя переменная для хранения фиксированного стартового смещения из Flash
static long initial_offset = 0;

void encoder_init() {
    pcnt_config_t pcnt_config = {};
    
    // Настройка железных входов модуля PCNT
    pcnt_config.pulse_gpio_num = PIN_HALL;
    pcnt_config.ctrl_gpio_num = PCNT_PIN_NOT_USED; // Направление реверса контролируем программно
    pcnt_config.channel = PCNT_CHANNEL_0;
    pcnt_config.unit = PCNT_UNIT_0;
    
    // Конфигурация счета: инкремент/декремент по спаду (FALLING)
    pcnt_config.pos_mode = PCNT_COUNT_DIS;   // Игнорируем фронт (RISING)
    pcnt_config.neg_mode = PCNT_COUNT_INC;   // По умолчанию считаем вверх
    
    // Граничные значения аппаратного регистра до сброса (максимальные лимиты)
    pcnt_config.counter_h_lim = 32000;
    pcnt_config.counter_l_lim = -32000;

    // Запись конфигурации в регистры кремния
    pcnt_unit_config(&pcnt_config);

    // Встроенный цифровой глитч-фильтр: защита от любых внешних помех
    // Значение 1000 тактов отсекает высокочастотный шум
    pcnt_set_filter_value(PCNT_UNIT_0, 1000);
    pcnt_filter_enable(PCNT_UNIT_0);

    // Сброс и запуск аппаратного счетчика в чистый кремниевый ноль
    pcnt_counter_clear(PCNT_UNIT_0);
    pcnt_counter_resume(PCNT_UNIT_0);
    
    Serial.println("[PCNT] Железный счетчик импульсов успешно запущен на GPIO 23");
}

void encoder_set_direction(int dir) {
    // На лету меняем логику счета железного регистра при изменении направления мотора
    if (dir == 1) {
        pcnt_set_mode(PCNT_UNIT_0, PCNT_CHANNEL_0, PCNT_COUNT_DIS, PCNT_COUNT_INC, PCNT_MODE_KEEP, PCNT_MODE_KEEP);
    } else {
        pcnt_set_mode(PCNT_UNIT_0, PCNT_CHANNEL_0, PCNT_COUNT_DIS, PCNT_COUNT_DEC, PCNT_MODE_KEEP, PCNT_MODE_KEEP);
    }
}

void encoder_update_count() {
    int16_t pulse_val = 0;
    // Мгновенное чтение значения напрямую из аппаратного регистра PCNT
    pcnt_get_counter_value(PCNT_UNIT_0, &pulse_val);
    
    // Живая координата окна — это чистый аппаратный счет плюс фиксированный стартовый офсет
    window_pulses = (long)pulse_val + initial_offset; 
}

void encoder_set_offset(long offset_val) {
    initial_offset = offset_val;
    window_pulses = offset_val; // Сразу выставляем стартовую позицию для первого вывода в логи
    
    Serial.printf("[PCNT] Стартовый офсет %ld шагов успешно применен к аппаратному регистру.\n", initial_offset);
}
