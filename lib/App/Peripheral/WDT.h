#pragma once

#include <stdint.h>

// forward decl
struct device;


class Watchdog
{
    static const uint32_t DEFAULT_TIMEOUT_MS = 5'000;

public:

    static void SetTimeout(uint32_t timeoutMs);
    static void Start();
    static void Stop();
    static void Feed();


private:

    inline static uint32_t timeoutMs_ = DEFAULT_TIMEOUT_MS;
    inline static int channelId_ = 0;
    inline static bool running_ = false;
};