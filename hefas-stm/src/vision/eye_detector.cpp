#include "vision/eye_detector.h"

#include "camera.h"
#include "gc2145.h"

#include "config.h"

static GC2145 s_sensor;
static Camera s_cam(s_sensor);
static FrameBuffer s_fb(VISION_RAMKA_SZEROKOSC, VISION_RAMKA_WYSOKOSC, 2);

// Bufor ROI (szarość) — rozmiar z config.h
static uint8_t s_roiGray[VISION_ROI_SZEROKOSC * VISION_ROI_WYSOKOSC];

bool EyeDetector::initCamera() {
#if defined(ARDUINO_NICLA_VISION)
    if (!s_cam.begin(CAMERA_R160x120, CAMERA_GRAYSCALE, 30)) {
        return false;
    }
#else
    if (!s_cam.begin(CAMERA_R160x120, CAMERA_GRAYSCALE, 30)) {
        return false;
    }
#endif
    for (int i = 0; i < VISION_KLATKI_POMIN_START; i++) {
        (void)s_cam.grabFrame(s_fb, 500);
    }
    return true;
}

bool EyeDetector::begin() {
    if (!initCamera()) {
        return false;
    }
    _ready = true;
    return true;
}

bool EyeDetector::extractRoiGrayscale(const uint8_t* frame, int frameW, int frameH, int bpp) {
    if (bpp != 1) {
        // RGB565 — uproszczone Y z luminancji
        const int roiW = VISION_ROI_SZEROKOSC;
        const int roiH = VISION_ROI_WYSOKOSC;
        const int x0 = (frameW - roiW) / 2;
        const int y0 = (frameH - roiH) / 2;
        long sum = 0;
        for (int y = 0; y < roiH; y++) {
            for (int x = 0; x < roiW; x++) {
                const int fx = x0 + x;
                const int fy = y0 + y;
                const uint16_t* px = (const uint16_t*)(frame + (fy * frameW + fx) * 2);
                const uint16_t rgb = *px;
                const uint8_t r = (rgb >> 11) & 0x1F;
                const uint8_t g = (rgb >> 5) & 0x3F;
                const uint8_t b = rgb & 0x1F;
                const uint8_t gray = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
                s_roiGray[y * roiW + x] = gray;
                sum += gray;
            }
        }
        _roiMean = (float)sum / (float)(roiW * roiH);
        return true;
    }

    const int roiW = VISION_ROI_SZEROKOSC;
    const int roiH = VISION_ROI_WYSOKOSC;
    const int x0 = (frameW - roiW) / 2;
    const int y0 = (frameH - roiH) / 2;
    long sum = 0;
    for (int y = 0; y < roiH; y++) {
        for (int x = 0; x < roiW; x++) {
            const uint8_t v = frame[(y0 + y) * frameW + (x0 + x)];
            s_roiGray[y * roiW + x] = v;
            sum += v;
        }
    }
    _roiMean = (float)sum / (float)(roiW * roiH);
    return true;
}

float EyeDetector::runClassifier(const uint8_t* grayRoi, int w, int h) {
    (void)grayRoi;
    (void)w;
    (void)h;

#if VISION_TRYB_PROG_JASNOSCI
    // Prototyp: niska jasność ≈ powieka zasłania oko (wymaga strojenia i oświetlenia IR).
    if (_roiMean < VISION_PROG_JASNOSC_ZAMKNIETE) {
        return 0.85f;
    }
    return 0.15f;
#else
    // TODO: wywołanie modelu Edge Impulse (run_classifier) na grayRoi.
    return 0.0f;
#endif
}

bool EyeDetector::captureAndClassify() {
    if (!_ready) {
        return false;
    }

    if (s_cam.grabFrame(s_fb, 100) != 0) {
        return false;
    }

    const int frameW = VISION_RAMKA_SZEROKOSC;
    const int frameH = VISION_RAMKA_WYSOKOSC;
    const int bpp = 1;  // CAMERA_GRAYSCALE

    if (!extractRoiGrayscale(s_fb.getBuffer(), frameW, frameH, bpp)) {
        return false;
    }

    _closedConfidence = runClassifier(s_roiGray, VISION_ROI_SZEROKOSC, VISION_ROI_WYSOKOSC);

    if (_closedConfidence >= VISION_PROG_OKO_ZAMKNIETE) {
        _eyeClosedRaw = true;
    } else if (_closedConfidence <= VISION_PROG_OKO_OTWARTE) {
        _eyeClosedRaw = false;
    }
    return true;
}
