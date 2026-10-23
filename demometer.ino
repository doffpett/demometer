#include "Particle.h"
#include "neopixel.h"
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

SYSTEM_MODE(AUTOMATIC);
SYSTEM_THREAD(ENABLED);

#define FIRMWARE_VERSION "2026-10-23-r6"

#if (PLATFORM_ID == 32)
  #define PIXEL_PIN SPI1
#else
  #define PIXEL_PIN D2
#endif

#define PIXEL_COUNT 52
#define FIRST_LIT_PIXEL 1
#define MAX_LIT_PIXELS (PIXEL_COUNT - FIRST_LIT_PIXEL)
#define PIXEL_TYPE WS2812B

#define DEFAULT_BRIGHTNESS 80
#define MAX_BRIGHTNESS 120

#define WINNING_DURATION_MS 5000
#define WINNING_STEP_MS 120
#define FALL_STEP_MS 100
#define DEMO_MODE_INTERVAL_MS 3000

#define PERSIST_MIN_INTERVAL_MS 5000
#define CLOUD_OFFLINE_RESET_MS (15UL * 60UL * 1000UL)
#define WATCHDOG_TIMEOUT_MS 60000
#define HARDWARE_WATCHDOG_TIMEOUT_MS 90000
#define LIVE_COLOR_TIMEOUT_MS 15000

// Report these through reset_data after a software recovery reset.
#define RESET_DATA_APP_WATCHDOG 1
#define RESET_DATA_CLOUD_OFFLINE 2

#define EEPROM_ADDR 0
#define STATE_MAGIC 0x44454D4FUL
#define STATE_VERSION 1

struct PersistedState {
    uint32_t magic;
    uint16_t version;
    uint16_t litPixels;
    uint8_t brightness;
    uint8_t reserved;
    uint16_t checksum;
};

Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);

String firmwareVersion = FIRMWARE_VERSION;
String displayState = "meter";
bool liveColorMode = false;
uint32_t liveColor = 0;
system_tick_t lastLiveColorMs = 0;
uint32_t fixedPixels[PIXEL_COUNT];

int litPixels = 0;
int brightnessValue = DEFAULT_BRIGHTNESS;
int pendingSimulate = 0;
int demoModeValue = 0;
int cloudOk = 0;
int uptimeSeconds = 0;
int lastResetReason = 0;
int lastResetData = 0;
int freeMemoryBytes = 0;
int minFreeMemoryBytes = INT_MAX;
int wifiOk = 0;
int wifiRssi = 0; // 0 when disconnected; connected signal values are negative dBm.
int offlineSeconds = 0;
int cloudDropouts = 0;
int hardwareWatchdogStatus = 0; // 1 = started, negative = Device OS error, 0 = unsupported.
bool wasCloudConnected = false;
int batteryMillivolts = -1;
int batteryPercent = -1;
int batteryState = 0;

bool winningMode = false;
system_tick_t winningStartTime = 0;
system_tick_t lastWinningStep = 0;
uint32_t backupFixedPixels[PIXEL_COUNT];
int backupLitPixels = 0;

int fallingPixel = -1;
uint32_t fallingPixelColor = 0;
system_tick_t lastFallingStep = 0;

bool stateDirty = false;
int lastPersistedLitPixels = -1;
int lastPersistedBrightness = -1;
system_tick_t lastPersistMs = 0;
system_tick_t lastCloudConnectedMs = 0;
system_tick_t lastTelemetryMs = 0;
system_tick_t lastDemoModeStep = 0;

void watchdogHandler() {
    // The system/cloud thread may be stuck. Never wait for it to acknowledge a reset.
    System.reset(RESET_DATA_APP_WATCHDOG, RESET_NO_WAIT);
}

void startWatchdogs() {
    // Keep the software watchdog as a fallback, with sufficient callback stack.
    // Construct it during setup, after Device OS has initialized.
    static ApplicationWatchdog appWatchdog(WATCHDOG_TIMEOUT_MS, watchdogHandler, 1536);
#if (PLATFORM_ID == 32)
    // Hardware can recover a stalled scheduler/disabled interrupts as well.
    // No onExpired callback: on Photon 2 that would disable the automatic reset.
    hardwareWatchdogStatus = Watchdog.init(WatchdogConfiguration()
        .timeout(std::chrono::milliseconds(HARDWARE_WATCHDOG_TIMEOUT_MS)));
    if (hardwareWatchdogStatus == 0) {
        hardwareWatchdogStatus = Watchdog.start();
        if (hardwareWatchdogStatus == 0) hardwareWatchdogStatus = 1;
    }
#endif
}

int clampLitPixels(int count) {
    if (count < 0) return 0;
    if (count > MAX_LIT_PIXELS) return MAX_LIT_PIXELS;
    return count;
}

int clampBrightness(int value) {
    if (value < 0) return 0;
    if (value > MAX_BRIGHTNESS) return MAX_BRIGHTNESS;
    return value;
}

int interpolatePercent(int mv, int minMv, int maxMv, int minPct, int maxPct) {
    if (mv <= minMv) return minPct;
    if (mv >= maxMv) return maxPct;
    return minPct + (int)(((long)(mv - minMv) * (maxPct - minPct)) / (maxMv - minMv));
}

int estimateBatteryPercent(int mv) {
    if (mv < 2500) return -1;
    if (mv < 3300) return 0;
    if (mv < 3500) return interpolatePercent(mv, 3300, 3500, 0, 10);
    if (mv < 3650) return interpolatePercent(mv, 3500, 3650, 10, 20);
    if (mv < 3800) return interpolatePercent(mv, 3650, 3800, 20, 40);
    if (mv < 3950) return interpolatePercent(mv, 3800, 3950, 40, 70);
    if (mv < 4100) return interpolatePercent(mv, 3950, 4100, 70, 90);
    if (mv < 4200) return interpolatePercent(mv, 4100, 4200, 90, 100);
    return 100;
}

int classifyBatteryState(int mv) {
    if (mv < 2500) return 0;
    if (mv < 3400) return 1;
    if (mv < 3650) return 2;
    if (mv < 4100) return 3;
    return 4;
}

void updateBatteryTelemetry() {
#if (PLATFORM_ID == 32)
    int raw = analogRead(A6);
    batteryMillivolts = (int)(((long)raw * 5000L + 2047L) / 4095L);
    batteryPercent = estimateBatteryPercent(batteryMillivolts);
    batteryState = classifyBatteryState(batteryMillivolts);
#else
    batteryMillivolts = -1;
    batteryPercent = -1;
    batteryState = 0;
#endif
}

uint16_t stateChecksum(const PersistedState& state) {
    uint32_t sum = 0;
    sum += state.magic & 0xFFFF;
    sum += state.magic >> 16;
    sum += state.version;
    sum += state.litPixels;
    sum += state.brightness;
    sum += state.reserved;
    return (uint16_t)(sum ^ 0xA5A5);
}

bool stateIsValid(const PersistedState& state) {
    if (state.magic != STATE_MAGIC) return false;
    if (state.version != STATE_VERSION) return false;
    if (state.litPixels > MAX_LIT_PIXELS) return false;
    if (state.brightness > MAX_BRIGHTNESS) return false;
    return state.checksum == stateChecksum(state);
}

void rememberPersistedState() {
    lastPersistedLitPixels = litPixels;
    lastPersistedBrightness = brightnessValue;
    lastPersistMs = millis();
    stateDirty = false;
}

void persistState() {
    if (litPixels == lastPersistedLitPixels && brightnessValue == lastPersistedBrightness) {
        rememberPersistedState();
        return;
    }

    PersistedState state = {};
    state.magic = STATE_MAGIC;
    state.version = STATE_VERSION;
    state.litPixels = (uint16_t)litPixels;
    state.brightness = (uint8_t)brightnessValue;
    state.reserved = 0;
    state.checksum = stateChecksum(state);

    EEPROM.put(EEPROM_ADDR, state);
    rememberPersistedState();
}

void markStateDirty(bool persistNow) {
    stateDirty = true;
    if (persistNow) {
        persistState();
    }
}

void loadPersistedState() {
    PersistedState state = {};
    EEPROM.get(EEPROM_ADDR, state);

    if (stateIsValid(state)) {
        litPixels = clampLitPixels(state.litPixels);
        brightnessValue = clampBrightness(state.brightness);
    } else {
        litPixels = 0;
        brightnessValue = DEFAULT_BRIGHTNESS;
        markStateDirty(true);
    }

    rememberPersistedState();
}

uint32_t getFixedPixelColor(int visibleIndex) {
    if (visibleIndex <= 10) {
        return strip.Color(150, 0, 0);
    }

    int effectiveIndex = visibleIndex - 10;
    int maxEffective = MAX_LIT_PIXELS - 10;
    float t = (float)effectiveIndex / (float)maxEffective;
    uint8_t r = (uint8_t)((1.0 - t) * 255);
    uint8_t g = (uint8_t)(165 + t * (255 - 165));
    return strip.Color(r, g, 0);
}

uint32_t getRandomColor() {
    return strip.Color(random(0, 256), random(0, 256), random(0, 256));
}

void rebuildFixedPixels() {
    for (int i = 0; i < PIXEL_COUNT; i++) fixedPixels[i] = 0;
    for (int visibleIndex = 1; visibleIndex <= litPixels; visibleIndex++) {
        fixedPixels[FIRST_LIT_PIXEL + visibleIndex - 1] = getFixedPixelColor(visibleIndex);
    }
}

void paintFixedPixels() {
    for (int i = 1; i < PIXEL_COUNT; i++) {
        strip.setPixelColor(i, fixedPixels[i]);
    }
    strip.setPixelColor(0, 0);
}

void paintFallingPixel() {
    if (fallingPixel == -1) return;

    strip.setPixelColor(fallingPixel, fallingPixelColor);

    if (fallingPixel + 1 < PIXEL_COUNT) {
        strip.setPixelColor(fallingPixel + 1, strip.Color(
            ((fallingPixelColor >> 16) & 0xFF) / 3,
            ((fallingPixelColor >> 8) & 0xFF) / 3,
            (fallingPixelColor & 0xFF) / 3
        ));
    }

    if (fallingPixel + 2 < PIXEL_COUNT) {
        strip.setPixelColor(fallingPixel + 2, strip.Color(
            ((fallingPixelColor >> 16) & 0xFF) / 8,
            ((fallingPixelColor >> 8) & 0xFF) / 8,
            (fallingPixelColor & 0xFF) / 8
        ));
    }
}

void showStripFrame() {
    // Only the display is overridden. The counter and its animations still advance.
    if (liveColorMode) {
        for (int i = 0; i < PIXEL_COUNT; ++i) strip.setPixelColor(i, liveColor);
    }
    strip.show();
}

void showNormalFrame() {
    paintFixedPixels();
    paintFallingPixel();
    showStripFrame();
}

int setLiveColor(String args) {
    args.trim();
    if (args == "off") {
        liveColorMode = false;
        displayState = "meter";
        showNormalFrame();
        return 0;
    }

    // Reject malformed/overflowing values instead of silently changing the color.
    const char* cursor = args.c_str();
    int rgb[3];
    for (int i = 0; i < 3; ++i) {
        char* end = nullptr;
        long value = strtol(cursor, &end, 10);
        if (end == cursor || value < 0 || value > 255) return -1;
        if (i < 2 ? *end != ',' : *end != '\0') return -1;
        rgb[i] = (int)value;
        cursor = i < 2 ? end + 1 : end;
    }

    uint32_t color = strip.Color(rgb[0], rgb[1], rgb[2]);
    bool changed = !liveColorMode || color != liveColor;
    liveColorMode = true;
    liveColor = color;
    lastLiveColorMs = millis();
    if (changed) {
        char label[8];
        snprintf(label, sizeof(label), "#%02x%02x%02x", rgb[0], rgb[1], rgb[2]);
        displayState = label;
        showNormalFrame();
    }
    return 1;
}

void maintainLiveColor() {
    if (liveColorMode && millis() - lastLiveColorMs >= LIVE_COLOR_TIMEOUT_MS) {
        liveColorMode = false;
        displayState = "meter";
        showNormalFrame();
    }
}

void setLitPixels(int count, bool persistNow) {
    litPixels = clampLitPixels(count);
    rebuildFixedPixels();
    showNormalFrame();
    markStateDirty(persistNow);
}

int queuedOrFallingPixels() {
    return pendingSimulate + (fallingPixel != -1 ? 1 : 0);
}

int availableDemoSlots() {
    int available = MAX_LIT_PIXELS - litPixels - queuedOrFallingPixels();
    return available < 0 ? 0 : available;
}

void stopTransientEffects() {
    winningMode = false;
    fallingPixel = -1;
    pendingSimulate = 0;
}

int simulateFunction(String countStr) {
    int count = countStr.toInt();
    if (count < 1) count = 1;

    int available = availableDemoSlots();
    if (count > available) count = available;
    pendingSimulate += count;

    return pendingSimulate;
}

int setFixed(String countStr) {
    stopTransientEffects();
    setLitPixels(countStr.toInt(), true);
    return litPixels;
}

int resetState(String args) {
    stopTransientEffects();
    setLitPixels(0, true);
    return litPixels;
}

int setBrightness(String brightnessStr) {
    brightnessValue = clampBrightness(brightnessStr.toInt());
    strip.setBrightness(brightnessValue);
    showNormalFrame();
    markStateDirty(true);
    return brightnessValue;
}

int demoModeFunction(String args) {
    args.toLowerCase();
    args.trim();
    demoModeValue = !(args == "0" || args == "off" || args == "false" || args == "stop");
    return demoModeValue;
}

int pingFunction(String args) {
    return uptimeSeconds;
}

int winningFunction(String args) {
    if (!winningMode) {
        memcpy(backupFixedPixels, fixedPixels, sizeof(fixedPixels));
        backupLitPixels = litPixels;
        winningMode = true;
        winningStartTime = millis();
        lastWinningStep = 0;
    }
    return 1;
}

void updateTelemetry() {
    if (millis() - lastTelemetryMs < 1000) return;
    lastTelemetryMs = millis();

    // The original 32-bit millis counter wraps after ~49.7 days.
    const uint64_t seconds = System.millis() / 1000ULL;
    uptimeSeconds = seconds > INT_MAX ? INT_MAX : (int)seconds;
    cloudOk = Particle.connected() ? 1 : 0;
    freeMemoryBytes = (int)System.freeMemory();
    if (freeMemoryBytes < minFreeMemoryBytes) minFreeMemoryBytes = freeMemoryBytes;
    wifiOk = WiFi.ready() ? 1 : 0;
    wifiRssi = wifiOk ? (int)WiFi.RSSI() : 0;
    offlineSeconds = cloudOk ? 0 : (int)((millis() - lastCloudConnectedMs) / 1000UL);
    updateBatteryTelemetry();
}

void maintainCloudRecovery() {
    if (Particle.connected()) {
        lastCloudConnectedMs = millis();
        wasCloudConnected = true;
        return;
    }

    if (wasCloudConnected) {
        if (cloudDropouts < INT_MAX) cloudDropouts++;
        wasCloudConnected = false;
    }

    if (millis() - lastCloudConnectedMs >= CLOUD_OFFLINE_RESET_MS) {
        // Preserve completed demos before recovery. If flash I/O stalls,
        // the hardware watchdog still provides a bounded recovery path.
        if (stateDirty) persistState();
        System.reset(RESET_DATA_CLOUD_OFFLINE, RESET_NO_WAIT);
    }
}

void flushStateIfNeeded() {
    if (!stateDirty) return;
    if (millis() - lastPersistMs >= PERSIST_MIN_INTERVAL_MS) {
        persistState();
    }
}

void setup() {
    startWatchdogs();
    System.enableFeature(FEATURE_RESET_INFO);
#if (PLATFORM_ID == 32)
    System.disableFeature(FEATURE_ETHERNET_DETECTION);
#endif

    lastResetReason = (int)System.resetReason();
    lastResetData = (int)System.resetReasonData();
    lastCloudConnectedMs = millis();

    loadPersistedState();

    strip.begin();
    strip.setBrightness(brightnessValue);
    rebuildFixedPixels();
    showNormalFrame();
    updateBatteryTelemetry();

    Particle.variable("fw", firmwareVersion);
    Particle.variable("display", displayState);
    Particle.variable("lit_pixels", litPixels);
    Particle.variable("brightness", brightnessValue);
    Particle.variable("pending", pendingSimulate);
    Particle.variable("demo_mode", demoModeValue);
    Particle.variable("cloud_ok", cloudOk);
    Particle.variable("uptime_s", uptimeSeconds);
    Particle.variable("reset_rsn", lastResetReason);
    Particle.variable("reset_data", lastResetData);
    Particle.variable("free_mem", freeMemoryBytes);
    Particle.variable("min_free", minFreeMemoryBytes);
    Particle.variable("wifi_ok", wifiOk);
    Particle.variable("wifi_rssi", wifiRssi);
    Particle.variable("offline_s", offlineSeconds);
    Particle.variable("dropouts", cloudDropouts);
    Particle.variable("hw_wd", hardwareWatchdogStatus);
    Particle.variable("batt_mv", batteryMillivolts);
    Particle.variable("batt_pct", batteryPercent);
    Particle.variable("batt_state", batteryState);

    Particle.function("simulate", simulateFunction);
    Particle.function("set_fixed", setFixed);
    Particle.function("reset_state", resetState);
    Particle.function("set_brightness", setBrightness);
    Particle.function("winning", winningFunction);
    Particle.function("demo_mode", demoModeFunction);
    Particle.function("ping", pingFunction);
    Particle.function("set_color", setLiveColor);
}

void loop() {
    ApplicationWatchdog::checkin();
#if (PLATFORM_ID == 32)
    if (hardwareWatchdogStatus == 1) Watchdog.refresh();
#endif
    maintainLiveColor();
    updateTelemetry();
    maintainCloudRecovery();
    flushStateIfNeeded();

    if (winningMode) {
        if (millis() - winningStartTime >= WINNING_DURATION_MS) {
            memcpy(fixedPixels, backupFixedPixels, sizeof(fixedPixels));
            litPixels = backupLitPixels;
            winningMode = false;
            showNormalFrame();
        } else if (millis() - lastWinningStep > WINNING_STEP_MS) {
            lastWinningStep = millis();
            for (int i = 0; i < PIXEL_COUNT; i++) {
                if (random(0, 100) < 30) strip.setPixelColor(i, getRandomColor());
                else strip.setPixelColor(i, 0);
            }
            strip.setPixelColor(0, 0);
            showStripFrame();
        }
        return;
    }

    if (demoModeValue && millis() - lastDemoModeStep > DEMO_MODE_INTERVAL_MS) {
        lastDemoModeStep = millis();
        if (availableDemoSlots() > 0) pendingSimulate++;
    }

    if (fallingPixel == -1 && pendingSimulate > 0 && litPixels < MAX_LIT_PIXELS) {
        fallingPixel = PIXEL_COUNT - 1;
        fallingPixelColor = getRandomColor();
        pendingSimulate--;
        lastFallingStep = 0;
    }

    if (fallingPixel != -1 && millis() - lastFallingStep > FALL_STEP_MS) {
        lastFallingStep = millis();
        int targetPixel = FIRST_LIT_PIXEL + litPixels;

        if (fallingPixel > targetPixel) {
            showNormalFrame();
            fallingPixel--;
        } else {
            litPixels++;
            rebuildFixedPixels();
            fallingPixel = -1;
            showNormalFrame();
            markStateDirty(false);
        }
    }
}
