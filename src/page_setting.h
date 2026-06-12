#pragma once
#include <Arduino.h>

// Функция возвращает HTML-код инженерной страницы настроек
// Принимает текущий IP-адрес платы, скорость ШИМ, коэффициент защиты и предел АЦП
String get_page_setting(String ip_address, int pwm_val, float sens_val, int stop_adc);
