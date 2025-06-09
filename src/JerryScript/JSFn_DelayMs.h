#pragma once

#include "JerryScriptUtl.h"
#include "PAL.h"


// Special function which knows all about the fact that we want to
// limit execution time, and will throw an exception if the user tries to
// delay for too long into that execution.

class JSFn_DelayMs
{
public:

    // setting to 0 means there is no limit.
    static void SetTotalDurationLimitMs(uint64_t ms)
    {
        totalDurationLimitMs_ = ms;
    }

    static void Register()
    {
        JerryScript::SetGlobalPropertyToJerryFunction("DelayMs", fn_);
    }

    static void StartTimeNow()
    {
        timeAtStartMs_ = PAL.Millis();
        totalDelayTimeMs_ = 0;
    }

    static uint64_t GetTotalDelayTimeMs()
    {
        return totalDelayTimeMs_;
    }


private:

    static void DelayMs(uint64_t ms)
    {
        uint64_t timeAtTotalDurationLimit = timeAtStartMs_ + totalDurationLimitMs_;

        uint64_t timeNowMs = PAL.Millis();
        uint64_t timeAtEndOfDelay = timeNowMs + ms;

        if (totalDurationLimitMs_ == 0 || timeAtEndOfDelay <= timeAtTotalDurationLimit)
        {
            PAL.Delay(ms);

            totalDelayTimeMs_ += ms;
        }
        else
        {
            auto &state = JerryScript::GetNativeFunctionState();
            state.retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "DelayMs() exceeded time limit");
        }
    }


private:

    inline static uint64_t timeAtStartMs_ = 0;
    inline static uint64_t totalDurationLimitMs_ = 0;
    inline static uint64_t totalDelayTimeMs_ = 0;

    inline static JerryFunction<void(uint64_t)> fn_ = DelayMs;
};