#pragma once
#include <Arduino.h>
#include "motor.h"

// Глобальная переменная порога тока (доступна для изменения из web_server.cpp)
extern int config_current_limit;

/**
 * @brief Инициализация модуля защиты (сброс таймеров)
 */
void protection_init();

/**
 * @brief Циклическая проверка аварийного упора по току и стоп-шагу.
 *        Вызывается каждую 1 мс внутри задачи ядра физики WindowPhysicsCoreCode.
 */
void protection_tick();
