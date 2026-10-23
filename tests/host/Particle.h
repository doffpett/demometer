#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#define SYSTEM_MODE(x)
#define SYSTEM_THREAD(x)
#define PLATFORM_ID 32
#define SPI1 1
#define A6 6
#define FEATURE_ETHERNET_DETECTION 1
#define FEATURE_RESET_INFO 2
#define RESET_NO_WAIT 1
using system_tick_t = uint32_t;
inline uint64_t clockMs = 0;
inline system_tick_t millis() { return static_cast<uint32_t>(clockMs); }
inline int analogRead(int) { return 3380; }
inline int random(int lo, int hi) { return lo + (hi - lo) / 2; }
class String : public std::string {
public:
    using std::string::string;
    int toInt() const { return std::strtol(c_str(), nullptr, 10); }
    void toLowerCase() { for (auto &c : *this) c = std::tolower(c); }
    void trim() {
        const auto first = find_first_not_of(" \r\n\t");
        if (first == npos) { clear(); return; }
        erase(find_last_not_of(" \r\n\t") + 1);
        erase(0, first);
    }
};
struct Reset { uint32_t data; int flags; };
struct SystemMock {
    uint32_t free = 50000;
    uint64_t millis() const { return clockMs; }
    uint32_t freeMemory() const { return free; }
    int resetReason() const { return 40; }
    int resetReasonData() const { return 0; }
    void enableFeature(int) {}
    void disableFeature(int) {}
    [[noreturn]] void reset(uint32_t data = 0, int flags = 0) { throw Reset{data, flags}; }
} inline System;
struct ParticleMock {
    bool online = true;
    std::vector<std::string> variables;
    bool connected() const { return online; }
    template<class T> void variable(const char* name, T&) { variables.emplace_back(name); }
    template<class T> void function(const char*, T) {}
} inline Particle;
struct WiFiMock {
    bool online = true;
    bool ready() const { return online; }
    int RSSI() const { return -57; }
} inline WiFi;
struct EEPROMMock {
    uint8_t data[64] = {};
    int writes = 0;
    template<class T> void get(int offset, T& value) { std::memcpy(&value, data + offset, sizeof(T)); }
    template<class T> void put(int offset, const T& value) {
        std::memcpy(data + offset, &value, sizeof(T)); ++writes;
    }
} inline EEPROM;
struct ApplicationWatchdog {
    static inline int stack = 0, interval = 0, feeds = 0;
    static inline std::function<void()> handler;
    ApplicationWatchdog(int ms, std::function<void()> fn, int bytes = 512) {
        interval = ms; stack = bytes; handler = fn;
    }
    static void checkin() { ++feeds; }
};
struct WatchdogConfiguration {
    int milliseconds = 0;
    WatchdogConfiguration& timeout(std::chrono::milliseconds value) {
        milliseconds = static_cast<int>(value.count()); return *this;
    }
};
struct WatchdogMock {
    int initResult = 0, startResult = 0, interval = 0, feeds = 0, starts = 0;
    int init(const WatchdogConfiguration& config) { interval = config.milliseconds; return initResult; }
    int start() { ++starts; return startResult; }
    int refresh() { ++feeds; return 0; }
} inline Watchdog;
