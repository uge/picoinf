#pragma once

#include <zephyr/kernel.h>

#include <functional>
#include <vector>
using namespace std;


/*

Class designed to abstract away clock speed changes on the device
vs what Zephyr is able to handle with its (seemingly) fixed
clock speed representation.

Core libraries that actually interface with the kernel time APIs
should use this library.

Calling code should never have to know or care about clock
differences.  Only the deepest libraries should know or care.

*/



class KTime
{
public:
    KTime(uint64_t msOrUs)
    {
        msOrUs_ = (uint64_t)(msOrUs * scalingFactor_);
    }

    KTime(k_timeout_t timeout)
    {
        if (timeout.ticks == -1)
        {
            timeout_ = timeout;
        }
        else
        {
            timeout_ = { .ticks = (uint32_t)(timeout.ticks * scalingFactor_) };
        }
    }

    operator uint32_t() { return (uint32_t)msOrUs_; }
    operator k_timeout_t() { return timeout_; }

    static void SetScalingFactor(double scalingFactor);

    // it is ugly that zephyr doesn't support clock freq change.
    // there is no viable way to handle, say, kmessagepassing semaphor
    // blocking and re-evaluate.
    // instead, for, say, evm, expose a callback which it can handle
    // and do whatever it thinks is the right thing.
    // not perfect, but better than nothing/impossible.
    static void RegisterCallbackScalingFactorChange(function<void()> cb)
    {
        cbList_.push_back(cb);
    }

private:
    uint64_t msOrUs_ = 0;
    k_timeout_t timeout_ = { 0 };

    inline static double scalingFactor_ = 1.0;

    inline static vector<function<void()>> cbList_;
};

