#include "Log.h"
#include "TimerSequence.h"

using namespace std;

#include "StrictMode.h"


TimerSequence::TimerSequence(const char *title)
{
    timer_.SetCallback([this]{ OnTimeout(); }, title);

    Reset();
}

TimerSequence &TimerSequence::Add(function<void()> fn)
{
    Step step = { fn, true, 0, };

    stepList_.push_back(step);

    return *this;
}

TimerSequence &TimerSequence::StepFromPause()
{
    SetLatestStepParameters(false, 0);

    return *this;
}

TimerSequence &TimerSequence::StepFromInUs(uint64_t durationUs)
{
    SetLatestStepParameters(true, durationUs);

    return *this;
}

TimerSequence &TimerSequence::StepFromInMs(uint64_t durationMs)
{
    SetLatestStepParameters(true, durationMs * 1'000);

    return *this;
}

void TimerSequence::Start()
{
    timer_.TimeoutInMs(0);
}

void TimerSequence::Reset()
{
    timer_.Cancel();
    stepListIdx_ = 0;
    stepList_.clear();
    stepList_.shrink_to_fit();
}


void TimerSequence::SetLatestStepParameters(bool autoStep, uint64_t durationUs)
{
    if (stepList_.size())
    {
        Step &step = stepList_.back();

        step.scheduleNext     = autoStep;
        step.scheduleNextInUs = durationUs;
    }
}

void TimerSequence::OnTimeout()
{
    if (stepListIdx_ < stepList_.size())
    {
        Step &step = stepList_[stepListIdx_];

        step.fn();

        if (step.scheduleNext)
        {
            timer_.TimeoutInUs(step.scheduleNextInUs);
        }

        ++stepListIdx_;
    }
    else
    {
        timer_.Cancel();
    }
}


