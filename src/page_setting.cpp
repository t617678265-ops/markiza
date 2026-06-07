#include "page_setting.h"

String get_page_setting(String ip_address) {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    
    // Стили инженерной страницы (Пастельно-тёмное оформление)
    html += "<style>";
    html += "body{background-color:#22252a; color:#e2e5e9; font-family:sans-serif; display:flex; justify-content:center; align-items:center; height:100vh; margin:0;}";
    html += ".card{background-color:#2d3139; padding:25px; border-radius:12px; box-shadow:0 4px 15px rgba(0,0,0,0.3); width:90%; max-width:360px; text-align:center;}";
    html += "h2{color:#b0c4de; margin-top:0;}";
    html += "p{font-size:14px; color:#a0a5af; margin:5px 0;}";
    html += ".ip-info{background-color:#1e2127; padding:10px; border-radius:6px; font-family:monospace; color:#7f9cc4; font-size:16px; margin:15px 0;}";
    
    // Стили формы OTA обновления прошивки
    html += ".ota-form{background-color:#1e2127; padding:15px; border-radius:8px; border:1px solid #3d424d; margin-top:20px; text-align:left;}";
    html += ".ota-form label{display:block; font-size:13px; color:#a0a5af; margin-bottom:8px;}";
    html += "input[type=file]{width:100%; color:#fff; font-size:14px; margin-bottom:10px;}";
    html += "input[type=submit]{width:100%; padding:10px; border:none; border-radius:6px; background-color:#7f9cc4; color:#22252a; font-size:14px; font-weight:bold; cursor:pointer;}";
    
    // Пастельно-красная кнопка сброса настроек Wi-Fi
    html += "button.btn-reset{width:100%; padding:12px; margin-top:20px; border:none; border-radius:6px; background-color:#c47f7f; color:#22252a; font-size:16px; font-weight:bold; cursor:pointer;}";
    html += "button.btn-reset:hover{background-color:#cd9292;}";
    html += ".link-back{display:inline-block; font-size:14px; color:#7f9cc4; text-decoration:none; margin-top:20px;}";
    html += "</style></head><body>";

    // Сборка карточки настроек
    html += "<div class='card'><h2>Инженерное Меню</h2>";
    html += "<p>Текущий IP адрес платы в сети:</p>";
    html += "<div class='ip-info'>" + ip_address + "</div>";
    
    // Форма выбора файла прошивки для OTA
    html += "<div class='ota-form'>";
    html += "  <form method='POST' action='/update' enctype='multipart/form-data'>";
    html += "    <label>Обновление прошивки по воздуху:</label>";
    html += "    <input type='file' name='update' required>";
    html += "    <input type='submit' value='Загрузить прошивку (Update)'>";
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
