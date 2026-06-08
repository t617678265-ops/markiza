#include "current.h"
#include <Arduino.h>

void current_init() {
    // Настраиваем пин шунта как аналоговый вход
    pinMode(PIN_SHUNT, ANALOG);
}

int current_get_raw() {
    int max_value = 0;
    
    // Делаем быструю серию замеров, чтобы поймать пик импульса ШИМ
    for (int i = 0; i < 30; i++) {
        int current_sample = analogRead(PIN_SHUNT);
        if (current_sample > max_value) {
            max_value = current_sample;
        }
        // Небольшая микросекундная пауза для разделения замеров
        delayMicroseconds(5); 
    }
    
    return max_value;
}
