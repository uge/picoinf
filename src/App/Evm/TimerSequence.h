#pragma once

#include "Timer.h"

#include <functional>
#include <vector>


class TimerSequence
{
public:

    TimerSequence(const char *title = "TimerSequence");
    
    TimerSequence &Add(std::function<void()> fn);
    TimerSequence &StepFromPause();
    TimerSequence &StepFromInUs(uint64_t durationUs);
    TimerSequence &StepFromInMs(uint64_t durationMs);

    void Start();
    void Reset();


private:

    void SetLatestStepParameters(bool autoStep, uint64_t durationUs);
    void OnTimeout();


private:

    Timer timer_;

    struct Step
    {
        std::function<void()> fn;
        bool                  scheduleNext;
        uint64_t              scheduleNextInUs;
    };

    size_t            stepListIdx_ = 0;
    std::vector<Step> stepList_;
};

