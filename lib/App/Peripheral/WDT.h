#pragma once

#include <stdint.h>


extern void WatchdogSetupShell();


class Watchdog
{
    static const uint32_t DEFAULT_TIMEOUT_MS = 5'000;

public:

    static void SetTimeout(uint32_t timeoutMs);
    static void Start();
    static void Stop();
    static void Feed();
    static bool CausedReboot();


private:

    inline static uint32_t timeoutMs_ = DEFAULT_TIMEOUT_MS;
};