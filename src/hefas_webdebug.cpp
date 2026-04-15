/**
 * HEFAS – Modul diagnostyczny WiFi (WebDebug)
 *
 * ESP32 stawia wlasna siec WiFi (Access Point) i serwuje strone
 * diagnostyczna na 192.168.4.1 z:
 *   - wizualizacja myszy (klikniecia L/R, scroll, drag),
 *   - oscyloskop ruchu (wykresy dX/dY w czasie rzeczywistym),
 *   - wskaznik kierunku ruchu glowy (joystick),
 *   - panel logow na zywo,
 *   - przyciski: pauza, rekalibracja, czyszczenie logow.
 */

#include "hefas_config.h"

#if WEBDEBUG_AKTYWNY

#include <WiFi.h>
#include <WebServer.h>
#include "hefas_webdebug.h"

extern "C" bool tud_mounted(void);

extern bool     trybScrolla;
extern bool     przytrzymanieAktywne;
extern int      kursorDeltaX;
extern int      kursorDeltaY;
extern uint32_t licznikKlikLewych;
extern uint32_t licznikKlikPrawych;

bool webPauzaMyszy = false;
bool webZadanieRekalibracji = false;

// ======================== BUFOR LOGOW ==============================

static const int ROZMIAR_BUFORA = 80;
static String    buforLogow[ROZMIAR_BUFORA];
static int       indeksZapisu = 0;

// ======================== WEB SERVER ===============================

static WebServer serwer(80);

// ======================== STRONA HTML ==============================

static const char STRONA_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="pl"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>HEFAS Debug</title>
<style>
:root{--bg:#0f0f1a;--pn:#1a1a2e;--bd:#2a2a4e;--tx:#e0e0e0;--ac:#00d4ff;--rd:#e94560;--gn:#4ecca3;--yl:#f39c12}
*{margin:0;padding:0;box-sizing:border-box}
body{background:var(--bg);color:var(--tx);font-family:'Courier New',monospace;padding:10px;max-width:900px;margin:0 auto}
h1{color:var(--ac);font-size:1.15em}
.top{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;flex-wrap:wrap;gap:6px}
.dots{font-size:.78em;display:flex;gap:12px;align-items:center}
.dot{width:9px;height:9px;border-radius:50%;display:inline-block;margin-right:3px;background:#444}
.dot.on{background:var(--gn);box-shadow:0 0 6px var(--gn)}
.grid{display:grid;grid-template-columns:170px 1fr;gap:10px;margin-bottom:10px}
@media(max-width:560px){.grid{grid-template-columns:1fr}}
.pn{background:var(--pn);border:1px solid var(--bd);border-radius:8px;padding:12px}
.mv{display:flex;flex-direction:column;align-items:center;gap:8px}
.mv svg{width:90px}
.cnt{font-size:.78em;text-align:center;line-height:1.9}
.cnt b{color:var(--ac)}
.cnt .vl{font-size:1.1em}
.jc{border:1px solid var(--bd);border-radius:50%;background:#0a0a15}
.chw{position:relative}
.chw canvas{width:100%;height:200px;display:block;border-radius:6px}
.leg{font-size:.7em;position:absolute;top:6px;right:10px;background:rgba(15,15,26,.8);padding:2px 6px;border-radius:4px}
#lg{height:22vh;overflow-y:auto;font-size:.72em;line-height:1.5;background:#0a0a15;border:1px solid var(--bd);border-radius:6px;padding:8px;margin-bottom:10px}
#lg div{border-bottom:1px solid #111;padding:1px 0}
.br{display:flex;gap:8px;flex-wrap:wrap}
.br button{flex:1;min-width:100px;padding:10px;border:none;border-radius:6px;font-size:.9em;cursor:pointer;font-weight:700;transition:background .2s}
.bp{background:var(--rd);color:#fff}.bp.on{background:var(--gn)}
.bk{background:var(--yl);color:#111}
.bc{background:var(--bd);color:var(--tx)}
.br button:active{opacity:.7}
</style></head><body>
<div class="top">
<h1>HEFAS Debug Monitor</h1>
<div class="dots">
<span><span class="dot" id="du"></span>USB</span>
<span><span class="dot" id="ds"></span>Scroll</span>
<span><span class="dot" id="dd"></span>Drag</span>
<span><span class="dot" id="dp"></span>Pauza</span>
</div></div>
<div class="grid">
<div class="pn mv">
<svg viewBox="0 0 120 170">
<ellipse cx="60" cy="105" rx="44" ry="62" fill="#2a2a4e" stroke="#555" stroke-width="1.5"/>
<path d="M16,80 C16,35 60,18 60,18 L60,80 Z" id="ml" fill="#2a2a4e" stroke="#555" stroke-width="1.5"/>
<path d="M104,80 C104,35 60,18 60,18 L60,80 Z" id="mr" fill="#2a2a4e" stroke="#555" stroke-width="1.5"/>
<rect x="52" y="48" width="16" height="26" rx="8" id="mw" fill="#3a3a5e" stroke="#555"/>
<line x1="60" y1="18" x2="60" y2="80" stroke="#555" stroke-width="1.5"/>
<polygon points="60,35 54,44 66,44" id="su" fill="none" stroke="#555" stroke-width="1"/>
<polygon points="60,67 54,58 66,58" id="sdn" fill="none" stroke="#555" stroke-width="1"/>
</svg>
<canvas class="jc" id="joy" width="80" height="80"></canvas>
<div class="cnt">
L: <b id="cl" class="vl">0</b> &nbsp; R: <b id="cr" class="vl">0</b><br>
dX:<b id="vx">0</b> dY:<b id="vy">0</b>
</div></div>
<div class="pn chw">
<canvas id="ch"></canvas>
<div class="leg"><span style="color:#e94560">&#9632; dX</span> &nbsp;<span style="color:#00d4ff">&#9632; dY</span></div>
</div></div>
<div id="lg"></div>
<div class="br">
<button class="bp" id="bp" onclick="fetch('/pauza')">PAUZA</button>
<button class="bk" onclick="fetch('/rekalibracja')">REKALIBRACJA</button>
<button class="bc" onclick="document.getElementById('lg').innerHTML=''">WYCZYSC</button>
</div>
<script>
var MH=200,hx=[],hy=[],li=0,plc=0,prc=0,lg=document.getElementById('lg');
function sC(){var c=document.getElementById('ch');c.width=c.parentElement.clientWidth-26;c.height=200}
window.addEventListener('resize',sC);sC();

function dS(x,d,w,h,pk,col){if(d.length<2)return;x.strokeStyle=col;x.lineWidth=1.5;x.beginPath();
for(var i=0;i<d.length;i++){var px=i/(MH-1)*w,py=h/2-d[i]/pk*(h/2-14);i?x.lineTo(px,py):x.moveTo(px,py)}x.stroke()}

function dCh(){var c=document.getElementById('ch'),x=c.getContext('2d'),w=c.width,h=c.height;
var pk=5;hx.concat(hy).forEach(function(v){if(Math.abs(v)>pk)pk=Math.abs(v)});pk=Math.ceil(pk*1.2);if(pk<5)pk=5;
x.fillStyle='#0a0a15';x.fillRect(0,0,w,h);
x.strokeStyle='#1a1a3e';x.lineWidth=.5;
for(var i=1;i<4;i++){var y=h*i/4;x.beginPath();x.moveTo(0,y);x.lineTo(w,y);x.stroke()}
x.strokeStyle='#333';x.lineWidth=1;x.beginPath();x.moveTo(0,h/2);x.lineTo(w,h/2);x.stroke();
dS(x,hx,w,h,pk,'#e94560');dS(x,hy,w,h,pk,'#00d4ff');
x.fillStyle='#555';x.font='10px monospace';x.textAlign='left';
x.fillText('+'+pk,4,12);x.fillText('0',4,h/2-4);x.fillText('-'+pk,4,h-4)}

function dJ(dx,dy){var c=document.getElementById('joy'),x=c.getContext('2d'),w=c.width,h=c.height,cx=w/2,cy=h/2;
x.clearRect(0,0,w,h);
x.strokeStyle='#222';x.lineWidth=.5;x.beginPath();x.moveTo(cx,0);x.lineTo(cx,h);x.moveTo(0,cy);x.lineTo(w,cy);x.stroke();
x.strokeStyle='#2a2a4e';x.lineWidth=1;x.beginPath();x.arc(cx,cy,32,0,Math.PI*2);x.stroke();
x.beginPath();x.arc(cx,cy,16,0,Math.PI*2);x.stroke();
var s=1.5,px=cx+Math.max(-32,Math.min(32,dx*s)),py=cy+Math.max(-32,Math.min(32,dy*s));
x.fillStyle='#00d4ff';x.shadowColor='#00d4ff';x.shadowBlur=8;x.beginPath();x.arc(px,py,5,0,Math.PI*2);x.fill();
x.shadowBlur=0}

function fl(id,col){var e=document.getElementById(id);e.setAttribute('fill',col);
setTimeout(function(){e.setAttribute('fill','#2a2a4e')},350)}

function sd(id,on){document.getElementById(id).className='dot'+(on?' on':'')}

async function po(){try{
var r=await(await fetch('/logi?od='+li)).json();
r.l.forEach(function(t){var d=document.createElement('div');d.textContent=t;lg.appendChild(d)});
if(r.l.length)lg.scrollTop=lg.scrollHeight;
li=r.i;
hx.push(r.dx);hy.push(r.dy);if(hx.length>MH)hx.shift();if(hy.length>MH)hy.shift();
document.getElementById('vx').textContent=r.dx;
document.getElementById('vy').textContent=r.dy;
document.getElementById('cl').textContent=r.lc;
document.getElementById('cr').textContent=r.rc;
sd('du',r.u);sd('ds',r.s);sd('dd',r.d);sd('dp',r.p);
if(r.lc>plc)fl('ml','#e94560');if(r.rc>prc)fl('mr','#00d4ff');
plc=r.lc;prc=r.rc;
document.getElementById('mw').setAttribute('fill',r.s?'#4ecca3':'#3a3a5e');
if(r.d)document.getElementById('ml').setAttribute('fill','#e94560');
var b=document.getElementById('bp');b.textContent=r.p?'WZNOW':'PAUZA';b.className='bp'+(r.p?' on':'');
dCh();dJ(r.dx,r.dy)}catch(e){}}
setInterval(po,300);
</script></body></html>
)rawliteral";

// ====================== HANDLERY HTTP ==============================

static void obsluzStrone() {
    serwer.send(200, "text/html", STRONA_HTML);
}

static void obsluzLogi() {
    int od = serwer.hasArg("od") ? serwer.arg("od").toInt() : 0;

    int najstarszy = indeksZapisu - ROZMIAR_BUFORA;
    if (najstarszy < 0) najstarszy = 0;
    int start = (od > najstarszy) ? od : najstarszy;

    String json;
    json.reserve(768);
    json = "{\"l\":[";

    bool pierwszy = true;
    for (int idx = start; idx < indeksZapisu; idx++) {
        if (!pierwszy) json += ',';
        json += '"';
        String& wpis = buforLogow[idx % ROZMIAR_BUFORA];
        for (unsigned int c = 0; c < wpis.length(); c++) {
            if (wpis[c] == '"') json += "\\\"";
            else if (wpis[c] == '\\') json += "\\\\";
            else json += wpis[c];
        }
        json += '"';
        pierwszy = false;
    }

    json += "],\"i\":";   json += String(indeksZapisu);
    json += ",\"dx\":";   json += String(kursorDeltaX);
    json += ",\"dy\":";   json += String(kursorDeltaY);
    json += ",\"lc\":";   json += String(licznikKlikLewych);
    json += ",\"rc\":";   json += String(licznikKlikPrawych);
    json += ",\"u\":";    json += tud_mounted() ? "true" : "false";
    json += ",\"s\":";    json += trybScrolla ? "true" : "false";
    json += ",\"d\":";    json += przytrzymanieAktywne ? "true" : "false";
    json += ",\"p\":";    json += webPauzaMyszy ? "true" : "false";
    json += '}';

    serwer.send(200, "application/json", json);
}

static void obsluzPauze() {
    webPauzaMyszy = !webPauzaMyszy;
    serwer.send(200, "text/plain", webPauzaMyszy ? "PAUZA" : "AKTYWNA");
}

static void obsluzRekalibracje() {
    webZadanieRekalibracji = true;
    serwer.send(200, "text/plain", "OK");
}

// ====================== API PUBLICZNE ==============================

void webDebugInit() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WEBDEBUG_SSID, WEBDEBUG_HASLO);

    serwer.on("/",              obsluzStrone);
    serwer.on("/logi",          obsluzLogi);
    serwer.on("/pauza",         obsluzPauze);
    serwer.on("/rekalibracja",  obsluzRekalibracje);
    serwer.begin();

    Serial.print(F("[WEBDEBUG] AP: "));
    Serial.println(WEBDEBUG_SSID);
    Serial.print(F("[WEBDEBUG] Haslo: "));
    Serial.println(WEBDEBUG_HASLO);
    Serial.print(F("[WEBDEBUG] Strona: http://"));
    Serial.println(WiFi.softAPIP());
}

void webDebugLoop() {
    serwer.handleClient();
}

void webDebugLog(const char* wiadomosc) {
    buforLogow[indeksZapisu % ROZMIAR_BUFORA] = String(wiadomosc);
    indeksZapisu++;
}

void webDebugLog(const String& wiadomosc) {
    buforLogow[indeksZapisu % ROZMIAR_BUFORA] = wiadomosc;
    indeksZapisu++;
}

#endif // WEBDEBUG_AKTYWNY
