#include "Log.h"
#include "TimerSequence.h"

using namespace std;

#include "StrictMode.h"


TimerSequence::TimerSequence(const char *name)
{
    timer_.SetName(name);
    timer_.SetCallback([this]{ OnTimeout(); });

    Reset();
}

void TimerSequence::Reset()
{
    timer_.Cancel();
    
    firstStep_ = Step{};
    stepListIdx_ = 0;
    stepList_.clear();
    stepList_.shrink_to_fit();
}

TimerSequence &TimerSequence::Add(function<void()> fn)
{
    Step step = { fn, ScheduleNextType::ScheduleIn };

    stepList_.push_back(step);

    return *this;
}

TimerSequence &TimerSequence::DelayUs(uint64_t durationUs)
{
    // update the last element in the sequence to call next after a delay
    Step &step = stepList_.size() == 0 ? firstStep_ : stepList_.back();

    step.scheduleNextType = ScheduleNextType::ScheduleIn;
    step.scheduleNext     = true;
    step.scheduleNextInUs = durationUs;

    // add an additional element of nothing to represent that delay
    Add([]{});

    return *this;
}

TimerSequence &TimerSequence::DelayMs(uint64_t durationMs)
{
    DelayUs(durationMs * 1'000);

    return *this;
}

TimerSequence &TimerSequence::StartAtUs(uint64_t timeAtUs)
{
    if (stepList_.size() >= 1)
    {
        Step &step = stepList_.size() == 0 ? firstStep_ : stepList_[stepList_.size() - 2];

        step.scheduleNextType = ScheduleNextType::ScheduleAt;
        step.scheduleNext     = true;
        step.scheduleNextAtUs = timeAtUs;
    }

    return *this;
}

TimerSequence &TimerSequence::StartAtUs(function<uint64_t()> fn)
{
    if (stepList_.size() >= 1)
    {
        Step &step = stepList_.size() == 0 ? firstStep_ : stepList_[stepList_.size() - 2];

        step.scheduleNextType = ScheduleNextType::ScheduleAtUsByFn;
        step.scheduleNext     = true;
        step.scheduleNextAtFn = fn;
    }

    return *this;
}

TimerSequence &TimerSequence::StartAtMs(uint64_t timeAtMs)
{
    return StartAtUs(timeAtMs * 1'000);
}


void TimerSequence::Start()
{
    ScheduleNextFromStep(firstStep_);
}


void TimerSequence::OnTimeout()
{
    if (stepListIdx_ < stepList_.size())
    {
        Step &step = stepList_[stepListIdx_];

        step.fn();

        ++stepListIdx_;

        ScheduleNextFromStep(step);
    }
}


void TimerSequence::ScheduleNextFromStep(const Step &step)
{
    if (step.scheduleNext && stepListIdx_ < stepList_.size())
    {
        if (step.scheduleNextType == ScheduleNextType::ScheduleIn)
        {
            timer_.TimeoutInUs(step.scheduleNextInUs);
        }
        else if (step.scheduleNextType == ScheduleNextType::ScheduleAt)
        {
            timer_.TimeoutAtUs(step.scheduleNextAtUs);
        }
        else if (step.scheduleNextType == ScheduleNextType::ScheduleAtUsByFn)
        {
            timer_.TimeoutAtUs(step.scheduleNextAtFn());
        }
    }
}
