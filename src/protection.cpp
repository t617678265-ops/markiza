#include "protection.h"
#include "encoder.h"
#include "current.h"
#include "motor.h" // Подключаем для доступа к режимам мотора и slider_direction

// ИНТЕГРАЦИЯ: Аккуратно импортируем живые общие переменные защиты из web_server.cpp
extern float config_protection_sens;
extern int config_abs_stop_adc;

// Глобальный флаг фиксации физического упора для модуля motor.cpp
bool is_protect_triggered = false;

static unsigned long speed_calc_timer = 0;
static unsigned long start_blanking_timer = 0; // Таймер слепой зоны пуска
static unsigned long motor_run_start_timer = 0; // Таймер защиты от поломки редуктора (2 минуты)

// Накопительный массив для сбора маршевого хода (600 ячеек по 50 мс = 30 секунд пути)
#define CURRENT_BUFFER_SIZE 600
static int current_history[CURRENT_BUFFER_SIZE];
static int history_count = 0;                  // Текущее количество собранных элементов в массиве

static bool in_blanking = false;               // Флаг активного пускового окна

// Локальный трекер состояния для отслеживания момента старта мотора
static MotorControlState last_motor_state = MAN_STOP;

// Импортируем из motor.cpp маркер реального направления автоматического слайдера
extern int slider_direction;

void protection_init() {
    speed_calc_timer = millis();
    start_blanking_timer = 0;
    motor_run_start_timer = 0;
    history_count = 0;
    in_blanking = false;
    last_motor_state = MAN_STOP;
    is_protect_triggered = false;
    
    // Очищаем массив истории нулями
    for (int i = 0; i < CURRENT_BUFFER_SIZE; i++) {
        current_history[i] = 0;
    }
}

void protection_tick() {
    // Если мотор полностью остановлен, сбрасываем флаги и выходим
    if (target_motor_state == MAN_STOP) {
        in_blanking = false;
        last_motor_state = MAN_STOP;
        return;
    }

    // --- 1. ДЕТЕКЦИЯ МОМЕНТА СТАРТА ДВИЖЕНИЯ (РУЧНОГО ИЛИ ПО СЛАЙДЕРУ) ---
    if (last_motor_state == MAN_STOP && 
       (target_motor_state == MAN_OPEN || target_motor_state == MAN_CLOSE || target_motor_state == GOTO_POSITION)) {
        
        start_blanking_timer = millis();
        motor_run_start_timer = millis(); // Фиксируем честное время старта!
        speed_calc_timer = millis();
        in_blanking = true;            // Включаем 100 мс слепой зоны
        history_count = 0;             // Сбрасываем старый массив
        is_protect_triggered = false;  
        last_motor_state = target_motor_state;
        Serial.println("[PROTECT] Мотор запущен. Таймер редуктора инициализирован. Слепая зона 100 мс...");
    }

    // --- 2. КРИТИЧЕСКАЯ ЗАЩИТА РЕДУКТОРА ПО ТАЙМ-АУТУ ХОДА (2 МИНУТЫ) ---
    if (millis() - motor_run_start_timer >= 120000) { 
        target_motor_state = MAN_STOP;
        last_motor_state = MAN_STOP;
        is_protect_triggered = false; // Это авария, калибровку выполнять нельзя
        Serial.println("[ALARM] КРИТИЧЕСКАЯ ОШИБКА: ПРЕВЫШЕН ТАЙМ-АУТ ХОДА (2 МИН)!");
        return;
    }
    // --- 3. ПОЛУЧЕНИЕ ОТФИЛЬТРОВАННОГО ТОКА ШУНТА ---
    int current_adc_current = current_get_raw();

    // --- 4. РУБЕЖ ЗАЩИТЫ №2: ЖЕСТКИЙ АБСОЛЮТНЫЙ ПОТОЛОК ИЗ ВЕБ-ИНТЕРФЕЙСА ---
    if (current_adc_current > config_abs_stop_adc) {
        is_protect_triggered = true;
        target_motor_state = MAN_STOP;
        last_motor_state = MAN_STOP;
        Serial.printf("[PROTECT] ДВУХУРОВНЕВЫЙ СТОП! Превышен абсолютный предел тока: %d | Порог: %d\n", 
                      current_adc_current, config_abs_stop_adc);
        return;
    }

    // --- 5. КОНТРОЛЬ ВРЕМЕНИ СЛЕПОЙ ЗОНЫ ПУСКА (100 мс) ---
    if (in_blanking) {
        if (millis() - start_blanking_timer >= 500) {
            in_blanking = false; // Окно закрылось, переходим к адаптивному накоплению хода
            speed_calc_timer = millis();
            Serial.println("[PROTECT] Слепая зона пуска завершена. Начался сбор адаптивного массива.");
        } else {
            return; // Внутри слепой зоны динамические пороги не считаем
        }
    }

    // --- 6. АДАПТИВНЫЙ АВТОСТОП С РАСЧЕТОМ СРЕДНЕГО ПО СТАРЫМ 2/3 МАССИВА ---
    if (millis() - speed_calc_timer >= 50) {
        speed_calc_timer = millis();

        // Динамически выбираем коэффициент с учетом автоматического режима слайдера GOTO_POSITION
        float active_factor = config_protection_sens; // Общий живой коэффициент с сайта

        // Если массив еще пустой — просто инициализируем первую ячейку
        if (history_count == 0) {
            current_history[0] = current_adc_current;
            history_count = 1;
            return;
        }

        // Вычисляем индекс отсечки, равный ровно двум третям (2/3) от текущего размера массива
        int cut_off_index = (history_count * 2) / 3;
        
        if (cut_off_index == 0) {
            cut_off_index = 1;
        }

        // Вычисляем среднее арифметическое маршевое значение строго по исторической части (от 0 до cut_off_index)
        long current_sum = 0;
        for (int i = 0; i < cut_off_index; i++) {
            current_sum += current_history[i];
        }
        float average_march_current = (float)current_sum / (float)cut_off_index;

        // Рассчитываем динамический порог перегрузки для текущего шага
        float dynamic_threshold = average_march_current * active_factor;

        // Сравниваем текущее мгновенное значение с чистой исторической базой
        if ((float)current_adc_current > dynamic_threshold) {
            is_protect_triggered = true;
            target_motor_state = MAN_STOP;
            last_motor_state = MAN_STOP;
            
            Serial.printf("[PROTECT] АД-АВТОСТОП (50мс)! Ток прыгнул: %d | Старая база 2/3: %.1f | Допуск: %.1f | Элементов: %d\n", 
                          current_adc_current, average_march_current, dynamic_threshold, history_count);
        } else {
            // Ток в норме (обычный чистый ход). Дописываем замер в конец накопительного массива
            if (history_count < CURRENT_BUFFER_SIZE) {
                current_history[history_count] = current_adc_current;
                history_count++;
            } else {
                // Если окно едет дольше 30 секунд, сдвигаем массив назад, чтобы освободить место для свежей точки
                for (int i = 1; i < CURRENT_BUFFER_SIZE; i++) {
                    current_history[i - 1] = current_history[i];
                }
                current_history[CURRENT_BUFFER_SIZE - 1] = current_adc_current;
            }
        }
    }
}