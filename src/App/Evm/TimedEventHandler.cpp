#include "Evm.h"
#include "Timeline.h"


bool TimedEventHandler::TimeoutAtMs(uint64_t absTimeMs)
{
    return TimeoutAtUs(Micros{absTimeMs * 1000});
}

bool TimedEventHandler::TimeoutInMs(uint64_t durationMs)
{
    return TimeoutInUs(Micros{durationMs * 1000});
}

bool TimedEventHandler::TimeoutIntervalMs(uint64_t timeout)
{
    return TimeoutIntervalUs(Micros{timeout * 1000});
}

bool TimedEventHandler::TimeoutIntervalMs(uint64_t durationIntervalMs, uint64_t durationFirstInMs)
{
    return TimeoutIntervalUs(Micros{durationIntervalMs * 1000}, Micros{durationFirstInMs * 1000});
}


bool TimedEventHandler::TimeoutAtUs(Micros absTimeUs)
{
    // Don't allow yourself to be scheduled more than once.
    // Cache whether this is an interval callback since that
    // gets reset during cancel.
    bool isIntervalCache = isInterval_;
    Cancel();
    isInterval_ = isIntervalCache;

    // give this registration a unique incrementing number, so tie-breaks
    // between this and another time set for the same expiry can be
    // broken when sorting (earliest setter wins).
    seqNo_ = seqNoNext_;
    ++seqNoNext_;

    return Evm::RegisterTimedEventHandler(this, absTimeUs.value_);
}

bool TimedEventHandler::TimeoutInUs(Micros durationUs)
{
    durationUs_ = durationUs.value_;

    return TimeoutAtUs(Micros{PAL.Micros() + durationUs_});
}

bool TimedEventHandler::TimeoutIntervalUs(Micros durationIntervalUs)
{
    isInterval_ = 1;
    
    durationIntervalUs_ = durationIntervalUs.value_;
    
    return TimeoutInUs(durationIntervalUs);
}

bool TimedEventHandler::TimeoutIntervalUs(Micros durationIntervalUs, Micros durationFirstInUs)
{
    isInterval_ = 1;
    
    durationIntervalUs_ = durationIntervalUs.value_;
    
    return TimeoutInUs(durationFirstInUs);
}


void TimedEventHandler::Cancel()
{
    Evm::DeRegisterTimedEventHandler(this);
    
    // make sure this isn't re-scheduled if interval and you attempt to
    // deregister yourself from within your own callback.
    isInterval_ = 0;
}

bool TimedEventHandler::IsPending()
{
    uint8_t retVal = Evm::IsRegisteredTimedEventHandler(this);
    
    return retVal;
}

bool TimedEventHandler::GetTimeQueued()
{
    return timeQueued_;
}

uint64_t TimedEventHandler::GetTimeoutTimeUs()
{
    return timeoutAbs_;
}


void Timer::OnTimedEvent()
{
    Timeline::Global().Event(origin_);

    cbFn_();
}