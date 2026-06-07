#include "wifi_config.h"
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <EEPROM.h>

static AsyncWebServer config_server(80);

static void writeStringToEEPROM(int addr, String str) {
    int len = str.length();
    if (len > 50) len = 50; 
    EEPROM.write(addr, len);
    for (int i = 0; i < len; i++) EEPROM.write(addr + 1 + i, str[i]);
    EEPROM.write(addr + 1 + len, '\0');
}

static String readStringFromEEPROM(int addr) {
    int len = EEPROM.read(addr);
    if (len == 0xFF || len == 0 || len > 50) return ""; 
    String str = "";
    for (int i = 0; i < len; i++) {
        char c = char(EEPROM.read(addr + 1 + i));
        if (c == 0xFF || c == '\0') break;
        str += c;
    }
    return str;
}

void wifi_config_init() {
    EEPROM.begin(512);
    delay(50); 

    String ssid = readStringFromEEPROM(0);   
    String pass = readStringFromEEPROM(50);  
    bool connected = false;

    if (ssid.length() > 0 && ssid != "") {
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());
        int timeout = 0;
        while (WiFi.status() != WL_CONNECTED && timeout < 30) { 
            delay(500);
            timeout++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            WiFi.softAPdisconnect(true); 
            WiFi.mode(WIFI_STA); 
            Serial.println("\n[Wi-Fi] Успешно подключено к роутеру. IP: " + WiFi.localIP().toString());
        }
    }

    if (!connected) {
        WiFi.disconnect(true); 
        delay(100);
        WiFi.mode(WIFI_AP);
        WiFi.softAP("windowDrive_Core");
        delay(500); 

        config_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
            String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
            html += "<style>body{background-color:#22252a; color:#e2e5e9; font-family:sans-serif; display:flex; justify-content:center; align-items:center; height:100vh; margin:0;}";
            html += ".card{background-color:#2d3139; padding:25px; border-radius:12px; width:90%; max-width:360px; text-align:center;} h2{color:#b0c4de; margin-top:0;}";
            html += "label{display:block; margin:14px 0 4px; font-size:14px; color:#a0a5af; text-align:left;} input{width:100%; padding:10px; border:1px solid #3d424d; border-radius:6px; background-color:#1e2127; color:#fff; box-sizing:border-box;}";
            html += ".pass-wrapper{position:relative; width:100%;} .toggle-btn{position:absolute; right:10px; top:50%; transform:translateY(-50%); color:#7f9cc4; font-size:13px; cursor:pointer; font-weight:bold;}";
            html += "button{width:100%; padding:12px; margin-top:24px; border:none; border-radius:6px; background-color:#7f9cc4; color:#22252a; font-size:16px; font-weight:bold;}</style>";
            html += "<script>function togglePass(){var p=document.getElementById('p'); var b=document.getElementById('b'); if(p.type==='password'){p.type='text'; b.innerText='СКРЫТЬ';}else{p.type='password'; b.innerText='ПОКАЗАТЬ';}}</script></head><body>";
            html += "<div class='card'><h2>Настройка Wi-Fi</h2><form method='POST' action='/save'>";
            html += "<label>Имя сети (SSID)</label><input type='text' name='ssid' required>";
            html += "<label>Пароль от Wi-Fi</label><div class='pass-wrapper'><input type='password' name='pass' id='p' required><span class='toggle-btn' id='b' onclick='togglePass()'>ПОКАЗАТЬ</span></div>";
            html += "<button type='submit'>Сохранить настройки</button></form></div></body></html>";
            request->send(200, "text/html", html);
        });

        config_server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
            if (request->hasParam("ssid", true) && request->hasParam("pass", true)) {
                writeStringToEEPROM(0, request->getParam("ssid", true)->value());
                writeStringToEEPROM(50, request->getParam("pass", true)->value());
                EEPROM.commit(); 
                request->send(200, "text/html", "<body style='background:#22252a;color:#fff;text-align:center;padding-top:50px;'><h3>Настройки сохранены! Перезагрузка...</h3></body>");
                delay(2000);
                ESP.restart();
            } else request->send(400, "text/plain", "Bad Form");
        });

        config_server.begin();
        Serial.println("[Wi-Fi] Точка windowDrive_Core активна (192.168.4.1)");
        while (WiFi.status() != WL_CONNECTED) { 
            delay(100);
            yield();
        }
    }
}
