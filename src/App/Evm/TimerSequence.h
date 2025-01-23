#pragma once

#include "Timer.h"

#include <functional>
#include <vector>


class TimerSequence
{
public:

    // constructor
    TimerSequence(const char *title = "TimerSequence");

    // restore object to post-constructor state
    void Reset();

    
    // add an event which will be triggered asynchronously after the prior event
    TimerSequence &Add(std::function<void()> fn);
    
    // add a delay after the most recently added event
    TimerSequence &DelayUs(uint64_t durationUs);
    TimerSequence &DelayMs(uint64_t durationMs);

    // change the start time of the most-recently-added event
    TimerSequence &StartAtUs(uint64_t timeAtUs);
    TimerSequence &StartAtUs(std::function<uint64_t()> fn);
    TimerSequence &StartAtMs(uint64_t timeAtMs);


    // initiate sequence
    void Start();


private:

    void OnTimeout();

    struct Step;
    void ScheduleNextFromStep(const Step &step);


private:

    Timer timer_;

    enum class ScheduleNextType : uint8_t
    {
        ScheduleIn,
        ScheduleAt,
        ScheduleAtUsByFn,
    };

    struct Step
    {
        std::function<void()> fn;

        ScheduleNextType scheduleNextType;

        bool scheduleNext = true;

        uint64_t scheduleNextInUs = 0;
        uint64_t scheduleNextAtUs;

        std::function<uint64_t()> scheduleNextAtFn;
    };

    Step firstStep_;

    size_t            stepListIdx_ = 0;
    std::vector<Step> stepList_;
};

