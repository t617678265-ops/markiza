#include "calibrator.h"
#include "encoder.h"

// Определение новых глобальных переменных границ
long config_pos_closed = 0; 
long config_pos_opened = 0; 

// Инициализация глобальных флагов блокировки пуска в упоры рамы
bool is_window_fully_closed = false;
bool is_window_fully_open = false;

// Объект Preferences для работы с энергонезависимой памятью ESP32
static Preferences prefs;

void calibrator_init() {
    // Открываем хранилище в режиме "только чтение" (read-only = true)
    prefs.begin("window_kv", true);
    
    // 1. Считываем сохраненные независимые координаты краев
    config_pos_closed = prefs.getLong("pos_closed", 0);
    config_pos_opened = prefs.getLong("pos_opened", 2760); // 2760 как эталонный заводской запас
    
    // 2. Считываем последнюю живую позицию, на которой остановился вал
    long last_saved_pos = prefs.getLong("last_pos", 0);
    
    prefs.end();
    
    // 3. ЖЕСТКАЯ ПЕРЕДАЧА: Записываем восстановленное число в энкодер как стартовый офсет
    encoder_set_offset(last_saved_pos);
    
    // На основе восстановленной координаты сразу выставляем флаги блокировок при включении платы
    if (window_pulses <= (config_pos_closed + 5)) {
        is_window_fully_closed = true;
        is_window_fully_open = false;
    } 
    else if (window_pulses >= (config_pos_opened - 5)) {
        is_window_fully_open = true;
        is_window_fully_closed = false;
    } else {
        is_window_fully_closed = false;
        is_window_fully_open = false;
    }
    
    Serial.printf("[CALIB] Память загружена: Низ (0%%) = %ld | Верх (100%%) = %ld | Текущая позиция = %ld шагов\n", 
                  config_pos_closed, config_pos_opened, window_pulses);
}

void calibrator_check_stop_event(MotorControlState last_moving_state) {
    if (last_moving_state != MAN_OPEN && last_moving_state != MAN_CLOSE) {
        return;
    }

    long current_measured_steps = window_pulses;

    // --- 1. ОБРАБОТКА УПОРА ПРИ ЗАКРЫТИИ (Точка 0%) ---
    if (last_moving_state == MAN_CLOSE) {
        // Вычисляем отклонение текущего физического упора от старой сохраненной границы
        long steps_delta = abs(current_measured_steps - config_pos_closed);

        is_window_fully_closed = true;
        is_window_fully_open = false;
        
        Serial.printf("[CALIB] Упор закрытия зафиксирован на отметке: %ld. Блокировка пуска вниз АКТИВИРОВАНА.\n", current_measured_steps);

        // КОРИДОР НЕЧУВСТВИТЕЛЬНОСТИ 5 ШАГОВ: бережем ресурс Flash от микро-люфтов резины
        if (steps_delta > 5) {
            config_pos_closed = current_measured_steps;
            
            prefs.begin("window_kv", false);
            prefs.putLong("pos_closed", config_pos_closed);
            prefs.end();
            
            Serial.printf("[CALIB] Граница низа обновлена во Flash: %ld шагов (дельта: %ld)\n", config_pos_closed, steps_delta);
        } else {
            config_pos_closed = current_measured_steps;
            Serial.printf("[CALIB] Совпадение с базой низа (дельта: %ld). Запись во Flash пропущенна.\n", steps_delta);
        }
        
        // После любого автостопа принудительно фиксируем финальную координату
        calibrator_save_current_position();
    }

    // --- 2. ОБРАБОТКА УПОРА ПРИ ОТКРЫТИИ (Точка 100%) ---
    else if (last_moving_state == MAN_OPEN) {
        // Вычисляем отклонение текущего физического упора от старой максимальной границы
        long steps_delta = abs(current_measured_steps - config_pos_opened);

        is_window_fully_open = true;
        is_window_fully_closed = false;
        
        Serial.printf("[CALIB] Упор открытия зафиксирован на отметке: %ld. Блокировка пуска вверх АКТИВИРОВАНА.\n", current_measured_steps);

        // КОРИДОР НЕЧУВСТВИТЕЛЬНОСТИ 5 ШАГОВ: бережем ресурс Flash
        if (steps_delta > 5) {
            config_pos_opened = current_measured_steps;
            
            prefs.begin("window_kv", false);
            prefs.putLong("pos_opened", config_pos_opened);
            prefs.end();
            
            Serial.printf("[CALIB] Граница верха обновлена во Flash: %ld шагов (дельта: %ld)\n", config_pos_opened, steps_delta);
        } else {
            config_pos_opened = current_measured_steps;
            Serial.printf("[CALIB] Совпадение с базой верха (дельта: %ld). Запись во Flash пропущенна.\n", steps_delta);
        }
        
        // После любого автостопа принудительно фиксируем финальную координату
        calibrator_save_current_position();
    }
}

void calibrator_update_flags() {
    // Если окно начало открываться и уехало от сохраненного нижнего упора более чем на 5 шагов
    if (is_window_fully_closed && window_pulses > (config_pos_closed + 5)) {
        is_window_fully_closed = false;
        Serial.println("[CALIB] Окно вышло из зоны закрытия. Блокировка пуска вниз снята.");
    }

    // Если окно начало закрываться и опустилось ниже сохраненного верхнего упора более чем на 5 шагов
    if (is_window_fully_open && window_pulses < (config_pos_opened - 5)) {
        is_window_fully_open = false;
        Serial.println("[CALIB] Окно вышло из зоны открытия. Блокировка пуска вверх снята.");
    }
}

void calibrator_save_current_position() {
    prefs.begin("window_kv", false);
    prefs.putLong("last_pos", window_pulses);
    prefs.end();
    
    Serial.printf("[CALIB] Позиция %ld шагов сохранена во Flash.\n", window_pulses);
}
