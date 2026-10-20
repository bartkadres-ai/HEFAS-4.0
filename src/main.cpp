/**
 * HEFAS STM – Nicla Vision: IMU + ToF → HID mysz
 */

#include <Arduino.h>
#include <math.h>

#include "USBMouse.h"

#include "config.h"
#include "click_logic/blink_fsm.h"
#include "hid/mouse_output.h"
#include "imu/imu_driver.h"
#include "tof/tof_blink.h"

static ImuDriver g_imu;
static TofBlink g_tof;
static BlinkFsm g_blink;
static MouseOutput g_mouse;

static bool g_systemReady = false;
static bool g_debug = DEBUG_SERIAL_DOMYSLNIE;
static bool g_trybScrolla = false;
static bool g_trwaKalibracja = false;

static unsigned long g_lastDiagMs = 0;
static unsigned long g_lastTofMs = 0;

static bool g_gestPrzechylTrwa = false;
static unsigned long g_gestStartMs = 0;
static unsigned long g_gestCooldownDoMs = 0;

enum class StanKalibracji : uint8_t { Rownolegle, StabilizacjaTof };

static StanKalibracji g_stanKalibracji = StanKalibracji::Rownolegle;

static void mrugnijLed(int ile, int czasMs) {
    for (int i = 0; i < ile; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(czasMs);
        digitalWrite(LED_BUILTIN, LOW);
        delay(czasMs);
    }
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
    if (g_debug && zrodlo) {
        Serial.print(F("[KAL] Start ("));
        Serial.print(zrodlo);
        Serial.println(F(") IMU + ToF"));
    }
    digitalWrite(LED_BUILTIN, HIGH);
}

static bool czyZamrozicBaselineTof() {
    return g_trwaKalibracja || g_blink.dragActive() || g_blink.blocksCursorGesture();
}

static void obsluzGestPrzechylenia() {
#if !GEST_PRZECHYL_PRAWO_WLACZONY
    return;
#else
    if (!g_systemReady || g_trwaKalibracja || g_blink.blocksCursorGesture()) {
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
            if (g_debug) {
                Serial.println(F("[GEST] przechyl prawo — OSK (USBKeyboard TODO)"));
            }
            g_gestCooldownDoMs = millis() + GEST_COOLDOWN_MS;
            g_gestPrzechylTrwa = false;
        }
    } else if (g_gestPrzechylTrwa && gyAdj > progHist) {
        g_gestPrzechylTrwa = false;
    }
#endif
}

static void obsluzAkcjeBlink(const BlinkAction& action) {
    switch (action.type) {
        case BlinkAction::Type::LeftClick:
            g_mouse.clickLeft();
            break;
        case BlinkAction::Type::DoubleLeftClick:
            g_mouse.doubleClickLeft();
            break;
        case BlinkAction::Type::RightClick:
            g_mouse.clickRight();
            break;
        case BlinkAction::Type::ScrollOn:
            g_trybScrolla = true;
            break;
        case BlinkAction::Type::ScrollOff:
            g_trybScrolla = false;
            break;
        case BlinkAction::Type::Recalibrate:
            rozpocznijKalibracje("mrugniecie");
            break;
        case BlinkAction::Type::ToggleDebug:
            g_debug = !g_debug;
            Serial.println(g_debug ? F("[DEBUG] ON") : F("[DEBUG] OFF"));
            break;
        case BlinkAction::Type::DragPress:
            g_mouse.press(MOUSE_LEFT);
            break;
        case BlinkAction::Type::DragRelease:
            g_mouse.release(MOUSE_LEFT);
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
    Serial.println(F("  HEFAS STM – Nicla Vision (ToF)"));
    Serial.println(F("============================================"));

    if (!g_mouse.begin()) {
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

    g_blink.reset();
    rozpocznijKalibracje("start");
}

void loop() {
    const unsigned long now = millis();

    if (g_trwaKalibracja) {
        if (g_stanKalibracji == StanKalibracji::Rownolegle) {
            (void)g_imu.calibrationStep();
            (void)g_tof.calibrationSampleParallel();
            if (!g_imu.isCalibrating() && g_tof.isStabilizePhase()) {
                g_stanKalibracji = StanKalibracji::StabilizacjaTof;
            }
        } else if (g_tof.calibrationStabilizeStep()) {
            g_trwaKalibracja = false;
            g_systemReady = true;
            digitalWrite(LED_BUILTIN, LOW);
            mrugnijLed(3, 200);
            Serial.println(F("[KAL] Gotowe (IMU + ToF)"));
            Serial.println(F("  1 mrug = LPM | 2 = double | 3 = PPM | 4 = scroll | 5 = kalibracja | 6 = debug"));
        }
        delay(CAL_PRZERWA_MS);
        return;
    }

    g_imu.update(g_trybScrolla);
    obsluzGestPrzechylenia();

    if (g_systemReady && (now - g_lastTofMs >= TOF_ODCZYT_OKRES_MS)) {
        g_lastTofMs = now;
        g_tof.setFreezeBaseline(czyZamrozicBaselineTof());
        if (g_tof.update()) {
            bool scroll = g_trybScrolla;
            g_blink.tick(g_tof.eyeClosedRaw(), scroll, g_trybScrolla);

            BlinkAction action;
            while (g_blink.pollAction(action)) {
                obsluzAkcjeBlink(action);
            }
        }
    }

    g_mouse.tickClickFsm();

    if (g_systemReady) {
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

    diagnostyka();
    delay(OKRES_PETLI_MS);
}
