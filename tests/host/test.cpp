// Runs the actual candidate firmware against fake time, network, EEPROM and LEDs.
// Hardware watchdog timing, radio behavior and electrical faults require device tests.
#include <cassert>
#include <iostream>
#include "../../demometer.ino"

template<class F> void expectReset(F fn, int data) {
    bool reset = false;
    try { fn(); } catch (Reset r) {
        reset = true; assert(r.data == (uint32_t)data); assert(r.flags == RESET_NO_WAIT);
    }
    assert(reset);
}

int main(int argc, char** argv) {
    assert(argc == 2);
    std::string scenario(argv[1]);
    if (scenario == "watchdog-init-failure") Watchdog.initResult = -5;
    if (scenario == "watchdog-start-failure") Watchdog.startResult = -6;
    setup();
    assert(ApplicationWatchdog::stack >= 1536);
    assert(ApplicationWatchdog::interval == 60000);
    assert(Particle.variables.size() == 19);

    if (scenario == "watchdog") {
        assert(hardwareWatchdogStatus == 1 && Watchdog.interval == 90000);
        loop(); assert(Watchdog.feeds == 1 && ApplicationWatchdog::feeds == 1);
        expectReset([] { ApplicationWatchdog::handler(); }, RESET_DATA_APP_WATCHDOG);
    } else if (scenario == "watchdog-init-failure" || scenario == "watchdog-start-failure") {
        assert(hardwareWatchdogStatus == (scenario == "watchdog-init-failure" ? -5 : -6));
        loop(); assert(Watchdog.feeds == 0 && ApplicationWatchdog::feeds == 1);
        expectReset([] { ApplicationWatchdog::handler(); }, RESET_DATA_APP_WATCHDOG);
    } else if (scenario == "offline-recovery") {
        clockMs = 10000; maintainCloudRecovery();
        Particle.online = false;
        clockMs += 60000; maintainCloudRecovery();
        assert(cloudDropouts == 1);
        maintainCloudRecovery(); assert(cloudDropouts == 1);
        Particle.online = true; maintainCloudRecovery();
        Particle.online = false;
        clockMs += 899999; maintainCloudRecovery();
        assert(cloudDropouts == 2);
        setLitPixels(14, false); // a completed demo not yet persisted
        clockMs += 1;
        expectReset([] { maintainCloudRecovery(); }, RESET_DATA_CLOUD_OFFLINE);
        PersistedState saved{}; EEPROM.get(0, saved);
        assert(stateIsValid(saved) && saved.litPixels == 14 && saved.brightness == 80);
    } else if (scenario == "offline-rollover") {
        clockMs = UINT32_MAX - 10000ULL; maintainCloudRecovery();
        Particle.online = false;
        clockMs += 899999; maintainCloudRecovery();
        clockMs += 1;
        expectReset([] { maintainCloudRecovery(); }, RESET_DATA_CLOUD_OFFLINE);
    } else if (scenario == "telemetry") {
        clockMs = (1ULL << 32) + 5000; System.free = 20000; updateTelemetry();
        assert(uptimeSeconds == (int)(clockMs / 1000));
        assert(freeMemoryBytes == 20000 && minFreeMemoryBytes == 20000);
        assert(wifiOk == 1 && wifiRssi == -57);
        clockMs += 1000; System.free = 30000; WiFi.online = false; updateTelemetry();
        assert(minFreeMemoryBytes == 20000 && freeMemoryBytes == 30000 && wifiRssi == 0);
    } else if (scenario == "animations-and-storage") {
        setFixed("13"); setBrightness("80"); simulateFunction("3");
        winningFunction("");
        for (int i = 0; i < 2500; ++i) { clockMs += 10; loop(); }
        assert(litPixels == 16 && pendingSimulate == 0 && fallingPixel == -1 && !winningMode);
        loadPersistedState(); assert(litPixels == 16 && brightnessValue == 80);
        setFixed("999"); assert(litPixels == 51 && simulateFunction("2147483647") == 0);
        resetState(""); assert(litPixels == 0);
    } else if (scenario == "60-days-idle") {
        setFixed("13"); setBrightness("80");
        int writes = EEPROM.writes;
        const uint64_t end = 60ULL * 24 * 60 * 60 * 1000;
        for (clockMs = 1000; clockMs <= end; clockMs += 1000) loop();
        assert(litPixels == 13 && brightnessValue == 80 && pendingSimulate == 0);
        assert(EEPROM.writes == writes && cloudDropouts == 0);
        assert(uptimeSeconds == 60 * 24 * 60 * 60);
    } else {
        assert(false && "unknown scenario");
    }
    std::cout << "PASS " << scenario << '\n';
}
