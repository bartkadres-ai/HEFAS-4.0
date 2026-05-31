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

// Detektor analogowy TCRT5000 (sygnał z fototranzystora)
extern float    tcrtBaseline;
extern float    tcrtFiltered;
extern int      tcrtRaw;
extern bool     wirtualnyStanCzujnika;

// Runtime flaga debug (toggle 6 mrugnięć) — wyświetlana jako dot
extern bool     czyDebugWlaczony;

// Maszyna stanów wieloklików — podgląd na żywo dla diagnostyki
extern uint8_t       licznikImpulsow;
extern unsigned long czasOstatniegoImpulsu;

// Monitor baterii (D16 = GPIO10)
extern bool      bateriaPodlaczona;
extern float     bateriaNapiecie;
extern uint8_t   bateriaProcent;
extern int       bateriaRawAdc;

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
.dot.eye.on{background:var(--rd);box-shadow:0 0 6px var(--rd)}
.dot.bat.lo{background:var(--yl);box-shadow:0 0 6px var(--yl)}
.dot.bat.cr{background:var(--rd);box-shadow:0 0 6px var(--rd)}
.dot.bat.off{background:#555}
.grid{display:grid;grid-template-columns:170px 1fr;gap:10px;margin-bottom:10px}
@media(max-width:560px){.grid{grid-template-columns:1fr}}
.pn{background:var(--pn);border:1px solid var(--bd);border-radius:8px;padding:12px}
.mv{display:flex;flex-direction:column;align-items:center;gap:8px}
.mv svg{width:80px}
.cnt{font-size:.78em;text-align:center;line-height:1.9}
.cnt b{color:var(--ac)}
.cnt .vl{font-size:1.1em}
.jc{border:1px solid var(--bd);border-radius:50%;background:#0a0a15}
.rcol{display:flex;flex-direction:column;gap:10px;min-width:0}
.chw{position:relative}
.chw canvas{width:100%;height:170px;display:block;border-radius:6px}
.chw.ct canvas{height:150px}
.leg{font-size:.7em;position:absolute;top:6px;right:10px;background:rgba(15,15,26,.85);padding:2px 6px;border-radius:4px;line-height:1.4}
.tt{font-size:.7em;position:absolute;top:6px;left:10px;color:#888;background:rgba(15,15,26,.85);padding:2px 6px;border-radius:4px}
#lg{height:18vh;overflow-y:auto;font-size:.72em;line-height:1.5;background:#0a0a15;border:1px solid var(--bd);border-radius:6px;padding:8px;margin-bottom:10px}
.lgd{margin-bottom:10px;background:var(--pn);border:1px solid var(--bd);border-radius:8px;padding:8px 12px;font-size:.78em}
.lgd summary{cursor:pointer;color:var(--ac);font-weight:700;outline:none;padding:2px 0}
.lgd summary:hover{color:var(--gn)}
.lgr{line-height:1.65;padding-top:10px;color:#bbb}
.lgr b{color:var(--yl);display:inline-block;margin-top:6px}
.lgr b:first-child{margin-top:0}
.lgr i{color:var(--ac);font-style:normal;font-weight:700}
.lgr .lk{color:#888}
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
<span><span class="dot" id="dg"></span>Debug</span>
<span><span class="dot eye" id="de"></span>Oko</span>
<span><span class="dot bat" id="db"></span><b id="dbp">--</b>%</span>
</div></div>
<div class="grid">
<div class="pn mv">
<svg viewBox="0 0 80 120">
<rect x="5" y="42" width="70" height="72" rx="35" fill="#2a2a4e" stroke="#555" stroke-width="1.5"/>
<path d="M5,52 Q5,8 40,5 Q75,8 75,52 Z" fill="#2a2a4e" stroke="#555" stroke-width="1.5"/>
<path d="M5,52 Q5,8 40,5 L40,52 Z" id="ml" fill="#333355" stroke="#555" stroke-width="1"/>
<path d="M75,52 Q75,8 40,5 L40,52 Z" id="mr" fill="#333355" stroke="#555" stroke-width="1"/>
<line x1="40" y1="5" x2="40" y2="52" stroke="#555" stroke-width="1"/>
<rect x="34" y="20" width="12" height="22" rx="6" id="mw" fill="#444" stroke="#666" stroke-width="1"/>
</svg>
<canvas class="jc" id="joy" width="80" height="80"></canvas>
<div class="cnt">
L: <b id="cl" class="vl">0</b> &nbsp; R: <b id="cr" class="vl">0</b><br>
dX:<b id="vx">0</b> dY:<b id="vy">0</b><br>
<span style="color:#888">SERIA</span> <b id="im" class="vl" style="color:var(--yl)">0</b><br>
<b id="mss" style="color:#888">--ms</b> / <b id="oww" style="color:#888">400</b><br>
<span style="color:#888">TCRT</span><br>
R:<b id="tr">0</b> F:<b id="tf">0</b><br>
B:<b id="tb">0</b> <b id="te" style="color:var(--gn)">OTW</b><br>
<span style="color:#888">BAT</span> <b id="bvv" style="color:var(--gn)">--V</b><br>
<b id="bpp" style="color:var(--gn)">--%</b> <span style="color:#888;font-size:.85em">pin:<b id="brr">0</b>mV</span>
</div></div>
<div class="rcol">
<div class="pn chw">
<div class="tt">Ruch glowy</div>
<canvas id="ch"></canvas>
<div class="leg"><span style="color:#e94560">&#9632; dX</span> &nbsp;<span style="color:#00d4ff">&#9632; dY</span></div>
</div>
<div class="pn chw ct">
<div class="tt">Sygnal TCRT</div>
<canvas id="cht"></canvas>
<div class="leg"><span style="color:#00d4ff">&#9632; Filt</span> &nbsp;<span style="color:#888">&#9632; Base</span> &nbsp;<span style="color:#e94560">&#9632; Trig</span> &nbsp;<span style="color:#f39c12">&#9632; Rel</span></div>
</div>
</div></div>
<div id="lg"></div>
<details class="lgd">
<summary>Legenda - jak czytac panel</summary>
<div class="lgr">
<b>Kropki statusu (gora ekranu)</b><br>
<i>USB</i> = mysz HID widoczna po USB-C (gdy zgaszone aktywne jest BLE)<br>
<i>Scroll</i> = wlaczony tryb scrolla (ruch glowy = przewijanie zamiast kursora)<br>
<i>Drag</i> = trwa przytrzymanie LPM (drag and drop)<br>
<i>Pauza</i> = mysz spauzowana przyciskiem ponizej<br>
<i>Debug</i> = wlaczone logowanie na Serial (mocniej grzeje)<br>
<i>Oko</i> = detektor TCRT widzi zamkniete oko<br>
<i>BAT</i> = stan ogniwa Akyga. Zielona = OK (&gt; 40%), zolta = nisko (20-40%), czerwona = krytyczna (&lt; 20%), szara = BRAK (&lt; 2.8 V). Przy USB i podlaczonym ogniwie pokazuje tez napięcie i procent.<br>
<b>Monitor baterii (panel BAT)</b><br>
Ogniwo: <i>Akyga Li-Pol 1900 mAh 1S 3,7 V</i> — podlaczone tylko <i>+</i> i <i>−</i> do BAT+/BAT− XIAO;
srodkowy pin JST (NTC) nieuzywany.<br>
Pin <i>D16 = GPIO10 = ADC_BAT</i>, wewnetrzny dzielnik <i>1:11</i> (R9=100K + R8=10K).<br>
Odczyt uzywa <i>analogReadMilliVolts()</i> z atenuacja <i>ADC_11db</i> + trimmed-mean 64 probek.<br>
Pierwsza liczba = napiecie ogniwa Li-Pol [V]. <span class="lk">Pelna = 4.20 V, pusta (cutoff) = 3.20 V.</span><br>
Druga liczba = procent stanu naladowania wyliczony z 8-punktowej krzywej rozladowania Li-Pol.<br>
<span class="lk">Plateau 3.65-4.20 V to ok. 80% pojemnosci - dlatego procent dlugo trzyma sie wysoko.</span><br>
<span class="lk">pin:</span> = napiecie na pinie GPIO10 [mV] usrednione z <i>64 probek</i> — uzyteczne do kalibracji dzielnika.<br>
Kalibracja: <span class="lk">DZIELNIK_BATERII = V_multimetr / (pin_mV / 1000.0)</span> w hefas_config.h.<br>
<span class="lk">(USB)</span> = pomiar wykonany przy podlaczonym USB; napiecie moze byc minimalnie wyzsze (~0.05 V) podczas ladowania.<br>
<i>BRAK</i> = napiecie ponizej 2.5 V — ogniwo niepodlaczone lub zle +/−. Przy OK: pin ok. 300–380 mV, Vbat 3.3–4.2 V.<br>
<b>Licznik serii mrugniec (SERIA)</b><br>
Pierwsza liczba (zolta) = ile mrugniec system zliczyl w trwajacej serii<br>
Druga liczba = czas od ostatniego mrugniecia (zielona = w oknie, czerwona = juz blisko timeoutu)<br>
Trzecia liczba = aktualne okno (OKNO_WIELOKLIKU_MS)<br>
Aby PPM (3 mrug) zadzialal, przerwa miedzy mrugnieciami musi byc &lt; okno<br>
<b>Wykres ruchu glowy</b><br>
Linia <i style="color:#e94560">czerwona</i> = predkosc kursora dX (lewo / prawo z zyroskopu)<br>
Linia <i style="color:#00d4ff">cyjan</i> = predkosc kursora dY (gora / dol)<br>
Liczby <span class="lk">+pk / -pk</span> w rogach = aktualnie wyswietlana skala<br>
Joystick obok = chwilowy wektor ruchu (intensywnosc + kierunek)<br>
<span class="lk">Co mowia liczby:</span> dX / dY to delta HID wysylana co 10 ms.<br>
<span class="lk">1 jednostka &asymp; 1 piksel</span> ruchu kursora (bez akceleracji systemu).<br>
np. dX = 10 -&gt; kursor leci o 10 px / tick = <b>1000 px/s</b> (ok. 1/2 ekranu FullHD na sekunde).<br>
Wzor: dX = predkosc_obrotu_glowy [&deg;/s] &times; CZULOSC_MYSZY (0.4).<br>
Czyli obrot szyja 50 &deg;/s -&gt; dX = 20 -&gt; <b>2000 px/s</b>. Max HID = 127.<br>
<b>Wykres sygnalu TCRT</b><br>
Linia <i style="color:#00d4ff">cyjan</i> = sygnal po filtracji EMA (to co widzi algorytm)<br>
<i style="color:#888">Szara</i> pozioma = baseline (oko otwarte, ambient light)<br>
<i style="color:#e94560">Czerwona</i> pozioma = prog TRIGGER (przekroczenie w dol = oko zamknieto)<br>
<i style="color:#f39c12">Zolta</i> pozioma = prog RELEASE (powrot nad nia = oko otwarte)<br>
Czerwone tlo = w tej chwili wykryto zamkniete oko<br>
Liczby R / F / B obok = surowy ADC / po filtrze / baseline<br>
<span class="lk">Co mowia liczby:</span> wartosci sa w jednostkach ADC ESP32-S3 (<b>12 bit, zakres 0-4095</b>).<br>
0 = 0 V na pinie AO czujnika, 4095 = 3.3 V. <span class="lk">1 jednostka &asymp; 0.8 mV.</span><br>
Wyzsza wartosc = mniej swiatla IR odbitego (oko otwarte, fototranzystor "widzi gleboko").<br>
Nizsza wartosc = wiecej odbicia (powieka blizej czujnika -&gt; jasniej dla IR).<br>
Typowy Baseline siedzi w okolicach 1500-2500 (zalezy od montazu i oswietlenia).<br>
Mrugniecie = spadek o min. <b>OFFSET_TRIGGER = 350</b> ADC (ok. 280 mV) ponizej Baseline.<br>
Powrot = wzrost ponad Baseline - <b>OFFSET_RELEASE = 120</b> ADC (ok. 96 mV).<br>
Roznica 350 vs 120 = histereza, broni przed migotaniem przy szumie.<br>
Zdarzenie mechaniczne = spadek glebszy niz Baseline - <b>OFFSET_MAX_ZWARCIA = 800</b> ADC (zdjecie okularow, zaslon czujnika) — system ignoruje i resetuje do "otwarte".<br>
<b>Mapowanie mrugniec (czasy z hefas_config.h)</b><br>
1 mrug -&gt; LPM &nbsp; 2 mrug -&gt; double-click LPM &nbsp; 3 mrug -&gt; PPM<br>
4 mrug -&gt; toggle scrolla &nbsp; 5 mrug -&gt; rekalibracja &nbsp; 6 mrug -&gt; toggle debug<br>
<span class="lk">Aby zliczyc serie:</span> przerwa miedzy mrugnieciami &lt; <b>OKNO_WIELOKLIKU_MS = 400 ms</b>.<br>
<span class="lk">Aby mrugniecie sie liczolo:</span> zamkniecie oka min. <b>CZAS_MIN_MRUG_MS = 80 ms</b> (krotsze = ruch glowy, ignorowane).<br>
<span class="lk">Aby uniknac drag:</span> otworz oko wyraznie miedzy mrugnieciami — EMA musi zdazyc wykryc otwarcie.<br>
<span class="lk">Aby zrobic drag:</span> trzymaj oko zamkniete CIAGLE dluzej niz 500 ms.<br>
<span class="lk">Baseline adaptuje sie</span> do zmian swiatla/pozycji glowy na biezaco (EMA_ALPHA_WOLNY = 0.005, stala czasowa ~2 s).<br>
<b>Przyciski</b><br>
<i>PAUZA / WZNOW</i> = blokuje wysylanie ruchu i klikniec (logika dalej dziala)<br>
<i>REKALIBRACJA</i> = ponowna kalibracja zyroskopu (offset Gx/Gy/Gz), bez baseline TCRT<br>
<i>WYCZYSC</i> = czysci historie logow w tym panelu (nie kasuje danych w MCU)<br>
<b>Czestotliwosci</b><br>
Petla glowna = 100 Hz (OKRES_PETLI_MS = 10 ms) - tyle razy na sekunde liczymy detektor i ruch.<br>
Panel WWW odswieza sie co 160 ms (~6 Hz) - dlatego krotkie mrugniecia widac jako pojedynczy punkt.<br>
Historia wykresow = 200 probek = ok. 32 sekundy przy obecnym tempie odpytywania.
</div>
</details>
<div class="br">
<button class="bp" id="bp" onclick="fetch('/pauza',{cache:'no-store'}).catch(function(){})">PAUZA</button>
<button class="bk" onclick="fetch('/rekalibracja',{cache:'no-store'}).catch(function(){})">REKALIBRACJA</button>
<button class="bc" onclick="document.getElementById('lg').innerHTML=''">WYCZYSC</button>
</div>
<script>
var MH=200,hx=[],hy=[],hf=[],he=[],li=0,plc=0,prc=0,lg=document.getElementById('lg');
var IDLE_BTN='#333355',DRAG_L='#e94560';
var lastSnap=null,mlTok=null,mrTok=null,pollGen=0,POLL_MS=160;

function sC(){
 var c=document.getElementById('ch');c.width=c.parentElement.clientWidth-26;c.height=170;
 var t=document.getElementById('cht');t.width=t.parentElement.clientWidth-26;t.height=150;
}
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

function dCT(base,trig,rel){
 var c=document.getElementById('cht');if(!c)return;var x=c.getContext('2d'),w=c.width,h=c.height;
 if(hf.length<2){x.fillStyle='#0a0a15';x.fillRect(0,0,w,h);return}
 var Tline=base-trig,Rline=base-rel;
 var lo=Tline,hi=base;
 for(var i=0;i<hf.length;i++){if(hf[i]<lo)lo=hf[i];if(hf[i]>hi)hi=hf[i]}
 var pad=(hi-lo)*0.12+8;lo-=pad;hi+=pad/2;
 var rng=hi-lo;if(rng<1)rng=1;
 function Y(v){return h-((v-lo)/rng)*(h-2)-1}
 x.fillStyle='#0a0a15';x.fillRect(0,0,w,h);
 for(var i=0;i<hf.length;i++){if(he[i]){var px=i/(MH-1)*w,pw=(w/MH)+1;x.fillStyle='rgba(233,69,96,.18)';x.fillRect(px,0,pw,h)}}
 x.strokeStyle='#1a1a3e';x.lineWidth=.5;
 for(var i=1;i<4;i++){var y=h*i/4;x.beginPath();x.moveTo(0,y);x.lineTo(w,y);x.stroke()}
 x.setLineDash([4,3]);
 x.strokeStyle='#888';x.lineWidth=1;x.beginPath();x.moveTo(0,Y(base));x.lineTo(w,Y(base));x.stroke();
 x.strokeStyle='#e94560';x.beginPath();x.moveTo(0,Y(Tline));x.lineTo(w,Y(Tline));x.stroke();
 x.strokeStyle='#f39c12';x.beginPath();x.moveTo(0,Y(Rline));x.lineTo(w,Y(Rline));x.stroke();
 x.setLineDash([]);
 x.strokeStyle='#00d4ff';x.lineWidth=1.6;x.beginPath();
 for(var i=0;i<hf.length;i++){var px=i/(MH-1)*w,py=Y(hf[i]);i?x.lineTo(px,py):x.moveTo(px,py)}x.stroke();
 x.fillStyle='#888';x.font='10px monospace';x.textAlign='left';
 x.fillText('B '+Math.round(base),4,Math.max(10,Y(base)-2));
 x.fillStyle='#e94560';x.fillText('T '+Math.round(Tline),4,Math.min(h-2,Y(Tline)+10));
 x.fillStyle='#f39c12';x.fillText('R '+Math.round(Rline),4,Math.min(h-2,Y(Rline)+10));
 x.fillStyle='#555';x.textAlign='right';x.fillText(Math.round(hi),w-4,10);x.fillText(Math.round(lo),w-4,h-4)
}

function dJ(dx,dy){var c=document.getElementById('joy'),x=c.getContext('2d'),w=c.width,h=c.height,cx=w/2,cy=h/2;
x.clearRect(0,0,w,h);
x.strokeStyle='#222';x.lineWidth=.5;x.beginPath();x.moveTo(cx,0);x.lineTo(cx,h);x.moveTo(0,cy);x.lineTo(w,cy);x.stroke();
x.strokeStyle='#2a2a4e';x.lineWidth=1;x.beginPath();x.arc(cx,cy,32,0,Math.PI*2);x.stroke();
x.beginPath();x.arc(cx,cy,16,0,Math.PI*2);x.stroke();
var s=1.5,px=cx+Math.max(-32,Math.min(32,dx*s)),py=cy+Math.max(-32,Math.min(32,dy*s));
x.fillStyle='#00d4ff';x.shadowColor='#00d4ff';x.shadowBlur=8;x.beginPath();x.arc(px,py,5,0,Math.PI*2);x.fill();
x.shadowBlur=0}

function syncMouseSvg(r){
var ml=document.getElementById('ml'),mr=document.getElementById('mr');
if(r.d){ml.setAttribute('fill',DRAG_L);}
else if(!mlTok){ml.setAttribute('fill',IDLE_BTN);}
if(!mrTok){mr.setAttribute('fill',IDLE_BTN);}
}

function flashMl(){if(lastSnap&&lastSnap.d)return;
clearTimeout(mlTok);var ml=document.getElementById('ml');ml.setAttribute('fill','#e94560');
mlTok=setTimeout(function(){mlTok=null;syncMouseSvg(lastSnap||{});},280);}

function flashMr(){clearTimeout(mrTok);var mr=document.getElementById('mr');mr.setAttribute('fill','#00d4ff');
mrTok=setTimeout(function(){mrTok=null;syncMouseSvg(lastSnap||{});},280);}

function sd(id,on){document.getElementById(id).className='dot'+(on?' on':'')}

async function pollLoop(){
var gen=++pollGen;
try{
var r=await(await fetch('/logi?od='+li,{cache:'no-store'})).json();
if(gen!==pollGen)return;
lastSnap=r;
var plc0=plc,prc0=prc;
r.l.forEach(function(t){var d=document.createElement('div');d.textContent=t;lg.appendChild(d)});
if(r.l.length)lg.scrollTop=lg.scrollHeight;
li=r.i;
hx.push(r.dx);hy.push(r.dy);hf.push(r.tf);he.push(r.te?1:0);
if(hx.length>MH)hx.shift();if(hy.length>MH)hy.shift();
if(hf.length>MH)hf.shift();if(he.length>MH)he.shift();
document.getElementById('vx').textContent=r.dx;
document.getElementById('vy').textContent=r.dy;
document.getElementById('cl').textContent=r.lc;
document.getElementById('cr').textContent=r.rc;
document.getElementById('tr').textContent=r.tr;
document.getElementById('tf').textContent=r.tf;
document.getElementById('tb').textContent=r.tb;
document.getElementById('im').textContent=r.im;
var msEl=document.getElementById('mss');
msEl.textContent=(r.im>0?r.ms:'--')+'ms';
msEl.style.color=(r.im>0 && r.ms>r.ow*0.7)?'var(--rd)':(r.im>0?'var(--gn)':'#888');
document.getElementById('oww').textContent=r.ow;
var dbEl=document.getElementById('db'),dbpEl=document.getElementById('dbp');
var bvvEl=document.getElementById('bvv'),bppEl=document.getElementById('bpp'),brrEl=document.getElementById('brr');
brrEl.textContent=r.br;
if(!r.bo){
 dbEl.className='dot bat off';
 dbpEl.textContent='--';
 bvvEl.textContent='--V'; bvvEl.style.color='#888';
 bppEl.textContent='BRAK'; bppEl.style.color='#888';
}else{
 var cls='dot bat on';
 if(r.bp<=20)cls='dot bat cr';
 else if(r.bp<=40)cls='dot bat lo';
 dbEl.className=cls;
 dbpEl.textContent=r.bp;
 bvvEl.textContent=r.bv+'V'+(r.u?' USB':'');
 bppEl.textContent=r.bp+'%';
 var col=(r.bp<=20)?'var(--rd)':(r.bp<=40?'var(--yl)':'var(--gn)');
 bvvEl.style.color=col; bppEl.style.color=col;
}
var teEl=document.getElementById('te');
teEl.textContent=r.te?'ZAMKN':'OTW';
teEl.style.color=r.te?'var(--rd)':'var(--gn)';
sd('du',r.u);sd('ds',r.s);sd('dd',r.d);sd('dp',r.p);sd('dg',r.dg);sd('de',r.te);
plc=r.lc;prc=r.rc;
syncMouseSvg(r);
if(r.lc>plc0)flashMl();
if(r.rc>prc0)flashMr();
document.getElementById('mw').setAttribute('fill',r.s?'#4ecca3':'#3a3a5e');
var b=document.getElementById('bp');b.textContent=r.p?'WZNOW':'PAUZA';b.className='bp'+(r.p?' on':'');
dCh();dCT(r.tb,r.tt,r.tx);dJ(r.dx,r.dy);
}catch(e){}
finally{if(gen===pollGen)setTimeout(pollLoop,POLL_MS);}
}
pollLoop();
</script></body></html>
)rawliteral";

// ====================== HANDLERY HTTP ==============================

static void obsluzStrone() {
    serwer.sendHeader("Cache-Control", "no-store");
    serwer.send(200, "text/html", STRONA_HTML);
}

static void obsluzLogi() {
    int od = serwer.hasArg("od") ? serwer.arg("od").toInt() : 0;

    int najstarszy = indeksZapisu - ROZMIAR_BUFORA;
    if (najstarszy < 0) najstarszy = 0;
    int start = (od > najstarszy) ? od : najstarszy;

    String json;
    json.reserve(1152);
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
    json += ",\"tr\":";   json += String(tcrtRaw);
    json += ",\"tf\":";   json += String((int)tcrtFiltered);
    json += ",\"tb\":";   json += String((int)tcrtBaseline);
    json += ",\"te\":";   json += wirtualnyStanCzujnika ? "true" : "false";
    json += ",\"tt\":";   json += String(OFFSET_TRIGGER);
    json += ",\"tx\":";   json += String(OFFSET_RELEASE);
    json += ",\"im\":";   json += String(licznikImpulsow);
    {
        unsigned long teraz = millis();
        unsigned long delta = (czasOstatniegoImpulsu == 0) ? 0
                              : (teraz - czasOstatniegoImpulsu);
        json += ",\"ms\":"; json += String(delta);
    }
    json += ",\"ow\":";   json += String(OKNO_WIELOKLIKU_MS);
    json += ",\"bo\":";   json += bateriaPodlaczona ? "true" : "false";
    json += ",\"bp\":";   json += String(bateriaProcent);
    json += ",\"bv\":";   json += String(bateriaNapiecie, 2);
    json += ",\"br\":";   json += String(bateriaRawAdc);
    json += ",\"u\":";    json += tud_mounted() ? "true" : "false";
    json += ",\"s\":";    json += trybScrolla ? "true" : "false";
    json += ",\"d\":";    json += przytrzymanieAktywne ? "true" : "false";
    json += ",\"p\":";    json += webPauzaMyszy ? "true" : "false";
    json += ",\"dg\":";   json += czyDebugWlaczony ? "true" : "false";
    json += '}';

    serwer.sendHeader("Cache-Control", "no-store");
    serwer.send(200, "application/json", json);
}

static void obsluzPauze() {
    webPauzaMyszy = !webPauzaMyszy;
    serwer.sendHeader("Cache-Control", "no-store");
    serwer.send(200, "text/plain", webPauzaMyszy ? "PAUZA" : "AKTYWNA");
}

static void obsluzRekalibracje() {
    webZadanieRekalibracji = true;
    serwer.sendHeader("Cache-Control", "no-store");
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
