#include "calibrator.h"
#include "encoder.h"
#include "driver/pcnt.h" // Устраняет ошибку строки 33, добавляя pcnt_counter_clear и PCNT_UNIT_0

// Определение глобальных переменных
long max_window_steps = 0; 

// Объект Preferences для работы с энергонезависимой памятью ESP32
static Preferences prefs;

void calibrator_init() {
    // Открываем хранилище в режиме "только чтение" (read-only = true)
    prefs.begin("window_kv", true);
    
    // Считываем сохраненный эталон шагов. Если данных нет, вернется 0 (по умолчанию)
    max_window_steps = prefs.getLong("max_steps", 0);
    
    prefs.end();
    
    Serial.printf("[CALIB] Инициализация памяти. Загружено крайнее положение: %ld шагов\n", max_window_steps);
}

void calibrator_check_stop_event(MotorControlState last_moving_state) {
    // Нам важна реакция только на автоматические команды открытия и закрытия до упора
    if (last_moving_state != MAN_OPEN && last_moving_state != MAN_CLOSE) {
        return;
    }

    // --- 1. ОБРАБОТКА УПОРА ПРИ ЗАКРЫТИИ (Точка 0%) ---
    if (last_moving_state == MAN_CLOSE) {
        long final_offset = window_pulses; // Смотрим, где аппаратно встал счетчик до обнуления

        // Принудительно сбрасываем аппаратный счетчик PCNT в физический ноль рамы
        pcnt_counter_clear(PCNT_UNIT_0);
        window_pulses = 0;
        
        Serial.printf("[CALIB] Упор закрытия зафиксирован. Счетчик аппаратно обнулен.\n");

        // ЗОНА НЕЧУВСТВИТЕЛЬНОСТИ 5 ШАГОВ: проверяем отклонение от старой базы
        if (abs(final_offset) > 5) {
            // Если сдвиг значительный, обновляем ноль в оперативной памяти и логах
            Serial.printf("[CALIB] Сдвиг нуля составил %ld шагов. База адаптирована.\n", final_offset);
        } else {
            Serial.printf("[CALIB] Сдвиг нуля незначительный (%ld шагов). Память Flash не перезаписывается.\n", final_offset);
        }
    }

    // --- 2. ОБРАБОТКА УПОРА ПРИ ОТКРЫТИИ (Точка 100%) ---
    else if (last_moving_state == MAN_OPEN) {
        long current_measured_steps = window_pulses;

        if (current_measured_steps < 0) {
            current_measured_steps = 0;
        }

        // ВЫЧИСЛЕНИЕ ДЕЛЬТЫ ДЛЯ ЗАЩИТЫ FLASH-ПАМЯТИ ОТ ИЗНОСА
        long steps_delta = abs(current_measured_steps - max_window_steps);

        if (steps_delta > 5) {
            // Отклонение превысило зону нечувствительности в 5 шагов — выполняем физическую запись
            max_window_steps = current_measured_steps;
            
            // Открываем хранилище в режиме записи (read-only = false)
            prefs.begin("window_kv", false);
            prefs.putLong("max_steps", max_window_steps);
            prefs.end();
            
            Serial.printf("[CALIB] ВНИМАНИЕ: Геометрия изменилась! Новая длина хода записана в Flash: %ld шагов (дельта: %ld)\n", 
                          max_window_steps, steps_delta);
        } else {
            // Отклонение в пределах 5 шагов — подменяем переменную в RAM, но Flash бережем
            max_window_steps = current_measured_steps;
            Serial.printf("[CALIB] Положение совпадает с эталоном (дельта: %ld). Запись во Flash пропущена для сохранения ресурса.\n", 
                          steps_delta);
        }
    }
}
