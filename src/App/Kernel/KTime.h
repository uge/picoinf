#pragma once

#include <cstdint>
#include <functional>
#include <vector>


/*

Class designed to abstract away clock speed changes on the device
vs what RTOS is able to handle with its (seemingly) fixed
clock speed representation.

Core libraries that actually interface with the kernel time APIs
should use this library.

Calling code should never have to know or care about clock
differences.  Only the deepest libraries should know or care.

*/



class KTime
{
public:
    KTime(uint64_t us);

    operator uint32_t();

    static void SetScalingFactor(double scalingFactor);

    // it is ugly that zephyr doesn't support clock freq change.
    // there is no viable way to handle, say, kmessagepassing semaphor
    // blocking and re-evaluate.
    // instead, for, say, evm, expose a callback which it can handle
    // and do whatever it thinks is the right thing.
    // not perfect, but better than nothing/impossible.
    static void RegisterCallbackScalingFactorChange(std::function<void()> cb);

private:
    uint64_t us_ = 0;

    inline static double scalingFactor_ = 1.0;

    inline static std::vector<std::function<void()>> cbList_;
};

