#include "Evm.h"
#include "Timeline.h"


bool TimedEventHandler::TimeoutAtMs(uint64_t absTimeMs)
{
    return TimeoutAtUs(absTimeMs * 1'000);
}

bool TimedEventHandler::TimeoutInMs(uint64_t durationMs)
{
    return TimeoutInUs(durationMs * 1'000);
}

bool TimedEventHandler::TimeoutIntervalMs(uint64_t durationIntervalMs)
{
    return TimeoutIntervalUs(durationIntervalMs * 1'000);
}

bool TimedEventHandler::TimeoutIntervalMs(uint64_t durationIntervalMs, uint64_t durationFirstInMs)
{
    return TimeoutIntervalUs(durationIntervalMs * 1'000, durationFirstInMs * 1'000);
}


bool TimedEventHandler::TimeoutAtUs(uint64_t absTimeUs)
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

    return Evm::RegisterTimedEventHandler(this, absTimeUs);
}

bool TimedEventHandler::TimeoutInUs(uint64_t durationUs)
{
    durationUs_ = durationUs;

    return TimeoutAtUs(PAL.Micros() + durationUs_);
}

bool TimedEventHandler::TimeoutIntervalUs(uint64_t durationIntervalUs)
{
    isInterval_ = 1;
    
    durationIntervalUs_ = durationIntervalUs;
    
    return TimeoutInUs(durationIntervalUs);
}

bool TimedEventHandler::TimeoutIntervalUs(uint64_t durationIntervalUs, uint64_t durationFirstInUs)
{
    isInterval_ = 1;
    
    durationIntervalUs_ = durationIntervalUs;
    
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