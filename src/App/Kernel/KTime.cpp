#include "KTime.h"
#include "Timeline.h"

#include "FreeRTOS.Wrapped.h"

#include <cmath>
using namespace std;

#include "StrictMode.h"


KTime::KTime(uint64_t us)
{
    us_ = us;
}

KTime::operator uint32_t()
{
    // the kernel is operating on ticks, and each tick is 1ms (awful rez).
    // Here we convert to ms, allowing truncation, would rather be
    // early than late.
    uint32_t ms = (uint32_t)us_;
    if (ms != portMAX_DELAY)
    {
        ms = (uint32_t)round((double)(us_ / 1'000) * scalingFactor_);
    }

    return ms;
}

void KTime::SetScalingFactor(double scalingFactor)
{
    Timeline::Global().Event("KTIME_SET_SCALING_FACTOR");
    
    scalingFactor_ = scalingFactor;

    for (auto &cb : cbList_)
    {
        cb();
    }
}

void KTime::RegisterCallbackScalingFactorChange(function<void()> cb)
{
    cbList_.push_back(cb);
}