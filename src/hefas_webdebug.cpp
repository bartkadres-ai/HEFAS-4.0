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
extern float    tcrtFast;
extern int      tcrtRaw;
extern bool     wirtualnyStanCzujnika;
extern bool     okoPotwierdzoneZamkniete;
extern bool     trybBezprzewodowy;
extern bool     statusUsbHidAktywny;
extern bool     statusUsbKabelAktywny;
extern bool     statusOgniwoMontowane;
extern bool     statusLadowanie;
extern bool     bleRadioWlaczony;
extern bool     bleUspionyBezczynnoscia;

// Runtime flaga debug (toggle 6 mrugnięć) — wyświetlana jako dot
extern bool     czyDebugWlaczony;
extern bool     trwaKalibracja;

// Maszyna stanów wieloklików — podgląd na żywo dla diagnostyki
extern uint8_t       licznikImpulsow;
extern unsigned long czasOstatniegoImpulsu;
extern unsigned long czasPierwszegoImpulsuSerii;
extern unsigned long ostatniaPrzerwaMiedzyImpulsamiMs;
extern unsigned long deadlineKoniecSeriiMs;

bool webPauzaMyszy = false;
bool webZadanieRekalibracji = false;
uint8_t webLogMask = WEBLOG_DOMYSLNA_MASKA;

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
.flt{display:flex;flex-wrap:wrap;gap:10px 14px;margin-bottom:10px;padding:10px 12px;background:var(--pn);border:1px solid var(--bd);border-radius:8px;font-size:.78em}
.flt label{display:flex;align-items:center;gap:5px;cursor:pointer;white-space:nowrap}
.flt input{accent-color:var(--ac)}
.flt .dim{color:#666;text-decoration:line-through}
</style></head><body>
<div class="top">
<h1>HEFAS Debug Monitor</h1>
<div class="dots">
<span><span class="dot" id="du"></span>HID</span>
<span><span class="dot" id="duk"></span>USB</span>
<span><span class="dot bat" id="dogn"></span>Ogn.</span>
<span><span class="dot" id="dlad"></span>Ład.</span>
<span><span class="dot" id="ds"></span>Scroll</span>
<span><span class="dot" id="dd"></span>Drag</span>
<span><span class="dot" id="dp"></span>Pauza</span>
<span><span class="dot" id="dg"></span>Debug</span>
<span><span class="dot eye" id="de"></span>Oko</span>
<span><span class="dot" id="db"></span>BLE</span>
<span><span class="dot" id="dkal"></span>Kal.</span>
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
<span style="color:#888">SERIA</span> <b id="im" class="vl" style="color:var(--yl)">0</b>
<span style="color:#888"> Kal.</span> <b id="kalSt" style="color:#888">—</b><br>
<b id="mss" style="color:#888">--ms</b> cisza:<b id="serDl" style="color:#888">--</b> T:<b id="serT">0</b> przerwa:<b id="serG">0</b><br>
<span style="color:#888">TCRT (wykres=Fast)</span><br>
R:<b id="tr">0</b> Fs:<b id="tf">0</b><br>
B:<b id="tb">0</b> <b id="te" style="color:var(--gn)">OTW</b><br>
<span style="color:#888">Zasil.</span> <b id="bzas" style="color:var(--yl)">—</b><br>
<span style="color:#888;font-size:.85em" id="bzasDet"></span><br>
<span style="color:#888">BLE</span> <b id="bles">OFF</b>
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
<div class="leg"><span style="color:#00d4ff">&#9632; Fast</span> &nbsp;<span style="color:#888">&#9632; Base</span> &nbsp;<span style="color:#e94560">&#9632; Trig</span> &nbsp;<span style="color:#f39c12">&#9632; Rel</span></div>
</div>
</div></div>
<div class="flt" id="flt">
<label><input type="checkbox" id="fG" value="1" onchange="ustawFiltry()"> Zyroskop</label>
<label><input type="checkbox" id="fI" value="2" onchange="ustawFiltry()"> Czujnik IR</label>
<label><input type="checkbox" id="fH" value="4" onchange="ustawFiltry()"> BLE/USB</label>
<label><input type="checkbox" id="fF" value="8" checked onchange="ustawFiltry()"> Mrugniecia</label>
<label><input type="checkbox" id="fB" value="16" checked onchange="ustawFiltry()"> Zasilanie</label>
</div>
<div id="lg"></div>
<details class="lgd">
<summary>Legenda - jak czytac panel</summary>
<div class="lgr">
<b>Kropki statusu (gora ekranu)</b><br>
<i>HID</i> = host widzi mysz USB-HID (tud_mounted)<br>
<i>USB</i> = kabel USB / 5 V (tud_connected lub pin ADC) — może być <b>razem z Ogn.</b> przy ładowaniu<br>
<i>Ogn.</i> = ogniwo na BAT+/BAT− (montaż w projekcie; nie pomiar %)<br>
<i>Ład.</i> = ogniwo + zasilanie z USB (ładowarka na płytce; czerwona LED na PCB)<br>
<i>Scroll</i> = wlaczony tryb scrolla (ruch glowy = przewijanie zamiast kursora)<br>
<i>Drag</i> = trwa przytrzymanie LPM (drag and drop)<br>
<i>Pauza</i> = mysz spauzowana przyciskiem ponizej<br>
<i>Debug</i> = <b>czyDebugWlaczony</b> (domyslnie OFF). Wlacz/wylacz <b>6 mrugnieciami</b>. Gdy ON: szczegolowe logi Serial + w panelu [SERIA], [KLIK], [SCROLL], [GEST]. Gdy OFF: mniej wpisow, mniejsze obciazenie CPU.<br>
<i>Oko</i> = oko <b>potwierdzone</b> (liczy sie do klikniecia; V:ZAMKN/OTW = surowy prog)<br>
<i>BLE</i> (kropka) = brak USB-HID — mysz po Bluetooth. <i>BLE</i> w panelu: ON / SLEEP / OFF.<br>
<i>Kal.</i> (kropka + pole <b>Kal. TRWA</b> obok SERIA) = trwa rekalibracja MPU+TCRT (~3 s). Logi <b>[KAL]</b> w filtrze Mrugniecia sa <b>zawsze</b> (start, stabilizacja, gotowe) — takze gdy Debug OFF.<br>
Ładowarka <b>tylko z prądem</b> (bez linii danych) może nie zapalić kropki USB — wtedy opcjonalnie PIN_VBUS_ADC w config.<br>
<b>Licznik serii mrugniec (SERIA)</b><br>
Zolta liczba = ile mrugniec w serii. <b>cisza</b> = pozostaly czas do akcji (zalezy od N: 1=350ms … 6=1000ms).<br>
<b>T</b> = czas calej serii od 1. impulsu. <b>przerwa</b> = ostatnia przerwa miedzy otwarciami.<br>
Laczenie impulsow: przerwa &lt; <b>OKNO</b> (600 ms). Cisza po N: 350/450/640/770/910/1000 ms.<br>
3×: T w zakresie 120–1300 ms (inaczej odrzucone). 4× max T 1900 ms, 5× 2500 ms, 6× 3100 ms. Krotkie serie liczone bez min. przerwy miedzy otwarciami.<br>
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
Linia <i style="color:#00d4ff">cyjan</i> = <b>tcrtFast</b> (ten sam sygnal co progi zamkniecia)<br>
Czerwone tlo wykresu = oko <b>potwierdzone</b> (nie tylko chwilowy prog)<br>
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
Mrugniecie = spadek o min. <b>OFFSET_TRIGGER</b> (320 ADC) ponizej Baseline na sygnale <b>Fast</b>.<br>
Powrot = wzrost ponad Baseline - <b>OFFSET_RELEASE</b> (140 ADC).<br>
Potwierdzenie: <b>2</b> kolejne probki 10 ms (PROBKI_POTWIERDZENIA_STANU).<br>
Roznica 350 vs 120 = histereza, broni przed migotaniem przy szumie.<br>
Zdarzenie mechaniczne = spadek glebszy niz Baseline - <b>OFFSET_MAX_ZWARCIA = 800</b> ADC (zdjecie okularow, zaslon czujnika) — system ignoruje i resetuje do "otwarte".<br>
<b>Mapowanie mrugniec (czasy z hefas_config.h)</b><br>
1 mrug -&gt; LPM &nbsp; 2 mrug -&gt; double (w scrollu: tylko scroll OFF) &nbsp; 3 mrug -&gt; PPM<br>
4 mrug -&gt; scroll ON &nbsp; 5 mrug -&gt; rekalibracja &nbsp; 6 mrug -&gt; przelacznik Debug (Serial + logi szczegolowe)<br>
<span class="lk">Scroll:</span> wylacza kazde mrugniecie &ge;2 (2× tylko OFF, 3× OFF + PPM). Wolniejszy: DZIELNIK_SCROLLA=7, prog Z PROG_ZYRO_SKROL_DEG_S=3.0.<br>
<span class="lk">Aby mrugniecie sie liczolo:</span> zamkniecie oka min. <b>CZAS_MIN_MRUG_MS</b> (krotsze = szum, ignorowane).<br>
<span class="lk">Aby uniknac drag:</span> otworz oko wyraznie miedzy mrugnieciami — EMA musi zdazyc wykryc otwarcie.<br>
<span class="lk">Drag:</span> oko zamkniete &gt; <b>PROG_PRZYTRZYMANIA_MS</b> (850 ms). Zamkniecie 280–850 ms = <b>bez kliku</b> (nie myl z mrugnieciem). Koniec dragu: otwarcie ~50 ms (5 probek).<br>
<span class="lk">Baseline adaptuje sie</span> wolno (EMA_ALPHA_WOLNY = 0.003), zamrozony w trakcie serii mrugniec.<br>
<b>Gest przechylenia (Gy)</b><br>
Przechyl glowe w <b>PRAWO</b> ~0,28 s, prog 40 deg/s (ta sama os co w kodzie, ODWROC_GY=-1) -&gt; <b>Ctrl+Win+O</b>.<br>
<span class="lk">Tylko USB</span> (mysz+klawiatura); na samym BLE mysz dziala, skrot nie.<br>
<b>Filtry logow</b> — checkboxy nad logiem. Domyslnie: <b>Mrugniecia</b> + <b>Zasilanie</b>. Zyro, IR (szczegoly TCRT/kal. baseline), BLE/USB opcjonalnie.<br>
<b>WebDebug vs Debug (kropka)</b> — panel WiFi dziala zawsze (wykresy, SERIA, kropki). Flaga Debug dotyczy tylko rozbudowanych logow tekstowych i Serialu.<br>
<b>Przyciski</b><br>
<i>PAUZA / WZNOW</i> = blokuje wysylanie ruchu i klikniec (logika dalej dziala)<br>
<i>REKALIBRACJA</i> = ponowna kalibracja MPU6050 + TCRT5000 (~3 s, async)<br>
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
var lastSnap=null,mlTok=null,mrTok=null,pollGen=0,POLL_MS=160,lmSync=0;

function maskaZCheckboxow(){
 var m=0;
 if(document.getElementById('fG').checked)m|=1;
 if(document.getElementById('fI').checked)m|=2;
 if(document.getElementById('fH').checked)m|=4;
 if(document.getElementById('fF').checked)m|=8;
 if(document.getElementById('fB').checked)m|=16;
 return m;
}
function ustawCheckboxy(m){
 document.getElementById('fG').checked=!!(m&1);
 document.getElementById('fI').checked=!!(m&2);
 document.getElementById('fH').checked=!!(m&4);
 document.getElementById('fF').checked=!!(m&8);
 document.getElementById('fB').checked=!!(m&16);
}
function ustawFiltry(){
 fetch('/filtry?m='+maskaZCheckboxow(),{cache:'no-store'}).catch(function(){});
}
function syncFiltryStart(){
 fetch('/filtry',{cache:'no-store'}).then(function(x){return x.json()}).then(function(j){
  if(j.m!==undefined)ustawCheckboxy(j.m);
 }).catch(function(){});
}

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
hx.push(r.dx);hy.push(r.dy);hf.push(r.txf);he.push(r.tpo?1:0);
if(hx.length>MH)hx.shift();if(hy.length>MH)hy.shift();
if(hf.length>MH)hf.shift();if(he.length>MH)he.shift();
document.getElementById('vx').textContent=r.dx;
document.getElementById('vy').textContent=r.dy;
document.getElementById('cl').textContent=r.lc;
document.getElementById('cr').textContent=r.rc;
document.getElementById('tr').textContent=r.tr;
document.getElementById('tf').textContent=r.txf;
document.getElementById('tb').textContent=r.tb;
var zEl=document.getElementById('bzas');
var zDet=document.getElementById('bzasDet');
var zParts=[];
if(r.ogn)zParts.push('Ogniwo');
if(r.u)zParts.push('USB-HID');
else if(r.uc)zParts.push('USB');
if(r.lad)zParts.push('ładowanie');
if(!r.u&&r.ogn)zParts.push('mysz→BLE');
zEl.textContent=zParts.length?zParts.join(' + '):'—';
zEl.style.color=r.lad?'var(--yl)':(r.u?'var(--gn)':'#aaa');
if(zDet)zDet.textContent='HID:'+(r.u?'TAK':'nie')+' · USB:'+(r.uc?'TAK':'nie')+' · Ogn.:'+(r.ogn?'TAK':'nie');
var bleTxt=r.bleSlp?'SLEEP':(r.ble?'ON':'OFF');
document.getElementById('bles').textContent=bleTxt;
document.getElementById('bles').style.color=r.bleSlp?'var(--yl)':(r.ble?'var(--gn)':'#888');
document.getElementById('im').textContent=r.im;
var msEl=document.getElementById('mss');
msEl.textContent=(r.im>0?r.ms:'--')+'ms';
var limSerii=r.serDl>0?r.serDl:600;
msEl.style.color=(r.im>0 && r.ms>limSerii*0.7)?'var(--rd)':(r.im>0?'var(--gn)':'#888');
document.getElementById('serDl').textContent=r.im>0?(r.serDl+'ms'):'--';
document.getElementById('serT').textContent=r.serT||0;
document.getElementById('serG').textContent=r.serG||0;
if(r.lm!==undefined && !lmSync){ustawCheckboxy(r.lm);lmSync=1;}
var teEl=document.getElementById('te');
teEl.textContent=(r.te?'V:Z ':'V:O ')+(r.tpo?'POTW':'');
teEl.style.color=r.tpo?'var(--rd)':(r.te?'#c77':'var(--gn)');
var kalEl=document.getElementById('kalSt');
if(kalEl){kalEl.textContent=r.kal?'TRWA ~3s':'—';kalEl.style.color=r.kal?'var(--yl)':'#888';}
sd('du',r.u);sd('duk',r.uc);sd('dogn',r.ogn);sd('dlad',r.lad);
sd('ds',r.s);sd('dd',r.d);sd('dp',r.p);sd('dg',r.dg);sd('de',r.tpo);sd('db',!r.u);sd('dkal',r.kal);
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
syncFiltryStart();
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
    json.reserve(1280);
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
    json += ",\"txf\":";  json += String((int)tcrtFast);
    json += ",\"tb\":";   json += String((int)tcrtBaseline);
    json += ",\"te\":";   json += wirtualnyStanCzujnika ? "true" : "false";
    json += ",\"tpo\":";  json += okoPotwierdzoneZamkniete ? "true" : "false";
    json += ",\"bat\":";  json += trybBezprzewodowy ? "true" : "false";
    json += ",\"uc\":";  json += statusUsbKabelAktywny ? "true" : "false";
    json += ",\"ogn\":"; json += statusOgniwoMontowane ? "true" : "false";
    json += ",\"lad\":"; json += statusLadowanie ? "true" : "false";
    json += ",\"ble\":";  json += bleRadioWlaczony ? "true" : "false";
    json += ",\"bleSlp\":"; json += bleUspionyBezczynnoscia ? "true" : "false";
    json += ",\"tt\":";   json += String(OFFSET_TRIGGER);
    json += ",\"tx\":";   json += String(OFFSET_RELEASE);
    json += ",\"im\":";   json += String(licznikImpulsow);
    {
        unsigned long teraz = millis();
        unsigned long delta = (czasOstatniegoImpulsu == 0) ? 0
                              : (teraz - czasOstatniegoImpulsu);
        json += ",\"ms\":"; json += String(delta);
    }
    json += ",\"owm\":";  json += String(OKNO_MIEDZY_IMPULSAMI_MS);
    {
        unsigned long terazSer = millis();
        unsigned long serT = (czasPierwszegoImpulsuSerii > 0 && licznikImpulsow > 0)
                             ? (czasOstatniegoImpulsu - czasPierwszegoImpulsuSerii) : 0;
        unsigned long serDl = 0;
        if (licznikImpulsow > 0 && deadlineKoniecSeriiMs > terazSer) {
            serDl = deadlineKoniecSeriiMs - terazSer;
        }
        json += ",\"serT\":"; json += String(serT);
        json += ",\"serG\":"; json += String(ostatniaPrzerwaMiedzyImpulsamiMs);
        json += ",\"serDl\":"; json += String(serDl);
    }
    json += ",\"u\":";    json += statusUsbHidAktywny ? "true" : "false";
    json += ",\"s\":";    json += trybScrolla ? "true" : "false";
    json += ",\"d\":";    json += przytrzymanieAktywne ? "true" : "false";
    json += ",\"p\":";    json += webPauzaMyszy ? "true" : "false";
    json += ",\"dg\":";   json += czyDebugWlaczony ? "true" : "false";
    json += ",\"kal\":";  json += trwaKalibracja ? "true" : "false";
    json += ",\"lm\":";  json += String(webLogMask);
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
    webDebugLogKategoria(WEBLOG_FSM, "[KAL] Zadanie rekalibracji (przycisk WebDebug)");
    serwer.sendHeader("Cache-Control", "no-store");
    serwer.send(200, "text/plain", "OK");
}

static void obsluzFiltry() {
    if (serwer.hasArg("m")) {
        int m = serwer.arg("m").toInt();
        if (m < 0) m = 0;
        if (m > 31) m = 31;
        webLogMask = (uint8_t)m;
        serwer.sendHeader("Cache-Control", "no-store");
        serwer.send(200, "text/plain", "OK");
        return;
    }
    String json = "{\"m\":";
    json += String(webLogMask);
    json += '}';
    serwer.sendHeader("Cache-Control", "no-store");
    serwer.send(200, "application/json", json);
}

// ====================== API PUBLICZNE ==============================

void webDebugInit() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WEBDEBUG_SSID, WEBDEBUG_HASLO);

    serwer.on("/",              obsluzStrone);
    serwer.on("/logi",          obsluzLogi);
    serwer.on("/pauza",         obsluzPauze);
    serwer.on("/rekalibracja",  obsluzRekalibracje);
    serwer.on("/filtry",        obsluzFiltry);
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

void webDebugLogKategoria(WebLogKategoria kat, const char* wiadomosc) {
    if ((webLogMask & (uint8_t)kat) == 0) return;
    buforLogow[indeksZapisu % ROZMIAR_BUFORA] = String(wiadomosc);
    indeksZapisu++;
}

void webDebugLogKategoria(WebLogKategoria kat, const String& wiadomosc) {
    if ((webLogMask & (uint8_t)kat) == 0) return;
    buforLogow[indeksZapisu % ROZMIAR_BUFORA] = wiadomosc;
    indeksZapisu++;
}

#endif // WEBDEBUG_AKTYWNY
