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
#include <Preferences.h> // ИНТЕГРАЦИЯ: Энергонезависимая память для параметров железа

AsyncWebServer server(80);
Preferences hw_prefs; // Объект памяти для конфигурации привода

// ГЛОБАЛЬНЫЕ НАСТРОЙКИ ЖЕЛЕЗА ДЛЯ МОДУЛЕЙ MOTOR И PROTECTION
int config_pwm_speed = 128;         // Скорость ШИМ (20% - 90%, по умолчанию 128)
float config_protection_sens = 5.0f; // Коэффициент чувствительности адаптивной защиты
int config_abs_stop_adc = 1500;     // Предел аварийной остановки по току АЦП

// РАЗДЕЛЕНИЕ ПЕРЕМЕННЫХ ИНТЕРФЕЙСА
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

    // --- ЧТЕНИЕ И ИНИЦИАЛИЗАЦИЯ ЗАВОДСКИХ НАСТРОЕК ЖЕЛЕЗА ИЗ FLASH ---
    hw_prefs.begin("hw_cfg", false);
    config_pwm_speed = hw_prefs.getInt("pwm", 128);
    config_protection_sens = hw_prefs.getFloat("sens", 5.0f);
    config_abs_stop_adc = hw_prefs.getInt("stop", 1500);
    hw_prefs.end();

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
                    slider_target_percent = target_pos; 
                    
                    if (target_pos == 0) {
                        motor_status_text = "Закрытие...";
                        encoder_set_direction(-1);
                        target_motor_state = MAN_CLOSE; 
                    } 
                    else if (target_pos == 100) {
                        motor_status_text = "Открытие...";
                        encoder_set_direction(1);
                        target_motor_state = MAN_OPEN;  
                    } 
                    else {
                        motor_status_text = "Переход...";
                        long total_steps = config_pos_opened - config_pos_closed;
                        motor_target_steps = config_pos_closed + (total_steps * target_pos) / 100;
                        target_motor_state = GOTO_POSITION;
                    }
                }
            }
        }
        request->send(200, "text/plain", "OK"); });

    // ИСПРАВЛЕНО: Передаем на страницу настроек текущие живые значения ШИМ, защиты и АЦП
    server.on("/setting", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String current_ip = WiFi.localIP().toString();
        request->send(200, "text/html", get_page_setting(current_ip, config_pwm_speed, config_protection_sens, config_abs_stop_adc)); });

    // МАРШРУТ СОХРАНЕНИЯ НАСТРОЕК ЖЕЛЕЗА ИЗ HTML-ФОРМЫ В ПАМЯТЬ PREFERENCES
    server.on("/save_hardware_config", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("pwm") && request->hasParam("sens") && request->hasParam("stop_adc")) {
            config_pwm_speed = request->getParam("pwm")->value().toInt();
            config_protection_sens = request->getParam("sens")->value().toFloat();
            config_abs_stop_adc = request->getParam("stop_adc")->value().toInt();

            Preferences prefs;
            prefs.begin("hw_cfg", false);
            prefs.putInt("pwm", config_pwm_speed);
            prefs.putFloat("sens", config_protection_sens);
            prefs.putInt("stop", config_abs_stop_adc);
            prefs.end();
            Serial.printf("[CONFIG] Настройки сохранены во Flash! ШИМ: %d, Защита: %.1f, АЦП: %d\n", config_pwm_speed, config_protection_sens, config_abs_stop_adc);
        }
        request->redirect("/setting"); });

    // МАРШРУТ СБРОСА КОНФИГУРАЦИИ НА НАЧАЛЬНЫЕ ЗАВОДСКИЕ ЗНАЧЕНИЯ ИЗ КОДА
    server.on("/reset_hardware_default", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        config_pwm_speed = 128;
        config_protection_sens = 5.0f;
        config_abs_stop_adc = 1500;

        Preferences prefs;
        prefs.begin("hw_cfg", false);
        prefs.putInt("pwm", 128);
        prefs.putFloat("sens", 5.0f);
        prefs.putInt("stop", 1500);
        prefs.end();
        Serial.println("[CONFIG] Настройки сброшены на заводские дефолты!");
        request->redirect("/setting"); });

    server.on("/open", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        slider_target_percent = 100; 
        motor_status_text = "Открытие...";
        encoder_set_direction(1); 
        target_motor_state = MAN_OPEN; 
        request->send(200, "text/plain", "OK"); });

    server.on("/close", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        slider_target_percent = 0; 
        motor_status_text = "Закрытие...";
        encoder_set_direction(-1); 
        target_motor_state = MAN_CLOSE; 
        request->send(200, "text/plain", "OK"); });

    server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        motor_status_text = "Остановлено";
        target_motor_state = MAN_STOP; 
        request->send(200, "text/plain", "OK"); });

    server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("pos")) {
            String val = request->getParam("pos")->value();
            int target_pos = val.toInt();
            if (target_pos >= 0 && target_pos <= 100) {
                slider_target_percent = target_pos; 
                
                if (target_pos == 0) {
                    motor_status_text = "Закрытие...";
                    encoder_set_direction(-1);
                    target_motor_state = MAN_CLOSE; 
                } 
                else if (target_pos == 100) {
                    motor_status_text = "Открытие...";
                    encoder_set_direction(1);
                    target_motor_state = MAN_OPEN;  
                } 
                else {
                    motor_status_text = "Переход...";
                    long total_steps = config_pos_opened - config_pos_closed;
                    motor_target_steps = config_pos_closed + (total_steps * target_pos) / 100;
                    target_motor_state = GOTO_POSITION;
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

void web_server_update() {}