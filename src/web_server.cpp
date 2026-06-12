#include "web_server.h"
#include "page_main.h"
#include "page_setting.h"
#include "motor.h"
#include "encoder.h"
#include "calibrator.h" // Подключаем для доступа к config_pos_closed и config_pos_opened
#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <Update.h>
#include <ESPAsyncWebServer.h>
#include <EEPROM.h>

AsyncWebServer server(80);

// РАЗДЕЛЕНИЕ ПЕРЕМЕННЫХ: 
int slider_target_percent = 45; // Уставка ползунка (орган ввода, стоит на месте)
int window_percent = 45;        // Реальный ход створки (анимация на экране)

String motor_status_text = "Остановлено";
int mem_positions[] = {255, 255, 255, 255, 255, 255};

// Определение глобальной целевой переменной шагов для motor.h
long motor_target_steps = 0;

extern volatile long window_pulses;

// Импортируем динамические плавающие границы из калибратора
extern long config_pos_closed;
extern long config_pos_opened;

void web_server_init()
{
    EEPROM.begin(512);

    for (int i = 0; i < 6; i++)
    {
        mem_positions[i] = EEPROM.read(200 + i);
        if (mem_positions[i] > 100 && mem_positions[i] != 255)
        {
            mem_positions[i] = 255;
        }
    }

    if (MDNS.begin("okno"))
    {
        Serial.println("[mDNS] Служба запущена! Адрес сайта: http://okno.local");
        MDNS.addService("http", "tcp", 80);
    }

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html", get_page_main(slider_target_percent, motor_status_text)); });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // На лету пересчитываем реальный процент окна для веб-интерфейса на основе шагов
        long total_steps = config_pos_opened - config_pos_closed;
        if (total_steps > 0) {
            window_percent = ((window_pulses - config_pos_closed) * 100) / total_steps;
            if (window_percent < 0) window_percent = 0;
            if (window_percent > 100) window_percent = 100;
        }

        // Если мотор стоит на месте, синхронизируем уставку ползунка с реальным закрытием
        if (target_motor_state == MAN_STOP) {
            if (is_window_fully_closed) slider_target_percent = 0;
            else if (is_window_fully_open) slider_target_percent = 100;
        }

        // ОТПРАВКА СТАТУСА: slider_target_percent(0), window_percent(1), motor_status_text(2)
        String data = String(slider_target_percent) + "," + String(window_percent) + "," + motor_status_text;
        for (int i = 0; i < 6; i++) {
            data += "," + String(mem_positions[i]);
        }
        request->send(200, "text/plain", data); });

    server.on("/mem_save", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("id") && request->hasParam("pos")) {
            int id = request->getParam("id")->value().toInt() - 1; 
            int pos = request->getParam("pos")->value().toInt();
            if (id >= 0 && id < 6 && pos >= 0 && pos <= 100) {
                mem_positions[id] = pos;           
                EEPROM.write(200 + id, pos);       
                EEPROM.commit();                   
                Serial.printf("[MEMORY] Ячейка M%d успешно сохранена: %d%%\n", id + 1, pos);
            }
        }
        request->send(200, "text/plain", "OK"); });

            server.on("/mem_go", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("id")) {
            int id = request->getParam("id")->value().toInt() - 1;
            if (id >= 0 && id < 6) {
                int target_pos = mem_positions[id];
                if (target_pos >= 0 && target_pos <= 100) {
                    slider_target_percent = target_pos; // Фиксируем уставку бегунка
                    
                    // ЗОЛОТОЙ СТАНДАРТ АВТОМАТИКИ ДЛЯ ПРЕСЕТОВ ПАМЯТИ
                    if (target_pos == 0) {
                        motor_status_text = "Закрытие...";
                        encoder_set_direction(-1);
                        target_motor_state = MAN_CLOSE; // Валим до силового упора рамы низа!
                        Serial.printf("[MOTOR] Пресет M%d (0%%): Запущен силовой дожим до упора вниз\n", id + 1);
                    } 
                    else if (target_pos == 100) {
                        motor_status_text = "Открытие...";
                        encoder_set_direction(1);
                        target_motor_state = MAN_OPEN;  // Валим до силового упора верха!
                        Serial.printf("[MOTOR] Пресет M%d (100%%): Запущен силовой накат до упора вверх\n", id + 1);
                    } 
                    else {
                        // Для всех промежуточных положений (1-99%) оставляем координатное ведение
                        motor_status_text = "Переход...";
                        long total_steps = config_pos_opened - config_pos_closed;
                        motor_target_steps = config_pos_closed + (total_steps * target_pos) / 100;
                        target_motor_state = GOTO_POSITION;
                        Serial.printf("[MOTOR] Едем к сохраненному пресету M%d в положение %d%% (Цель: %ld шагов)\n", 
                                      id + 1, target_pos, motor_target_steps);
                    }
                }
            }
        }
        request->send(200, "text/plain", "OK"); });

    server.on("/setting", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String current_ip = WiFi.localIP().toString();
        request->send(200, "text/html", get_page_setting(current_ip)); });

    // ─────────────────────────────────────────────────────────────────────────
    // ОБРАБОТЧИКИ СЕТИ: РУЧНЫЕ КНОПКИ САЙТА НАПРЯМУЮ УПРАВЛЯЮТ СИЛОВЫМ ЯДРОМ
    // ─────────────────────────────────────────────────────────────────────────
    server.on("/open", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        slider_target_percent = 100; // Синхронизируем уставку
        motor_status_text = "Открытие...";
        encoder_set_direction(1); 
        target_motor_state = MAN_OPEN; 
        request->send(200, "text/plain", "OK"); });

    server.on("/close", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        slider_target_percent = 0; // Синхронизируем уставку
        motor_status_text = "Закрытие...";
        encoder_set_direction(-1); 
        target_motor_state = MAN_CLOSE; 
        request->send(200, "text/plain", "OK"); });

    server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        motor_status_text = "Остановлено";
        target_motor_state = MAN_STOP; 
        request->send(200, "text/plain", "OK"); });
    // ─────────────────────────────────────────────────────────────────────────

    server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("pos")) {
            String val = request->getParam("pos")->value();
            int target_pos = val.toInt();
            if (target_pos >= 0 && target_pos <= 100) {
                slider_target_percent = target_pos; // Фиксируем уставку, ползунок замирает!
                
                // ЗОЛОТОЙ СТАНДАРТ АВТОМАТИКИ ДЛЯ ПОЛЗУНКА СЛАЙДЕРА
                if (target_pos == 0) {
                    motor_status_text = "Закрытие...";
                    encoder_set_direction(-1);
                    target_motor_state = MAN_CLOSE; // Вместо GOTO_POSITION валим до упора вниз!
                    Serial.println("[MOTOR] Слайдер переведен в 0%. Запущен силовой дожим в раму.");
                } 
                else if (target_pos == 100) {
                    motor_status_text = "Открытие...";
                    encoder_set_direction(1);
                    target_motor_state = MAN_OPEN;  // Вместо GOTO_POSITION валим до упора вверх!
                    Serial.println("[MOTOR] Слайдер переведен в 100%. Запущен силовой накат вверх.");
                } 
                else {
                    // Для всех промежуточных положений (1-99%) оставляем координатное ведение
                    motor_status_text = "Переход...";
                    long total_steps = config_pos_opened - config_pos_closed;
                    motor_target_steps = config_pos_closed + (total_steps * target_pos) / 100;
                    target_motor_state = GOTO_POSITION;
                    Serial.printf("[MOTOR] Положение изменено слайдером на: %d%% (Цель: %ld шагов)\n", target_pos, motor_target_steps);
                }
            }
        }
        request->send(200, "text/plain", "OK"); });

    server.on("/reset_wifi", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        EEPROM.write(0, 0xFF); 
        EEPROM.commit();       
        String html = "<body style='background:#22252a;color:#fff;text-align:center;font-family:sans-serif;padding-top:50px;'><h3>Память Wi-Fi очищена!</h3><p>Плата перезагружается...</p></body>";
        request->send(200, "text/html", html);
        delay(2000);
        ESP.restart(); });

    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request)
              { request->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK"); }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
              {
        if (!index) {
            int cmd = (filename.indexOf("spiffs") >= 0 || filename.indexOf("littlefs") >= 0) ? U_SPIFFS : U_FLASH;
            if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
                Update.printError(Serial);
            }
        }
        if (!Update.hasError()) {
            if (Update.write(data, len) != len) Update.printError(Serial);
        }
        if (final) {
            if (Update.end(true)) {
                delay(500);
                ESP.restart();
            } else {
                Update.printError(Serial);
            }
        } });

    server.begin();
    Serial.println("[SERVER] Основной асинхронный веб-сервер запущен!");
}

void web_server_update()
{
}
