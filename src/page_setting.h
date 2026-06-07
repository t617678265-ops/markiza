#pragma once
#include <Arduino.h>

// Функция возвращает HTML-код инженерной страницы настроек
// Принимает текущий IP-адрес платы в виде строки для вывода на экран
String get_page_setting(String ip_address);
