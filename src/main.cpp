/**
 * HEFAS STM – Nicla Vision: IMU + ToF → HID (USB/BLE), WebDebug, gest OSK
 */

#include <Arduino.h>
#include <math.h>

#include "USBKeyboard.h"
#include "USBMouse.h"

#include "config.h"
#include "click_logic/blink_fsm.h"
#include "hefas_telemetry.h"
#include "hefas_webdebug.h"
#include "hid/hid_transport.h"
#include "hid/mouse_output.h"
#include "imu/imu_driver.h"
#include "led/led_status.h"
#include "tof/tof_blink.h"

static ImuDriver g_imu;
static TofBlink g_tof;
static BlinkFsm g_blink;
static HidTransport g_hid;
static MouseOutput g_mouse;
static LedStatus g_led;
static USBKeyboard g_keyboard(false);

static bool g_systemReady = false;
static bool g_debug = DEBUG_SERIAL_DOMYSLNIE;
static bool g_trybScrolla = false;
static bool g_trwaKalibracja = false;

static unsigned long g_lastDiagMs = 0;
static unsigned long g_lastTofMs = 0;

static bool g_gestPrzechylTrwa = false;
static unsigned long g_gestStartMs = 0;
static unsigned long g_gestCooldownDoMs = 0;

enum class StanGestKlaw : uint8_t { Bezczynny = 0, Wcisniety };
static StanGestKlaw g_stanGestKlaw = StanGestKlaw::Bezczynny;
static unsigned long g_gestKlawCzasStanu = 0;

enum class StanKalibracji : uint8_t { Rownolegle, StabilizacjaTof };
static StanKalibracji g_stanKalibracji = StanKalibracji::Rownolegle;

static void syncTelemetry() {
    trybScrolla = g_trybScrolla;
    trwaKalibracja = g_trwaKalibracja;
    czyDebugWlaczony = g_debug;
    przytrzymanieAktywne = g_blink.dragActive();

    kursorDeltaX = g_imu.deltaX();
    kursorDeltaY = g_imu.deltaY();

    tcrtRaw = g_tof.rawMm();
    tcrtBaseline = g_tof.baselineMm();
    tcrtFiltered = g_tof.filteredMm();
    tcrtFast = g_tof.fastMm();
    wirtualnyStanCzujnika = g_tof.eyeClosedRaw();
    okoPotwierdzoneZamkniete = g_blink.eyeConfirmedClosed();

    licznikImpulsow = g_blink.impulseCount();
    czasOstatniegoImpulsu = g_blink.lastImpulseMs();
    czasPierwszegoImpulsuSerii = g_blink.firstImpulseMs();
    ostatniaPrzerwaMiedzyImpulsamiMs = g_blink.lastGapMs();
    deadlineKoniecSeriiMs = g_blink.seriesDeadlineMs();

    statusOgniwoMontowane = true;
    statusLadowanie = statusOgniwoMontowane && statusUsbKabelAktywny;
}

static void mrugnijLed(int ile, int czasMs) {
    g_led.startBlinkPattern(ile, czasMs);
}

static void rozpocznijKalibracje(const char* zrodlo) {
    if (g_trwaKalibracja) {
        return;
    }
    g_trwaKalibracja = true;
    g_systemReady = false;
    g_stanKalibracji = StanKalibracji::Rownolegle;
    g_imu.startCalibration();
    g_tof.startCalibration();
    g_led.setCalibrating(true, true);
    if (g_debug && zrodlo) {
        Serial.print(F("[KAL] Start ("));
        Serial.print(zrodlo);
        Serial.println(F(") IMU + ToF"));
    }
    webDebugLogKategoria(WEBLOG_FSM, "[KAL] Start rekalibracji");
}

static bool czyZamrozicBaselineTof() {
    return g_trwaKalibracja || g_blink.dragActive() || g_blink.blocksCursorGesture() ||
           g_blink.impulseCount() > 0;
}

static void rozpocznijSkrotKlawiaturyEkranowej() {
    g_hid.registerActivity();
    if (!g_hid.usbHidActive()) {
        if (g_debug) {
            Serial.println(F("[GEST] Ctrl+Win+O: tylko USB-HID"));
            webDebugLogKategoria(WEBLOG_FSM, "[GEST] OSK: tylko USB-HID");
        }
        return;
    }
    if (g_stanGestKlaw != StanGestKlaw::Bezczynny) {
        return;
    }

    g_keyboard.key_code('o', KEY_CTRL | KEY_LOGO);
    g_stanGestKlaw = StanGestKlaw::Wcisniety;
    g_gestKlawCzasStanu = millis();
    g_led.startBlinkPattern(2, 80);
    if (g_debug) {
        Serial.println(F("[GEST] prawo — Ctrl+Win+O"));
        webDebugLogKategoria(WEBLOG_FSM, "[GEST] prawo Ctrl+Win+O");
    }
}

static void odswiezGestKlawiatury() {
    if (g_stanGestKlaw == StanGestKlaw::Bezczynny) {
        return;
    }
    if ((millis() - g_gestKlawCzasStanu) < 60UL) {
        return;
    }
    g_keyboard.key_code_raw(0, 0);
    g_stanGestKlaw = StanGestKlaw::Bezczynny;
}

static void obsluzGestPrzechylenia() {
#if !GEST_PRZECHYL_PRAWO_WLACZONY
    return;
#else
    if (!g_systemReady || g_trwaKalibracja) {
        return;
    }
#if WEBDEBUG_AKTYWNY
    if (webPauzaMyszy) {
        return;
    }
#endif
    if (g_blink.dragActive() || g_blink.eyeConfirmedClosed() || g_tof.eyeClosedRaw()) {
        return;
    }
    if (g_stanGestKlaw != StanGestKlaw::Bezczynny) {
        return;
    }
    if (millis() < g_gestCooldownDoMs) {
        return;
    }

    const float gx = g_imu.gyroXDegPerS();
    const float gy = g_imu.gyroYDegPerS();
    const float gz = g_imu.gyroZDegPerS();

    if (fabsf(gx) > GEST_BLOKADA_GX_GZ_DEG_S || fabsf(gz) > GEST_BLOKADA_GX_GZ_DEG_S) {
        g_gestPrzechylTrwa = false;
        return;
    }

    const float gyAdj = gy * (float)ODWROC_GEST_GY;
    const bool prawo = gyAdj < -GEST_GY_PROG_DEG_S;
    const float progHist = -GEST_GY_PROG_DEG_S * GEST_GY_HISTEREZA_ODSET;

    if (prawo) {
        if (!g_gestPrzechylTrwa) {
            g_gestPrzechylTrwa = true;
            g_gestStartMs = millis();
        } else if (millis() - g_gestStartMs >= GEST_GY_CZAS_TRWANIA_MS) {
            rozpocznijSkrotKlawiaturyEkranowej();
            g_gestCooldownDoMs = millis() + GEST_COOLDOWN_MS;
            g_gestPrzechylTrwa = false;
        }
    } else if (g_gestPrzechylTrwa && gyAdj > progHist) {
        g_gestPrzechylTrwa = false;
    }
#endif
}

static void obsluzAkcjeBlink(const BlinkAction& action) {
    g_hid.registerActivity();
    switch (action.type) {
        case BlinkAction::Type::LeftClick:
            licznikKlikLewych++;
            g_mouse.clickLeft();
            webDebugLogKategoria(WEBLOG_FSM, "[KLIK] LEWY");
            break;
        case BlinkAction::Type::DoubleLeftClick:
            licznikKlikLewych++;
            g_mouse.doubleClickLeft();
            webDebugLogKategoria(WEBLOG_FSM, "[KLIK] DOUBLE LEWY");
            break;
        case BlinkAction::Type::RightClick:
            licznikKlikPrawych++;
            g_mouse.clickRight();
            webDebugLogKategoria(WEBLOG_FSM, "[KLIK] PRAWY");
            break;
        case BlinkAction::Type::ScrollOn:
            g_trybScrolla = true;
            g_led.setScrollMode(true);
            webDebugLogKategoria(WEBLOG_FSM, "[SCROLL] ON");
            break;
        case BlinkAction::Type::ScrollOff:
            g_trybScrolla = false;
            g_led.setScrollMode(false);
            webDebugLogKategoria(WEBLOG_FSM, "[SCROLL] OFF");
            break;
        case BlinkAction::Type::Recalibrate:
            rozpocznijKalibracje("mrugniecie");
            break;
        case BlinkAction::Type::ToggleDebug:
            g_debug = !g_debug;
            Serial.println(g_debug ? F("[DEBUG] ON") : F("[DEBUG] OFF"));
            webDebugLogKategoria(WEBLOG_FSM, g_debug ? "[DEBUG] ON" : "[DEBUG] OFF");
            break;
        case BlinkAction::Type::DragPress:
            g_mouse.press(MOUSE_LEFT);
            g_led.setDragActive(true);
            webDebugLogKategoria(WEBLOG_FSM, "[DRAG] press");
            break;
        case BlinkAction::Type::DragRelease:
            g_mouse.release(MOUSE_LEFT);
            g_led.setDragActive(false);
            webDebugLogKategoria(WEBLOG_FSM, "[DRAG] release");
            break;
        default:
            break;
    }
}

static void diagnostyka() {
    if (!g_debug) {
        return;
    }
    const unsigned long now = millis();
    if (now - g_lastDiagMs < OKRES_DIAGNOSTYKI_MS) {
        return;
    }
    g_lastDiagMs = now;

    Serial.print(F("[IMU] dX="));
    Serial.print(g_imu.deltaX());
    Serial.print(F(" dY="));
    Serial.print(g_imu.deltaY());
    Serial.print(F("  [ToF] raw="));
    Serial.print(g_tof.rawMm());
    Serial.print(F(" base="));
    Serial.print((int)g_tof.baselineMm());
    Serial.print(g_tof.eyeClosedRaw() ? F(" ZAMKN") : F(" OTW"));
    Serial.println();
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    Serial.begin(PREDKOSC_SERIAL);
    delay(CZAS_STABILIZACJI_START_MS);

    Serial.println(F("============================================"));
    Serial.println(F("  HEFAS STM – Nicla Vision"));
    Serial.println(F("============================================"));

    g_led.reset();

    if (!g_hid.begin()) {
        Serial.println(F("[BLAD] HID transport"));
    }
    if (!g_mouse.begin(g_hid)) {
        Serial.println(F("[BLAD] HID mysz"));
    }

    if (!g_imu.begin()) {
        Serial.println(F("[BLAD] LSM6DSOX"));
        while (true) {
            mrugnijLed(5, 100);
            delay(500);
        }
    }
    Serial.println(F("[OK] IMU"));

    if (!g_tof.begin()) {
        Serial.println(F("[BLAD] VL53L1X"));
        while (true) {
            mrugnijLed(5, 100);
            delay(500);
        }
    }
    Serial.println(F("[OK] ToF VL53L1X"));

#if WEBDEBUG_AKTYWNY
    webDebugInit();
#endif

    g_blink.reset();
    rozpocznijKalibracje("start");
}

void loop() {
    const unsigned long now = millis();

    g_hid.poll();
    statusUsbKabelAktywny = g_hid.usbHidActive();

#if WEBDEBUG_AKTYWNY
    webDebugLoop();
    if (webZadanieRekalibracji) {
        webZadanieRekalibracji = false;
        rozpocznijKalibracje("WebDebug");
    }
#endif

    if (g_trwaKalibracja) {
        if (g_stanKalibracji == StanKalibracji::Rownolegle) {
            (void)g_imu.calibrationStep();
            (void)g_tof.calibrationSampleParallel();
            if (!g_imu.isCalibrating() && g_tof.isStabilizePhase()) {
                g_stanKalibracji = StanKalibracji::StabilizacjaTof;
                g_led.setCalibrating(true, false);
            }
        } else if (g_tof.calibrationStabilizeStep()) {
            g_trwaKalibracja = false;
            g_systemReady = true;
            g_led.setCalibrating(false, false);
            g_led.setSystemReady(true);
            mrugnijLed(3, 200);
            Serial.println(F("[KAL] Gotowe (IMU + ToF)"));
            webDebugLogKategoria(WEBLOG_FSM, "[KAL] Gotowe");
            Serial.println(F("  1 mrug = LPM | 2 = double | 3 = PPM | 4 = scroll | 5 = kalibracja | 6 = debug"));
        }
        syncTelemetry();
        g_led.tick(g_hid.canSend());
        delay(CAL_PRZERWA_MS);
        return;
    }

    g_imu.update(g_trybScrolla);
    obsluzGestPrzechylenia();
    odswiezGestKlawiatury();

    const float motion = fabsf(g_imu.gyroXDegPerS()) + fabsf(g_imu.gyroYDegPerS());
    g_hid.refreshBlePolicy(motion);

    if (g_systemReady && (now - g_lastTofMs >= TOF_ODCZYT_OKRES_MS)) {
        g_lastTofMs = now;
        g_tof.setFreezeBaseline(czyZamrozicBaselineTof());
        if (g_tof.update()) {
            bool scroll = g_trybScrolla;
            g_blink.tick(g_tof.eyeClosedRaw(), scroll, g_trybScrolla);

            BlinkAction action;
            while (g_blink.pollAction(action)) {
                if (!g_hid.canSend() && action.type != BlinkAction::Type::ToggleDebug &&
                    action.type != BlinkAction::Type::Recalibrate) {
                    webDebugLogKategoria(WEBLOG_FSM, "[HID] Brak USB/BLE — klik pominiety");
                    continue;
                }
                obsluzAkcjeBlink(action);
            }
        }
    }

    g_mouse.tickClickFsm();

    if (g_systemReady) {
#if WEBDEBUG_AKTYWNY
        if (!webPauzaMyszy)
#endif
        {
            const int dx = g_imu.deltaX();
            const int dy = g_imu.deltaY();
            if (g_trybScrolla) {
                const int scroll = constrain(-dy / DZIELNIK_SCROLLA, -5, 5);
                if (scroll != 0) {
                    g_mouse.move(0, 0, (int8_t)scroll);
                }
            } else if (dx != 0 || dy != 0) {
                g_mouse.move((int8_t)dx, (int8_t)dy);
            }
        }
    }

    syncTelemetry();
    g_led.tick(g_hid.canSend());
    diagnostyka();
    delay(OKRES_PETLI_MS);
}
