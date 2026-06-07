#pragma once
#include <Arduino.h>

// Функция возвращает HTML-код главной страницы
// Принимает текущий процент окна и текст статуса мотора для отображения
String get_page_main(int percent, String status_text);
