#include "page_main.h"

String get_page_main(int percent, String status_text) {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=no'>";
    
    html += "<style>";
    html += "body{background-color:#22252a; color:#e2e5e9; font-family:sans-serif; display:flex; justify-content:center; align-items:center; min-height:100vh; margin:0; padding:20px; box-sizing:border-box;}";
    html += ".card{background-color:#2d3139; padding:25px; border-radius:16px; box-shadow:0 6px 20px rgba(0,0,0,0.4); width:100%; max-width:400px; text-align:center;}";
    html += "h2{color:#b0c4de; margin:0 0 5px 0;}";
    html += ".status-val{font-size:32px; font-weight:bold; color:#fff; margin:10px 0 2px 0;}";
    html += ".status-text{font-size:14px; color:#a0a5af; margin-bottom:20px;}";
    
    html += ".window-svg{background:#1e2127; border-radius:8px; margin-bottom:20px; border:1px solid #3d424d;}";
    html += ".window-frame{stroke:#3d424d; stroke-width:4; fill:none;}";
    html += ".window-sash{stroke:#7f9cc4; stroke-width:4; fill:none; transform-origin: 140px 30px; transition: transform 0.5s ease;}";
    
    html += ".btn-block{display:flex; flex-direction:column; gap:10px; margin-bottom:25px;}";
    html += ".btn-row{display:flex; gap:10px;}";
    
    html += ".btn{flex:1; padding:15px 10px; border:none; border-radius:8px; font-size:15px; font-weight:bold; cursor:pointer;";
    html += "-webkit-user-select:none; user-select:none; -webkit-touch-callout:none;}";
    
    html += ".btn-auto-close{background-color:#7f9cc4; color:#22252a;}";
    html += ".btn-stop{background-color:#c47f7f; color:#22252a;}";
    html += ".btn-auto-open{background-color:#7fc49c; color:#22252a;}";
    
    html += ".btn-man-close{background-color:#5c708a; color:#fff;}";
    html += ".btn-man-open{background-color:#5c8a6f; color:#fff;}";
    html += ".btn-spacer{flex:1; visibility:hidden;}"; 
    
    // Стили для строки подсказки слайдера
    html += ".slider-title{text-align:left; font-size:14px; color:#a0a5af; margin-bottom:5px; display:flex; justify-content:space-between;}";
    html += ".slider-val-hint{color:#7f9cc4; font-weight:bold;}"; // Пастельно-синий цвет для живых процентов подсказки
    
    html += ".slider{width:100%; -webkit-appearance:none; height:8px; border-radius:4px; background:#1e2127; outline:none; margin-bottom:30px;}";
    html += ".slider::-webkit-slider-thumb{-webkit-appearance:none; width:22px; height:22px; border-radius:50%; background:#7f9cc4; cursor:pointer;}";
    
    html += ".memory-title{text-align:left; font-size:14px; color:#a0a5af; margin-bottom:8px;}";
    html += ".memory-grid{display:grid; grid-template-columns: repeat(3, 1fr); gap:10px; margin-bottom:25px;}";
    html += ".btn-mem{padding:12px 5px; border:1px solid #3d424d; border-radius:6px; background-color:#1e2127; color:#b0c4de; font-size:14px; font-weight:bold; cursor:pointer; -webkit-user-select:none; user-select:none; transition: all 0.2s;}";
    html += ".btn-mem:active{background-color:#3d424d; color:#fff;}";
    
    html += ".link-setting{display:inline-block; font-size:13px; color:#5c616b; text-decoration:none; margin-top:10px;} .link-setting:hover{color:#7f9cc4;}";
    html += "</style>";
    
    html += "<script>";
    html += "var currentPercent = " + String(percent) + ";";
    html += "var touchTimer = null;";
    html += "var isLongTouch = false;";
    
    html += "function updateStatus() {";
    html += "  fetch('/status').then(res => res.text()).then(data => {";
    html += "    var arr = data.split(',');";
    html += "    currentPercent = parseInt(arr);";
    html += "    var txt = arr;";
    html += "    document.getElementById('v').innerText = currentPercent + '%';";
    html += "    document.getElementById('t').innerText = txt;";
    
    // Если пользователь ПРЯМО СЕЙЧАС НЕ ТАЩИТ ползунок, подтягиваем его к реальному положению окна
    html += "    if(document.activeElement !== document.getElementById('s')) {";
    html += "      document.getElementById('s').value = currentPercent;";
    html += "      document.getElementById('sl-hint').innerText = currentPercent + '%';"; // Обновляем подсказку тоже
    html += "    }";
    
    html += "    var angle = (currentPercent / 100) * 45; opacity = 1;";
    html += "    document.getElementById('w').style.transform = 'rotate(' + angle + 'deg)';";
    html += "    for(var i=1; i<=6; i++) {";
    html += "      var mVal = parseInt(arr[1+i]);";
    html += "      var b = document.getElementById('m'+i);";
    html += "      if(mVal >= 0 && mVal <= 100) { b.innerText = mVal + '%'; b.style.borderColor = '#7f9cc4'; }";
    html += "      else { b.innerText = 'M' + i; b.style.borderColor = '#3d424d'; }";
    html += "    }";
    html += "  });";
    html += "}";
    
    // Логика живого обновления подсказки при перетаскивании бегунка пальцем
    html += "function onSliderInput(val) {";
    html += "  document.getElementById('sl-hint').innerText = val + '%';"; // Мгновенно пишем цифру над слайдером
    html += "}";
    
    html += "function startMem(id) {";
    html += "  isLongTouch = false;";
    html += "  touchTimer = setTimeout(function() {";
    html += "    isLongTouch = true;";
    html += "    var b = document.getElementById('m'+id);";
    html += "    b.style.backgroundColor = '#7fc49c'; b.style.color = '#22252a';";
    html += "    fetch('/mem_save?id=' + id + '&pos=' + document.getElementById('s').value).then(() => {"; // Берем текущее положение слайдера при сохранении
    html += "      setTimeout(() => { b.style.backgroundColor = '#1e2127'; b.style.color = '#b0c4de'; updateStatus(); }, 500);";
    html += "    });";
    html += "  }, 1500);";
    html += "}";
    
    html += "function endMem(id) {";
    html += "  clearTimeout(touchTimer);";
    html += "  if(!isLongTouch) {";
    html += "    fetch('/mem_go?id=' + id);";
    html += "  }";
    html += "}";
    
    html += "setInterval(updateStatus, 1000);";
    html += "</script>";
    
    html += "</head><body onload='updateStatus()'>";

    html += "<div class='card'><h2>Управление Окном</h2>";
    html += "<div class='status-val' id='v'>" + String(percent) + "%</div>";
    html += "<div class='status-text'>Мотор: <b id='t'>" + status_text + "</b></div>";
    
    html += "<svg class='window-svg' width='160' height='140' viewBox='0 0 160 140'>";
    html += "  <line x1='30' y1='110' x2='140' y2='30' class='window-frame' />";
    html += "  <line x1='30' y1='110' x2='140' y2='30' class='window-sash' id='w' />"; 
    html += "</svg>";

    html += "<div class='btn-block' oncontextmenu='return false;'>";
    html += "  <div class='btn-row'>";
    html += "    <button class='btn btn-auto-close' onclick=\"fetch('/close'); setTimeout(updateStatus, 100);\">Авт. Закр.</button>";
    html += "    <button class='btn btn-stop' onclick=\"fetch('/stop'); setTimeout(updateStatus, 100);\">СТОП</button>";
    html += "    <button class='btn btn-auto-open' onclick=\"fetch('/open'); setTimeout(updateStatus, 100);\">Авт. Откр.</button>";
    html += "  </div>";
    html += "  <div class='btn-row'>";
    html += "    <button class='btn btn-man-close' onmousedown=\"fetch('/close')\" ontouchstart=\"fetch('/close')\" onmouseup=\"fetch('/stop')\" ontouchend=\"fetch('/stop')\">Ручн. Закр.</button>";
    html += "<div class='btn-spacer'></div>"; 
    html += "    <button class='btn btn-man-open' onmousedown=\"fetch('/open')\" ontouchstart=\"fetch('/open')\" onmouseup=\"fetch('/stop')\" ontouchend=\"fetch('/stop')\">Ручн. Откр.</button>";
    html += "  </div>";
    html += "</div>";

    // Обновленная строка над слайдером: добавлена разметка для вывода живых процентов
    html += "<div class='slider-title'>Выставить положение: <span class='slider-val-hint' id='sl-hint'>" + String(percent) + "%</span></div>";
    // Добавлено событие oninput, которое мгновенно вызывает функцию onSliderInput при перетаскивании бегунка
    html += "<input type='range' min='0' max='100' value='" + String(percent) + "' class='slider' id='s' oninput='onSliderInput(this.value)' onchange=\"fetch('/set?pos='+this.value); setTimeout(updateStatus, 100);\">";

    html += "<div class='memory-title'>Кнопки быстрой памяти положения:</div>";
    html += "<div class='memory-grid' oncontextmenu='return false;'>";
    for(int i=1; i<=6; i++) {
        html += "  <button class='btn-mem' id='m" + String(i) + "' ";
        html += "onmousedown='startMem(" + String(i) + ")' ontouchstart='startMem(" + String(i) + ")' ";
        html += "onmouseup='endMem(" + String(i) + ")' ontouchend='endMem(" + String(i) + ")'>M" + String(i) + "</button>";
    }
    html += "</div>";

    html += "<a href='/setting' class='link-setting'>Сервисные настройки системы</a>";
    html += "</div></body></html>";
    
    return html;
}
