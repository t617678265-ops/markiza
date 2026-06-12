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

int window_percent = 45;
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
              { request->send(200, "text/html", get_page_main(window_percent, motor_status_text)); });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // На лету пересчитываем реальный процент окна для веб-интерфейса на основе шагов
        long total_steps = config_pos_opened - config_pos_closed;
        if (total_steps > 0) {
            window_percent = ((window_pulses - config_pos_closed) * 100) / total_steps;
            if (window_percent < 0) window_percent = 0;
            if (window_percent > 100) window_percent = 100;
        }

        String data = String(window_percent) + "," + motor_status_text;
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
                    window_percent = target_pos; 
                    motor_status_text = "Переход...";
                    
                    // МАТЕМАТИКА ПРЕ СЕТА: переводим проценты пресета в шаги
                    long total_steps = config_pos_opened - config_pos_closed;
                    motor_target_steps = config_pos_closed + (total_steps * target_pos) / 100;
                    
                    // Переключаем привод в режим автоматического ведения к координате
                    target_motor_state = GOTO_POSITION;
                    
                    Serial.printf("[MOTOR] Едем к сохраненному пресету M%d в положение %d%% (Цель: %ld шагов)\n", 
                                  id + 1, target_pos, motor_target_steps);
                }
            }
        }
        request->send(200, "text/plain", "OK"); });

    server.on("/setting", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String current_ip = WiFi.localIP().toString();
        request->send(200, "text/html", get_page_setting(current_ip)); });

    // ─────────────────────────────────────────────────────────────────────────
    // ОБРАБОТЧИКИ СЕТИ: ТЕПЕРЬ ТУТ НЕТ МЕДЛЕННЫХ ЛОГОВ, ВСЁ РАБОТАЕТ МГНОВЕННО
    // ─────────────────────────────────────────────────────────────────────────
    server.on("/open", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        motor_status_text = "Открытие...";
        encoder_set_direction(1); 
        target_motor_state = MAN_OPEN; 
        request->send(200, "text/plain", "OK"); });

    server.on("/close", HTTP_GET, [](AsyncWebServerRequest *request)
              {
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
                window_percent = target_pos;
                motor_status_text = "Переход...";
                
                // МАТЕМАТИКА СЛАЙДЕРА: переводим проценты ползунка в шаги
                long total_steps = config_pos_opened - config_pos_closed;
                motor_target_steps = config_pos_closed + (total_steps * target_pos) / 100;
                
                // Переключаем привод в режим автоматического ведения к координате
                target_motor_state = GOTO_POSITION;
                
                Serial.printf("[MOTOR] Положение изменено слайдером на: %d%% (Цель: %ld шагов)\n", target_pos, motor_target_steps);
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
