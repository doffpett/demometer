#include "Particle.h"
#include "neopixel.h"
#include <string.h>
#define PI 3.14159265

#if (PLATFORM_ID == 32)
  #define PIXEL_PIN SPI1
#else
  #define PIXEL_PIN D2
#endif

#define PIXEL_COUNT 52
#define PIXEL_TYPE WS2812B
#define WINNING_DURATION 5000   // 5 sekunder
#define WINNING_STEP_MS 120
#define FALL_STEP_MS 100

Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);

uint32_t fixedPixels[PIXEL_COUNT];
int fixedCount = 1;

// Winning/konfetti
bool winningMode = false;
unsigned long winningStartTime = 0;
unsigned long lastWinningStep = 0;
uint32_t backupFixedPixels[PIXEL_COUNT];
int backupFixedCount = 0;

// Fallende pixel + hale
int fallingPixel = -1;
uint32_t fallingPixelColor = 0;
unsigned long lastFallingStep = 0;

// Kø for antall simulerte demoer som skal vises (fallende pixler)
int pendingSimulate = 0;

// Temperatur/farge-gradient for faste pixler
uint32_t getFixedPixelColor(int index) {
    if(index < 10) {
        return strip.Color(150, 0, 0);
    } else {
        int effectiveIndex = index - 10;
        int maxEffective = PIXEL_COUNT - 1 - 10;
        float t = (float) effectiveIndex / (float) maxEffective;
        uint8_t r = (uint8_t)((1.0 - t) * 255);
        uint8_t g = (uint8_t)(165 + t * (255 - 165));
        uint8_t b = 0;
        return strip.Color(r, g, b);
    }
}

uint32_t getRandomColor() {
    uint8_t r = random(0, 256);
    uint8_t g = random(0, 256);
    uint8_t b = random(0, 256);
    return strip.Color(r, g, b);
}

// Particle function: Simulere en eller flere fallende pixler
int simulateFunction(String countStr) {
    int count = countStr.toInt();
    if (count < 1) count = 1;
    pendingSimulate += count;
    return pendingSimulate;
}

// Particle function: Sette antall faste pixler
int setFixed(String countStr) {
    int newCount = countStr.toInt();
    if(newCount < 1) newCount = 1;
    if(newCount > PIXEL_COUNT) newCount = PIXEL_COUNT;
    fixedCount = newCount;
    for (int i = 1; i < PIXEL_COUNT; i++) {
        if(i < fixedCount) fixedPixels[i] = getFixedPixelColor(i);
        else fixedPixels[i] = 0;
    }
    strip.show();
    return fixedCount;
}

// Particle function: Winning/konfetti
int winningFunction(String dummy) {
    if (!winningMode) {
        memcpy(backupFixedPixels, fixedPixels, sizeof(fixedPixels));
        backupFixedCount = fixedCount;
        winningMode = true;
        winningStartTime = millis();
        lastWinningStep = 0;
    }
    return 1;
}

void setup() {
    strip.begin();
    strip.show();
    for (int i = 0; i < PIXEL_COUNT; i++) fixedPixels[i] = 0;
    for (int i = 1; i < fixedCount && i < PIXEL_COUNT; i++) {
        fixedPixels[i] = getFixedPixelColor(i);
    }
    Particle.function("simulate", simulateFunction);
    Particle.function("set_fixed", setFixed);
    Particle.function("winning", winningFunction);
}

void loop() {
    // 1. Winning/konfetti (helt asynkront)
    if (winningMode) {
        if (millis() - winningStartTime >= WINNING_DURATION) {
            memcpy(fixedPixels, backupFixedPixels, sizeof(fixedPixels));
            fixedCount = backupFixedCount;
            winningMode = false;
        } else if (millis() - lastWinningStep > WINNING_STEP_MS) {
            lastWinningStep = millis();
            for (int i = 0; i < PIXEL_COUNT; i++) {
                if (random(0, 100) < 30) strip.setPixelColor(i, getRandomColor());
                else strip.setPixelColor(i, 0);
            }
            strip.setPixelColor(0, 0); // Pixel 0 alltid av
            strip.show();
        }
    }

    // 2. Start ny falling pixel om det er i kø
    if (fallingPixel == -1 && pendingSimulate > 0 && !winningMode) {
        fallingPixel = PIXEL_COUNT - 1;
        fallingPixelColor = getRandomColor();
        pendingSimulate--;
    }

    // 3. Fallende pixel (asynkront) MED HALE
    if (fallingPixel != -1 && !winningMode) {
        if (millis() - lastFallingStep > FALL_STEP_MS) {
            lastFallingStep = millis();
            if (fallingPixel > fixedCount) {
                // HOVEDPIXEL
                strip.setPixelColor(fallingPixel, fallingPixelColor);

                // HALE – 1. LED under (svakere)
                if (fallingPixel + 1 < PIXEL_COUNT)
                    strip.setPixelColor(fallingPixel + 1, strip.Color(
                        ((fallingPixelColor >> 16) & 0xFF) / 3,
                        ((fallingPixelColor >> 8) & 0xFF) / 3,
                        (fallingPixelColor & 0xFF) / 3
                    ));

                // HALE – 2. LED under (enda svakere)
                if (fallingPixel + 2 < PIXEL_COUNT)
                    strip.setPixelColor(fallingPixel + 2, strip.Color(
                        ((fallingPixelColor >> 16) & 0xFF) / 8,
                        ((fallingPixelColor >> 8) & 0xFF) / 8,
                        (fallingPixelColor & 0xFF) / 8
                    ));

                strip.show();

                // Slukk etterpå
                strip.setPixelColor(fallingPixel, 0);
                if (fallingPixel + 1 < PIXEL_COUNT) strip.setPixelColor(fallingPixel + 1, 0);
                if (fallingPixel + 2 < PIXEL_COUNT) strip.setPixelColor(fallingPixel + 2, 0);

                fallingPixel--;
            } else {
                fixedPixels[fallingPixel] = getFixedPixelColor(fixedCount);
                fixedCount++;
                fallingPixel = -1;
                strip.show();
            }
        }
    }

    // 4. Oppdater faste pixler (og nullstill pixel 0)
    for (int i = 1; i < PIXEL_COUNT; i++) {
        strip.setPixelColor(i, fixedPixels[i]);
    }
    strip.setPixelColor(0, 0);
    strip.show();
}
