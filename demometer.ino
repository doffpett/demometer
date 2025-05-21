#include "Particle.h" #include "neopixel.h" #include <math.h> // Inkluder math-biblioteket #include // Include queue for std::queue #include <string.h> // For memcpy #define PI 3.14159265 // Definer PI

SYSTEM_MODE(AUTOMATIC); SYSTEM_THREAD(ENABLED);

#if (PLATFORM_ID == 32) #define PIXEL_PIN SPI1 #else #define PIXEL_PIN D2 #endif

#define PIXEL_COUNT 52 #define PIXEL_TYPE WS2812B

Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);

// Array for å lagre fargen på de "faste" pikslene. // Merk: Vi reserverer pixel 0 til pulsende rød, så faste piksler starter fra indeks 1. uint32_t fixedPixels[PIXEL_COUNT];

// Global variabel for antall faste piksler. Siden pixel 0 er reservert, starter vi med 1. int fixedCount = 1;

// Variabler for den fallende pikselen. int fallingPixel = -1; // -1 betyr at ingen piksel er aktiv. uint32_t fallingPixelColor = 0; // Farge for den fallende pikselen. bool temporaryFalling = false; // Angir om den fallende pikselen er fra timer (midlertidig).

// Queue for pending pixels struct PendingPixel { uint32_t color; bool temporary; }; std::queue pendingPixels; // Korrekt deklarasjon av køen

// Variabler for en "pending" pixel fra et eksternt event. bool hasPendingPixel = false; uint32_t pendingPixelColor = 0;

// Variabel for timeren – hvert 20. sekund skal en midlertidig fallende pixel trigges. unsigned long lastTimerFall = 0;

// Tidsforsinkelse for animasjonen (i millisekunder) #define STEP_DELAY 100

// --- Globale variabler for winning mode --- bool winningMode = false; unsigned long winningStartTime = 0; // Backup av tilstanden før winning ble trigget uint32_t backupFixedPixels[PIXEL_COUNT]; int backupFixedCount = 0; int backupFallingPixel = -1; uint32_t backupFallingPixelColor = 0; bool backupTemporaryFalling = false;

// --- Global variabel for demo mode --- // Default er at demo mode er aktiv, dvs. timer-trigget fallende pixel kjøres. bool demoMode = true;

// Funksjon for å generere en tilfeldig farge (brukes for eksterne eventer). uint32_t getRandomColor() { uint8_t r = random(0, 256); uint8_t g = random(0, 256); uint8_t b = random(0, 256); return strip.Color(r, g, b); }

// Funksjon for å skalere en 32-bit fargeverdi med en gitt lysstyrke (0-255). uint32_t applyBrightness(uint32_t color, uint8_t brightness) { uint8_t r = (color >> 16) & 0xFF; uint8_t g = (color >> 8) & 0xFF; uint8_t b = color & 0xFF; r = (r * brightness) / 255; g = (g * brightness) / 255; b = (b * brightness) / 255; return strip.Color(r, g, b); }

// Funksjon for å beregne gradientfargen for en fast piksel basert på indeksen. // (Merk: Indeksen her tilsvarer den faste pikselens plassering, med pixel 0 reservert.) uint32_t getFixedPixelColor(int index) { if(index < 10) { return strip.Color(150, 0, 0); } else { int effectiveIndex = index - 10; int maxEffective = PIXEL_COUNT - 1 - 10; // minus 1 fordi vi reserverer pixel 0. float t = (float) effectiveIndex / (float) maxEffective; uint8_t r = (uint8_t)((1.0 - t) * 255); uint8_t g = (uint8_t)(165 + t * (255 - 165)); uint8_t b = 0; return strip.Color(r, g, b); } }

// Funksjonen for å simulere et eksternt event. // Her ignoreres den innsendte fargeparameteren, og vi setter pendingPixelColor til en tilfeldig farge. void simulateEvent(const char* color) { pendingPixelColor = getRandomColor(); hasPendingPixel = true; }

// Updated simulateEvent to handle multiple pixels void simulateEvent(int count) { for (int i = 0; i < count; i++) { PendingPixel newPixel = { getRandomColor(), false }; pendingPixels.push(newPixel); } }

// Global flag to track simulation state bool simulationInProgress = false;

// Updated simulateFunction to track simulation state int simulateFunction(String countStr) { int count = countStr.toInt(); if (count < 1) { count = 1; // Minimum 1 pixel } simulateEvent(count); simulationInProgress = true; // Mark simulation as in progress return count; // Returner antall piksler som simuleres }

// Skyfunksjon for å manuelt sette antall faste piksler. // Minimumsverdien settes til 1 (fordi pixel 0 er reservert for puls). int setFixed(String countStr) { int newCount = countStr.toInt(); if(newCount < 1) { newCount = 1; // Minimum value is 1 } if(newCount > PIXEL_COUNT) { newCount = PIXEL_COUNT; // Maximum value is PIXEL_COUNT } fixedCount = newCount; for (int i = 1; i < PIXEL_COUNT; i++) { // Start fra index 1 if(i < fixedCount) { fixedPixels[i] = getFixedPixelColor(i); // Tildel gradientfarge } else { fixedPixels[i] = 0; // Fjern farge fra ubrukte piksler } } return fixedCount; // Returner oppdatert fast teller }

// Funksjon for å justere den globale lysstyrken på LED-strippen. int setBrightness(String brightnessStr) { int brightness = brightnessStr.toInt(); if(brightness < 0) { brightness = 0; } if(brightness > 255) { brightness = 255; } strip.setBrightness(brightness); return brightness; }

// --- Winning funksjonen --- // Denne funksjonen trigges eksternt (f.eks. via Particle Cloud) og setter i gang winning mode. int winningFunction(String arg) { // Hvis winning mode ikke allerede er aktiv, lag backup av nåværende tilstand if (!winningMode) { memcpy(backupFixedPixels, fixedPixels, sizeof(fixedPixels)); backupFixedCount = fixedCount; backupFallingPixel = fallingPixel; backupFallingPixelColor = fallingPixelColor; backupTemporaryFalling = temporaryFalling; winningMode = true; winningStartTime = millis(); } return 1; // Kan returnere en statuskode }

// --- Demo mode funksjonen --- // Denne funksjonen lar deg skru demo_mode (timer-trigget fallende pixel) av eller på. // Forventet parameter er "on" eller "off". Default er "on". // Etter å ha endret modus, publiseres en melding med status slik at systemet som trigget den får tilbakemelding. int demoModeFunction(String modeStr) { if(modeStr == "on") { demoMode = true; } else if(modeStr == "off") { demoMode = false; } Particle.publish("demo_mode_status", demoMode ? "on" : "off", PRIVATE); return demoMode ? 1 : 0; // Returnerer 1 for "on", 0 for "off" }

void setup() { strip.begin(); strip.show(); // Slår av alle LED-er ved oppstart.

// Initier fixedPixels for indekser 1 til PIXEL_COUNT-1 (pixel 0 er reservert for puls).
fixedPixels[0] = 0; // Ikke brukt for faste piksler.
for (int i = 1; i < PIXEL_COUNT; i++) {
    fixedPixels[i] = 0;
}

// Ved oppstart: dersom fixedCount > 1, fyll de faste pikslene (fra indeks 1 og oppover).
for (int i = 1; i < fixedCount && i < PIXEL_COUNT; i++) {
    fixedPixels[i] = getFixedPixelColor(i);
}

// Registrer skyfunksjoner.
Particle.function("simulate", simulateFunction);
Particle.function("set_fixed", setFixed);
Particle.function("set_brightness", setBrightness);
Particle.function("winning", winningFunction);
Particle.function("demo_mode", demoModeFunction);
}

void loop() { unsigned long currentMillis = millis();

// --- Winning mode: Fyrverkeri/konfetti effekt ---
if (winningMode) {
    // Sjekk om 30 sekunder har passert
    if (currentMillis - winningStartTime >= 30000) {
        // Gjenopprett backup av tilstanden før winning
        memcpy(fixedPixels, backupFixedPixels, sizeof(fixedPixels));
        fixedCount = backupFixedCount;
        fallingPixel = backupFallingPixel;
        fallingPixelColor = backupFallingPixelColor;
        temporaryFalling = backupTemporaryFalling;
        winningMode = false;
    } else {
        // Fyrverkeri-/konfetti-effekt: tilfeldig farge på enkelte piksler
        for (int i = 0; i < PIXEL_COUNT; i++) {
            // Ca. 30% sjanse for at en piksel lyser med en tilfeldig farge
            if (random(0, 100) < 30) {
                strip.setPixelColor(i, getRandomColor());
            } else {
                strip.setPixelColor(i, 0);
            }
        }
        strip.show();
        delay(STEP_DELAY);
        return; // Hopp over resten av loop() mens winning mode er aktiv
    }
}

// --- Pulsende rød effekt for pixel 0 ---
float phase = (currentMillis % 2000) / 2000.0;         // Periode: 2000 ms (2 sekunder)
float pulsate = (sinf(phase * 2 * PI) + 1) / 2;          // Normalisert verdi 0-1
uint8_t redBrightness = 50 + (uint8_t)(pulsate * (255 - 50)); // Skalere mellom 50 og 255
uint32_t pulsatingRed = strip.Color(redBrightness, 0, 0);

// --- Timer: Trigger fallende pixel kun hvis demo_mode er aktiv ---
if (demoMode && (currentMillis - lastTimerFall >= 20000) && (fallingPixel == -1)) {
    fallingPixel = PIXEL_COUNT - 1;  // Start fra toppen (siste pixel).
    uint32_t baseColor = getRandomColor();
    uint8_t randomBrightness = random(50, 256);  // Tilfeldig lysstyrke (mellom 50 og 255)
    fallingPixelColor = applyBrightness(baseColor, randomBrightness);
    temporaryFalling = true;
    lastTimerFall = currentMillis;
}

// --- Håndter eksterne pixel-eventer ---
if (fallingPixel == -1 && !pendingPixels.empty()) {
    PendingPixel nextPixel = pendingPixels.front();
    pendingPixels.pop();
    fallingPixel = PIXEL_COUNT - 1;  // Start fra toppen
    fallingPixelColor = nextPixel.color; // Bruk fargen fra køen
    temporaryFalling = nextPixel.temporary;
}

// Sjekk om simulering er fullført
if (pendingPixels.empty() && fallingPixel == -1 && simulationInProgress) {
    simulationInProgress = false; // Markér simulering som ferdig
    Particle.publish("simulationComplete", "true", PRIVATE); // Varsle Particle API
}

// --- Animer den fallende pikselen (gjelder for indekser 1 til PIXEL_COUNT-1) ---
if (fallingPixel != -1) {
    // La pikselen falle til den når "landingspunktet" definert av fixedCount.
    if (fallingPixel > fixedCount) {
        fallingPixel--;
    } else {
        if (temporaryFalling) {
            // For timer-event: Fjern den fallende pikselen uten å øke fixedCount.
            fallingPixel = -1;
            temporaryFalling = false;
        } else {
            // For eksterne events: "Fiks" pikselen og øk antallet faste piksler.
            fixedPixels[fallingPixel] = getFixedPixelColor(fixedCount);
            fixedCount++;
            fallingPixel = -1;
        }
    }
}

// --- Oppdater LED-strippen ---
// Sett alle piksler fra 1 til PIXEL_COUNT-1 basert på fixedPixels.
for (int i = 1; i < PIXEL_COUNT; i++) {
    strip.setPixelColor(i, fixedPixels[i]);
}
// Dersom en fallende piksel er aktiv (og ikke på indeks 0, som er reservert), vis den.
if (fallingPixel != -1 && fallingPixel != 0) {
    strip.setPixelColor(fallingPixel, fallingPixelColor);
}
// Sett pixel 0 til den pulsende røde effekten.
strip.setPixelColor(0, pulsatingRed);

strip.show();
delay(STEP_DELAY);
}
