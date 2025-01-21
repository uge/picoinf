#pragma once

#include "Timer.h"

#include <functional>
#include <vector>


class TimerSequence
: public Timer
{
public:

    TimerSequence(const char *title = "TimerSequence")
    {
        SetCallback([this]{ OnTimeoutPrivate(); }, title);

        Reset();
    }

    TimerSequence &Add(std::function<void()> fn)
    {
        Step step = { fn };

        stepList_.push_back(step);

        return *this;
    }

    void Reset()
    {
        Cancel();
        stepListIdx_ = 0;
        stepList_.clear();
        stepList_.shrink_to_fit();
    }


private:

    void OnTimeoutPrivate()
    {
        if (stepListIdx_ < stepList_.size())
        {
            stepList_[stepListIdx_].fn();
            ++stepListIdx_;
        }
        else
        {
            Cancel();
        }
    }


private:

    Timer timer_;

    struct Step
    {
        std::function<void()> fn = []{};
    };

    size_t stepListIdx_ = 0;
    std::vector<Step> stepList_;
};

