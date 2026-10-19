#pragma once

#include <Arduino.h>

/**
 * Detekcja stanu oka (otwarte / zamknięte) z kamery GC2145.
 * Docelowo: model int8 z Edge Impulse / TFLite Micro.
 * Obecnie: opcjonalny fallback na średnią jasność ROI (VISION_TRYB_PROG_JASNOSCI).
 */
class EyeDetector {
public:
    bool begin();
    bool isReady() const { return _ready; }

    /** Jedna klatka + inferencja; zwraca false przy błędzie kamery. */
    bool captureAndClassify();

    /** Ostatni wynik: true = oko uznane za zamknięte (po histerezie modelu). */
    bool eyeClosedRaw() const { return _eyeClosedRaw; }

    /** Pewność klasy „zamknięte” (0..1). */
    float closedConfidence() const { return _closedConfidence; }

    /** Średnia jasność ROI (0..255) — diagnostyka / kalibracja oświetlenia. */
    float roiMeanBrightness() const { return _roiMean; }

private:
    bool _ready = false;
    bool _eyeClosedRaw = false;
    float _closedConfidence = 0.0f;
    float _roiMean = 0.0f;

    bool initCamera();
    bool extractRoiGrayscale(const uint8_t* frame, int frameW, int frameH, int bpp);
    float runClassifier(const uint8_t* grayRoi, int w, int h);
};
