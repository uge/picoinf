#include "Evm.h"
#include "Timeline.h"


static const uint64_t MAX_DURATION_MS = UINT64_MAX / 1'000; 

static void LogIfTooLarge(uint64_t ms)
{
    if (ms > MAX_DURATION_MS)
    {
        Log("ERR: TimedEventHandler ms delay (", ms, ") not <= max (", MAX_DURATION_MS, ")");
        PAL.Fatal("PAL::LogIfTooLarge");
    }
}

bool TimedEventHandler::TimeoutAtMs(uint64_t absTimeMs)
{
    LogIfTooLarge(absTimeMs);
    return TimeoutAtUs(Micros{absTimeMs * 1000});
}

bool TimedEventHandler::RegisterForTimedEvent(uint64_t durationIntervalMs)
{
    LogIfTooLarge(durationIntervalMs);

    return RegisterForTimedEvent(Micros{durationIntervalMs * 1000});
}

bool TimedEventHandler::TimeoutIntervalMs(uint64_t timeout)
{
    LogIfTooLarge(timeout);
    return TimeoutIntervalUs(Micros{timeout * 1000});
}

bool TimedEventHandler::TimeoutIntervalMs(uint64_t durationIntervalMs, uint64_t durationFirstInMs)
{
    LogIfTooLarge(durationIntervalMs);
    LogIfTooLarge(durationFirstInMs);
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

bool TimedEventHandler::RegisterForTimedEvent(Micros timeout)
{
    timeoutDelta_ = timeout.value_;

    return TimeoutAtUs(Micros{PAL.Micros() + timeoutDelta_});
}

bool TimedEventHandler::TimeoutIntervalUs(Micros durationIntervalUs)
{
    isInterval_ = 1;
    
    durationIntervalUs_ = durationIntervalUs.value_;
    
    return RegisterForTimedEvent(durationIntervalUs);
}

bool TimedEventHandler::TimeoutIntervalUs(Micros durationIntervalUs, Micros durationFirstInUs)
{
    isInterval_ = 1;
    
    durationIntervalUs_ = durationIntervalUs.value_;
    
    return RegisterForTimedEvent(durationFirstInUs);
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