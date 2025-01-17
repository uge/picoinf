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

bool TimedEventHandler::RegisterForTimedEvent(uint64_t timeout)
{
    LogIfTooLarge(timeout);

    return RegisterForTimedEvent(Micros{timeout * 1000});
}

bool TimedEventHandler::RegisterForTimedEventInterval(uint64_t timeout)
{
    LogIfTooLarge(timeout);
    return RegisterForTimedEventInterval(Micros{timeout * 1000});
}

bool TimedEventHandler::RegisterForTimedEventInterval(uint64_t timeout, uint64_t firstTimeout)
{
    LogIfTooLarge(timeout);
    LogIfTooLarge(firstTimeout);
    return RegisterForTimedEventInterval(Micros{timeout * 1000}, Micros{firstTimeout * 1000});
}

bool TimedEventHandler::RegisterForTimedEventIntervalRigid(uint64_t timeout)
{
    LogIfTooLarge(timeout);
    return RegisterForTimedEventIntervalRigid(Micros{timeout * 1000});
}

bool TimedEventHandler::RegisterForTimedEventIntervalRigid(uint64_t timeout, uint64_t firstTimeout)
{
    LogIfTooLarge(timeout);
    LogIfTooLarge(firstTimeout);
    return RegisterForTimedEventIntervalRigid(Micros{timeout * 1000}, Micros{firstTimeout * 1000});
}


bool TimedEventHandler::TimeoutAtUs(Micros absTimeUs)
{
    // Don't allow yourself to be scheduled more than once.
    // Cache whether this is an interval callback since that
    // gets reset during cancel.
    bool isIntervalCache = isInterval_;
    bool isRigidCache    = isRigid_;
    Cancel();
    isInterval_ = isIntervalCache;
    isRigid_    = isRigidCache;

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

bool TimedEventHandler::RegisterForTimedEventInterval(Micros timeout)
{
    isInterval_ = 1;
    isRigid_    = 0;
    
    intervalTimeout_ = timeout.value_;
    
    return RegisterForTimedEvent(timeout);
}

bool TimedEventHandler::RegisterForTimedEventInterval(Micros timeout, Micros firstTimeout)
{
    isInterval_ = 1;
    isRigid_    = 0;
    
    intervalTimeout_ = timeout.value_;
    
    return RegisterForTimedEvent(firstTimeout);
}

bool TimedEventHandler::RegisterForTimedEventIntervalRigid(Micros timeout)
{
    uint8_t retVal = RegisterForTimedEventInterval(timeout);
    
    isRigid_ = 1;
    
    return retVal;
}

bool TimedEventHandler::RegisterForTimedEventIntervalRigid(Micros timeout, Micros firstTimeout)
{
    uint8_t retVal = RegisterForTimedEventInterval(timeout, firstTimeout);
    
    isRigid_ = 1;
    
    return retVal;
}


void TimedEventHandler::Cancel()
{
    Evm::DeRegisterTimedEventHandler(this);
    
    // make sure this isn't re-scheduled if interval and you attempt to
    // deregister yourself from within your own callback.
    isInterval_ = 0;
    isRigid_    = 0;
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