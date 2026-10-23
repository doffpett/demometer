#pragma once
#include <cassert>
#include <cstdint>
#include <vector>
#define WS2812B 1
class Adafruit_NeoPixel {
public:
    std::vector<uint32_t> pixels;
    int brightness = 0;
    Adafruit_NeoPixel(int count, int, int) : pixels(count) {}
    void begin() {}
    void show() {}
    void setBrightness(int n) { brightness = n; }
    void setPixelColor(int i, uint32_t color) { assert(i >= 0 && i < (int)pixels.size()); pixels[i] = color; }
    uint32_t Color(int r, int g, int b) { return (r << 16) | (g << 8) | b; }
};
