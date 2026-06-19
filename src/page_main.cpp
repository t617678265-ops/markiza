#include "page_main.h"

String get_page_main(int percent, String status_text) {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=no'>";

    html += "<link rel='icon' href='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAACXBIWXMAAAsTAAALEwEAmpwYAAAEs0lEQVR4nMWX60+TVxzHzyuXvZhcyx26LMuW7I3ZH7DsrcmGMud1XBwXL1PAIYlZyMQ5KC6yUqcM0KkIiNra4gSpaLm1agsEabkWKAyEUoG2tOVpSzHgd3meghtIS9NscJJPnhfn5Pmc88vznHO+hCy1D9PE77yfKUuPOiltYWdIreyMZrAzmhD1fSOiTjQgKr0Bken1iEyTIDL1MSJSHyHieB0ijj1E+DExwr+rRfjRBwg7WoOwI9UIO3wfYYf/ROihewhNqUJossgakixsCU4SpNEu8u8WkSEPZ2fKVOxMGdgnpfgf5AhJFiIk6S5CEgUITryj9E8QhL9Z+cbK+Qj+lo/ghNtKphJ02TdcfvAOgg7eBiuhMpVEZcpaN0MelHALrPhKBWGflFKbIQ+Kr0RgfAVFPJVvO9UAbu0QeGInBWINCmppBp08GHBS0w/uG9Tg1vSBW+3kULH8jZwVfxOsuAoQT1d+pXEUreNzqFZTXtGmncOD57oVclZsOYinZdeaHDjXOIWzkkmv6NTZEcORrJCzYstAPJHvzJejc8KO07XjXnH2oRYTJgeCEypXyAO/uQHiyQcnatOhSKpDOn/YK0oVk7haP/iWPPBAKch6cvbxWujMDqTdGsTxygGv6NZa8dkPNW/JA/dfB1nvV0ssacOjHgNiS7q84khpLwZeUmvKA/ZfA1nvP2/q0yOLP4ADhUqvKJWOgyNQrikP2HcVxJ384xNijOjt2H5O4TV9ExS2pd1dUx6w9w8QdztcNr8Hj1RTyCzr8opcoRrK0RmXcv+9V0Dcba+qF2Y8HrahbsgG9eQcZuYA6ytgfgFYWAQcCwA1D+htr9E+ZodYY8P9fiuEfU46X84hq7zNpdx/z2UQV/JPT9Vh1DiHi61mXOuwQDIwi3HzIox2YHbeORGLA9DbgSH9K1R1m3G53YKSdjPuqa3Mc9RgxwfJlS7l/rtLQFwdLJyqXjzUWHH+2QwutZpQrrJApbVDR72G3gYY7MC0DXhhWsSzvyhmkgUKEzOeprzTgjql1q3cb3cxiKtTrV9Hgdcyg7wnRuaFhW0mVCjNkI9YoZ6ch0b/Cr0vHWgapHC9w4wLLU45PZ7m6QiFuHyJW7nf10Uga8k/z5ZApbXiZ6kBv8qNuNllQY7UgALFDIraTLjRYYKoZxbX203MxPKfGZmxoj6KGZcjM2DMYENoXKlbud+u30HWOs+LH2twp8eC7CY9ChRG5rnMWakeeU8M4PdawJEZ8FPzP325MgN+eWpkyl/RrFlX7rurEGS1PCxFhOFpG8406ZHVMO0SujKu+hSjs9h+unpdue9XzARW3mR2nZfiycgsTkmmvOLHxmloJikE7ltf7htzCWT1NYovH0PTiA1lKrNX1A9TuHC/0yO5b8xFkNV3uENFcuRVdSNPtEwX8oQ0neDQ3FUtoQRH0OGE/xy5S+TcbsdHKeUeyX12/gYSmiSiPLlArr7JuNtePZVv3cGzkJAkYetmyH12XIDPlzwFobPapsijedgazU1lohmd1TZa/l40V0n2nNnC5EM6KNJZbSPl737BdYbT5fbJHsEWOqvRcYlOLP/5BxfNo3yiC+RM2ZdXTgj5Gwsmx7tsNfyxAAAAAElFTkSuQmCC' type='image/png'>";
    html += "<link rel='apple-touch-icon' href='data:image/png;base64,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' type='image/png'>";

    html += "<style>";
    html += "body{background-color:#22252a;color:#e2e5e9;font-family:sans-serif;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0;padding:20px;box-sizing:border-box;}";
    html += ".card{background-color:#2d3139;padding:25px;border-radius:16px;box-shadow:0 6px 20px rgba(0,0,0,0.4);width:100%;max-width:400px;text-align:center;}";
    html += "h2{color:#b0c4de;margin:0 0 5px 0;}";
    html += ".status-val{font-size:32px;font-weight:bold;color:#fff;margin:10px 0 20px 0;}";
    html += ".window-svg{background:#1e2127;border-radius:8px;margin-bottom:20px;border:1px solid #3d424d;display:block;margin-left:auto;margin-right:auto;}";
    html += ".btn-block{display:flex;flex-direction:column;gap:10px;margin-bottom:25px;}";
    html += ".btn-row{display:flex;gap:10px;}";
    html += ".btn{flex:1;padding:15px 10px;border:none;border-radius:8px;font-size:15px;font-weight:bold;cursor:pointer;-webkit-user-select:none;user-select:none;-webkit-touch-callout:none;}";
    html += ".btn-auto-close{background-color:#7f9cc4;color:#22252a;}";
    html += ".btn-stop{background-color:#c47f7f;color:#22252a;}";
    html += ".btn-auto-open{background-color:#7fc49c;color:#22252a;}";
    html += ".slider-title{text-align:left;font-size:14px;color:#a0a5af;margin-bottom:10px;display:flex;justify-content:space-between;align-items:center;}";
    html += ".slider-val-hint{color:#7f9cc4;font-weight:bold;font-size:22px;}";
    html += ".slider{width:100%;-webkit-appearance:none;height:8px;border-radius:4px;background:#1e2127;outline:none;margin-bottom:30px;}";
    html += ".slider::-webkit-slider-thumb{-webkit-appearance:none;width:22px;height:22px;border-radius:50%;background:#7f9cc4;cursor:pointer;}";
    html += ".memory-title{text-align:left;font-size:14px;color:#a0a5af;margin-bottom:8px;}";
    html += ".memory-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-bottom:25px;}";
    html += ".btn-mem{padding:12px 5px;border:1px solid #3d424d;border-radius:6px;background-color:#1e2127;color:#b0c4de;font-size:14px;font-weight:bold;cursor:pointer;-webkit-user-select:none;user-select:none;transition:all 0.2s;}";
    html += ".btn-mem:active{background-color:#3d424d;color:#fff;}";
    html += "</style>";

    html += "<script>";
    html += "var currentPercent = " + String(percent) + ";";
    html += "var touchTimer = null;";
    html += "var isLongTouch = false;";
    html += "var isDragging = false;";
    html += "function updateWindow(pct) {";
    html += "  var top=43,bot=156,h=113;";
    html += "  var blindBot=bot-h*(pct/100);";
    html += "  var bL=69+(blindBot-top)*(29-69)/h;";
    html += "  var bR=170+(blindBot-top)*(130-170)/h;";
    html += "  document.getElementById('blind').setAttribute('points','69,43 170,43 '+bR+','+blindBot+' '+bL+','+blindBot);";
    html += "  document.getElementById('roller').setAttribute('x1',bL);";
    html += "  document.getElementById('roller').setAttribute('y1',blindBot);";
    html += "  document.getElementById('roller').setAttribute('x2',bR);";
    html += "  document.getElementById('roller').setAttribute('y2',blindBot);";
    html += "  document.getElementById('sunClipRect').setAttribute('y',blindBot);";
    html += "  document.getElementById('sunClipRect').setAttribute('height',bot-blindBot+10);";
    html += "}";
    html += "function updateStatus(){";
    html += "  fetch('/status').then(res=>res.text()).then(data=>{";
    html += "    var arr=data.split(',');";
    html += "    var sliderTarget=parseInt(arr[0]);";
    html += "    currentPercent=parseInt(arr[1]);";
    html += "    document.getElementById('v').innerText=currentPercent+'%';";
    html += "    if(!isDragging){";
    html += "      document.getElementById('s').value=sliderTarget;";
    html += "      document.getElementById('sl-hint').innerText=sliderTarget+'%';";
    html += "    }";
    html += "    updateWindow(currentPercent);";
    html += "    for(var i=1;i<=6;i++){";
    html += "      var mVal=parseInt(arr[2+i]);";
    html += "      var b=document.getElementById('m'+i);";
    html += "      if(mVal>=0&&mVal<=100){b.innerText=mVal+'%';b.style.borderColor='#7f9cc4';}";
    html += "      else{b.innerText='M'+i;b.style.borderColor='#3d424d';}";
    html += "    }";
    html += "  });";
    html += "}";
    html += "function onSliderInput(val){";
    html += "  document.getElementById('sl-hint').innerText=val+'%';";
    html += "  updateWindow(parseInt(val));";
    html += "}";
    html += "function startMem(id){";
    html += "  isLongTouch=false;";
    html += "  touchTimer=setTimeout(function(){";
    html += "    isLongTouch=true;";
    html += "    var b=document.getElementById('m'+id);";
    html += "    b.style.backgroundColor='#7fc49c';b.style.color='#22252a';";
    html += "    fetch('/mem_save?id='+id+'&pos='+currentPercent).then(()=>{";
    html += "      setTimeout(()=>{b.style.backgroundColor='#1e2127';b.style.color='#b0c4de';updateStatus();},500);";
    html += "    });";
    html += "  },400);";
    html += "}";
    html += "function endMem(id){";
    html += "  clearTimeout(touchTimer);";
    html += "  if(!isLongTouch){fetch('/mem_go?id='+id);}";
    html += "}";
    html += "setInterval(updateStatus,1000);";
    html += "</script>";

    html += "</head><body onload='updateStatus()'>";
    html += "<div class='card'><h2>Управление Шторкой</h2>";
    html += "<div class='status-val' id='v'>" + String(percent) + "%</div>";

    html += "<svg class='window-svg' width='200' height='200' viewBox='0 0 200 200'>";
    html += "<defs>";
    html += "<linearGradient id='glassGrad' x1='0' y1='0' x2='0' y2='1'>";
    html += "<stop offset='0%' stop-color='#b8ddf8' stop-opacity='0.85'/>";
    html += "<stop offset='100%' stop-color='#70b8e8' stop-opacity='0.7'/>";
    html += "</linearGradient>";
    html += "<linearGradient id='blindGrad' x1='0' y1='0' x2='0' y2='1'>";
    html += "<stop offset='0%' stop-color='#90c8f0'/>";
    html += "<stop offset='100%' stop-color='#3a7abf'/>";
    html += "</linearGradient>";
    html += "<radialGradient id='sunGradDef'>";
    html += "<stop offset='0%' stop-color='#ffffff'/>";
    html += "<stop offset='70%' stop-color='#e8f4ff'/>";
    html += "<stop offset='100%' stop-color='#c0d8f0'/>";
    html += "</radialGradient>";
    html += "<clipPath id='sunClip'>";
    html += "<rect id='sunClipRect' x='0' y='156' width='200' height='10'/>";
    html += "</clipPath>";
    html += "</defs>";
    html += "<polygon points='69,43 170,43 130,156 29,156' fill='url(#glassGrad)'/>";
    html += "<g clip-path='url(#sunClip)'>";
    html += "<circle cx='99' cy='99' r='34' fill='white' opacity='0.15'/>";
    html += "<circle cx='99' cy='99' r='26' fill='url(#sunGradDef)'/>";
    html += "</g>";
    html += "<polygon id='blind' points='69,43 170,43 130,156 29,156' fill='url(#blindGrad)'/>";
    html += "<polygon points='69,43 170,43 130,156 29,156' fill='none' stroke='white' stroke-width='5' stroke-linejoin='round'/>";
    html += "<line id='roller' x1='69' y1='156' x2='130' y2='156' stroke='white' stroke-width='4' stroke-linecap='round' opacity='0.9'/>";
    html += "</svg>";

    html += "<div class='btn-block' oncontextmenu='return false;'>";
    html += "  <div class='btn-row'>";
    html += "    <button class='btn btn-auto-close' onclick=\"fetch('/close');setTimeout(updateStatus,100);\">Авт. Закр.</button>";
    html += "    <button class='btn btn-stop' onclick=\"fetch('/stop');setTimeout(updateStatus,100);\">СТОП</button>";
    html += "    <button class='btn btn-auto-open' onclick=\"fetch('/open');setTimeout(updateStatus,100);\">Авт. Откр.</button>";
    html += "  </div>";
    html += "</div>";

    html += "<div class='slider-title'>Выставить положение: <span class='slider-val-hint' id='sl-hint'>" + String(percent) + "%</span></div>";
    html += "<input type='range' min='0' max='100' value='" + String(percent) + "' class='slider' id='s' ";
    html += "oninput='onSliderInput(this.value)' ";
    html += "onmousedown='isDragging=true' ontouchstart='isDragging=true' ";
    html += "onmouseup='isDragging=false' ontouchend='isDragging=false' ";
    html += "onchange=\"fetch('/set?pos='+this.value);setTimeout(updateStatus,100);\">";

    html += "<div class='memory-title'>Кнопки быстрой памяти положения:</div>";
    html += "<div class='memory-grid' oncontextmenu='return false;'>";
    for(int i=1; i<=6; i++) {
        html += "  <button class='btn-mem' id='m" + String(i) + "' ";
        html += "onmousedown='startMem(" + String(i) + ")' ontouchstart='startMem(" + String(i) + ")' ";
        html += "onmouseup='endMem(" + String(i) + ")' ontouchend='endMem(" + String(i) + ")'>M" + String(i) + "</button>";
    }
    html += "</div>";

    html += "</div></body></html>";

    return html;
}