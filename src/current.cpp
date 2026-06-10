#include "current.h"
#include <Arduino.h>

void current_init() {
    // Настраиваем пин аналогового шунта на вход
    pinMode(PIN_SHUNT, INPUT);
    
    // Переключаем АЦП пина GPIO 34 на максимальную чувствительность 0 дБ
    // Измерительный диапазон до ~1.1 Вольт идеально под голый шунт 0.25 Ома
    analogSetPinAttenuation(PIN_SHUNT, ADC_0db);
}

int current_get_raw() {
    const int num_samples = 100;
    int samples[num_samples];
    long total_sum = 0;
    
    // --- ЭТАП 1: Набор сырых данных и вычисление общей базы ---
    for (int i = 0; i < num_samples; i++) {
        samples[i] = analogRead(PIN_SHUNT);
        total_sum += samples[i];
        delayMicroseconds(5); // Разделение выборок АЦП
    }
    int first_average = total_sum / num_samples;
    
    // --- ЭТАП 2: Отсекаем низ (паузы ШИМ) и ищем верхнюю планку ---
    long stage2_sum = 0;
    int stage2_count = 0;
    for (int i = 0; i < num_samples; i++) {
        if (samples[i] >= first_average) {
            stage2_sum += samples[i];
            stage2_count++;
        }
    }
    
    // Защита: если верхних пиков не найдено, отдаем базовое среднее
    if (stage2_count == 0) return first_average;
    int second_average = stage2_sum / stage2_count;
    
    // --- ЭТАП 3: Отсекаем верх (искры и артефакты) среди выживших пиков ---
    long final_sum = 0;
    int final_count = 0;
    for (int i = 0; i < num_samples; i++) {
        // Элемент должен быть выше общей базы, но ниже/равен верхней планке
        if (samples[i] >= first_average && samples[i] <= second_average) {
            final_sum += samples[i];
            final_count++;
        }
    }
    
    // --- ФИНАЛ: Расчет среднего значения внутри чистого коридора ---
    if (final_count > 0) {
        return final_sum / final_count;
    }
    
    return second_average;
}
