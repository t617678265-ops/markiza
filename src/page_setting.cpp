#include "page_setting.h"

String get_page_setting(String ip_address, int pwm_val, float sens_val, int stop_adc) {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    
    // Стили инженерной страницы (Пастельно-тёмное оформление)
    html += "<style>";
    html += "body{background-color:#22252a; color:#e2e5e9; font-family:sans-serif; display:flex; justify-content:center; align-items:center; min-height:100vh; margin:0; padding:20px; box-sizing:border-box;}";
    html += ".card{background-color:#2d3139; padding:25px; border-radius:12px; box-shadow:0 4px 15px rgba(0,0,0,0.3); width:90%; max-width:360px; text-align:center;}";
    html += "h2{color:#b0c4de; margin-top:0;}";
    html += "<p{font-size:14px; color:#a0a5af; margin:5px 0;}";
    html += ".ip-info{background-color:#1e2127; padding:10px; border-radius:6px; font-family:monospace; color:#7f9cc4; font-size:16px; margin:15px 0;}";
    
    // Стили форм и полей ввода параметров железа
    html += ".cfg-form{background-color:#1e2127; padding:15px; border-radius:8px; border:1px solid #3d424d; margin-top:15px; text-align:left;}";
    html += ".cfg-group{margin-bottom:15px;}";
    html += ".cfg-group label{display:block; font-size:13px; color:#b0c4de; font-weight:bold; margin-bottom:5px;}";
    html += ".cfg-group input[type=number]{width:100%; padding:8px; border:1px solid #3d424d; border-radius:6px; background-color:#22252a; color:#fff; font-size:14px; box-sizing:border-box;}";
    html += ".cfg-group .hint{font-size:11px; color:#a0a5af; margin-top:4px; line-height:1.3;}";
    
    // Стили кнопок управления
    html += "input[type=submit].btn-save{width:100%; padding:11px; border:none; border-radius:6px; background-color:#7fc49c; color:#22252a; font-size:14px; font-weight:bold; cursor:pointer; margin-top:5px;}";
    html += "button.btn-default{width:100%; padding:10px; border:1px solid #5c616b; border-radius:6px; background-color:#1e2127; color:#b0c4de; font-size:13px; font-weight:bold; cursor:pointer; margin-top:10px;}";
    
    // Стили формы OTA обновления прошивки
    html += ".ota-form{background-color:#1e2127; padding:15px; border-radius:8px; border:1px solid #3d424d; margin-top:20px; text-align:left;}";
    html += ".ota-form label{display:block; font-size:13px; color:#a0a5af; margin-bottom:8px;}";
    html += "input[type=file]{width:100%; color:#fff; font-size:14px; margin-bottom:10px;}";
    html += "input[type=submit].btn-ota{width:100%; padding:10px; border:none; border-radius:6px; background-color:#7f9cc4; color:#22252a; font-size:14px; font-weight:bold; cursor:pointer;}";
    
    // Пастельно-красная кнопка сброса настроек Wi-Fi
    html += "button.btn-reset{width:100%; padding:12px; margin-top:20px; border:none; border-radius:6px; background-color:#c47f7f; color:#22252a; font-size:16px; font-weight:bold; cursor:pointer;}";
    html += "button.btn-reset:hover{background-color:#cd9292;}";
    html += ".link-back{display:inline-block; font-size:14px; color:#7f9cc4; text-decoration:none; margin-top:20px;}";
    html += "</style></head><body>";

    // Сборка карточки настроек
    html += "<div class='card'><h2>Инженерное Меню</h2>";
    html += "<p>Текущий IP адрес платы в сети:</p>";
    html += "<div class='ip-info'>" + ip_address + "</div>";
    
    // --- ФОРМА НАСТРОЕК СИЛОВОГО ЖЕЛЕЗА ПРИВОДА ---
    html += "<div class='cfg-form'>";
    html += "  <form method='GET' action='/save_hardware_config'>";
    
    // 1. Скорость открытия окна (ИСПРАВЛЕНО: Прямое значение ШИМ вместо процентов)
    html += "    <div class='cfg-group'>";
    html += "      <label>1. Мощность мотора (ШИМ 0-255):</label>";
    html += "      <input type='number' name='pwm' min='50' max='230' value='" + String(pwm_val) + "' required>";
    html += "      <div class='hint'>Рабочий диапазон: 50 - 230. Заводское значение: 128. Максимум 230 для защиты силовых конденсаторов драйвера.</div>";
    html += "    </div>";
    
    // 2. Чувствительность к препятствиям
    html += "    <div class='cfg-group'>";
    html += "      <label>2. Чувствительность к препятствиям:</label>";
    html += "      <input type='number' name='sens' min='1.5' max='10.0' step='0.1' value='" + String(sens_val, 1) + "' required>";
    html += "      <div class='hint'>Диапазон: 1.5 - 10.0. Чем меньше цифра, тем чувствительнее защита к заклиниванию створки.</div>";
    html += "    </div>";
    
    // 3. Аварийная остановка по току
    html += "    <div class='cfg-group'>";
    html += "      <label>3. Аварийная остановка по току:</label>";
    html += "      <input type='number' name='stop_adc' min='500' max='3000' value='" + String(stop_adc) + "' required>";
    html += "      <div class='hint'>Диапазон: 500 - 3000 АЦП. Мгновенная жесткая отсечка при глухом ударе в раму на старте.</div>";
    html += "    </div>";
    
    html += "    <input type='submit' value='Сохранить конфигурацию' class='btn-save'>";
    html += "  </form>";
    
    // Кнопка возврата к дефолтным константам из кода
    html += "  <a href='/reset_hardware_default'><button class='btn-default'>Сбросить настройки по умолчанию</button></a>";
    html += "</div>";

    // Форма выбора файла прошивки для OTA
    html += "<div class='ota-form'>";
    html += "  <form method='POST' action='/update' enctype='multipart/form-data'>";
    html += "    <label>Обновление прошивки по воздуху:</label>";
    html += "    <input type='file' name='update' required>";
    html += "    <input type='submit' value='Загрузить прошивку (Update)' class='btn-ota'>";
    html += "  </form>";
    html += "</div>";

    // Кнопка полного сброса сети
    html += "<a href='/reset_wifi'><button class='btn-reset'>Сбросить настройки Wi-Fi</button></a>";
    
    // Ссылка возврата на пользовательский пульт
    html += "<br><a href='/' class='link-back'>← Вернуться на главный пульт</a>";
    html += "</div>";

    html += "</body></html>";
    return html;
}
