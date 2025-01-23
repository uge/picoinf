#include "Evm.h"
#include "TimeClass.h"
#include "Timeline.h"
#include "Utl.h"

using namespace std;

#include "StrictMode.h"


/////////////////////////////////////////////////////////////////
// Constructor / Destructor
/////////////////////////////////////////////////////////////////

Timer::Timer()
: id_(idNext_)
{
    ++idNext_;
}

Timer::~Timer()
{
    Cancel();
}


/////////////////////////////////////////////////////////////////
// Setting and Getting Callback
/////////////////////////////////////////////////////////////////

// tried to the nice way but none worked
// https://www.techiedelight.com/find-name-of-the-calling-function-in-cpp/
void Timer::SetCallback(function<void()> cbFn, const char *origin)
{
    cbFn_ = cbFn;
    cbSetFromFn_ = origin;
}

function<void()> Timer::GetCallback()
{
    return cbFn_;
}

void Timer::operator()()
{
    GetCallback()();
}

/////////////////////////////////////////////////////////////////
// Timer Alignment
/////////////////////////////////////////////////////////////////

void Timer::SetSnapToMs(uint64_t ms)
{
    SetSnapToUs(ms * 1'000);
}

void Timer::SetSnapToUs(uint64_t us)
{
    snapToUs_ = us;
}
    

/////////////////////////////////////////////////////////////////
// Setting Timeout in Milliseconds
/////////////////////////////////////////////////////////////////

void Timer::TimeoutAtMs(uint64_t timeAtMs)
{
    TimeoutAtUs(timeAtMs * 1'000);
}

void Timer::TimeoutInMs(uint64_t durationMs)
{
    TimeoutInUs(durationMs * 1'000);
}

void Timer::TimeoutIntervalMs(uint64_t durationIntervalMs)
{
    TimeoutIntervalMs(durationIntervalMs, durationIntervalMs);
}

void Timer::TimeoutIntervalMs(uint64_t durationIntervalMs, uint64_t durationFirstInMs)
{
    TimeoutIntervalUs(durationIntervalMs * 1'000, durationFirstInMs * 1'000);
}


/////////////////////////////////////////////////////////////////
// Setting Timeout in Microseconds
/////////////////////////////////////////////////////////////////

void Timer::TimeoutAtUs(uint64_t timeAtUs)
{
    uint64_t registeredAtUs = PAL.Micros();

    // cancel existing timer if there is one, without touching
    // internal state, because that is used in Evm to find this
    // timer in a data structure
    Cancel();

    // stamp as early as possible
    registeredAtUs_ = registeredAtUs;
    durationUs_     = timeAtUs - registeredAtUs_;

    // work out absolute time of expiry
    timeoutAtUs_ = timeAtUs;

    // optionally snap to ms, only going forward in time when necessary to adjust
    snapToUsAtRegistration_ = snapToUs_;
    if (snapToUs_)
    {
        timeoutAtUs_ = (timeoutAtUs_ % snapToUs_) == 0 ?
                       timeoutAtUs_ :
                       ((timeoutAtUs_ / snapToUs_ * snapToUs_) + snapToUs_);
    }

    // set up interval details
    isInterval_         = false;
    durationIntervalUs_ = 0;

    // register
    Register();
}

void Timer::TimeoutInUs(uint64_t durationUs)
{
    uint64_t registeredAtUs = PAL.Micros();

    // cancel existing timer if there is one, without touching
    // internal state, because that is used in Evm to find this
    // timer in a data structure
    Cancel();

    // stamp as early as possible
    registeredAtUs_ = registeredAtUs;
    durationUs_     = durationUs;

    // work out absolute time of expiry
    timeoutAtUs_ = registeredAtUs_ + durationUs_;

    // optionally snap to ms, only going forward in time when necessary to adjust
    snapToUsAtRegistration_ = snapToUs_;
    if (snapToUs_)
    {
        timeoutAtUs_ = (timeoutAtUs_ % snapToUs_) == 0 ?
                       timeoutAtUs_ :
                       ((timeoutAtUs_ / snapToUs_ * snapToUs_) + snapToUs_);
    }

    // set up interval details
    isInterval_         = false;
    durationIntervalUs_ = 0;

    // register
    Register();
}

void Timer::TimeoutIntervalUs(uint64_t durationIntervalUs)
{
    TimeoutIntervalUs(durationIntervalUs, durationIntervalUs);
}

void Timer::TimeoutIntervalUs(uint64_t durationIntervalUs, uint64_t durationFirstInUs)
{
    // register before interval set since that would get clobbered
    TimeoutInUs(durationFirstInUs);

    // set up interval details
    isInterval_         = true;
    durationIntervalUs_ = durationIntervalUs;
}


/////////////////////////////////////////////////////////////////
// Timer State
/////////////////////////////////////////////////////////////////

bool Timer::IsPending()
{
    uint8_t retVal = Evm::IsTimerRegistered(this);
    
    return retVal;
}

void Timer::Cancel()
{
    Evm::DeRegisterTimer(this);
    
    // make sure this isn't re-scheduled if an interval callback
    // decides to deregister itself.
    isInterval_ = false;
}


/////////////////////////////////////////////////////////////////
// EVM Interface
/////////////////////////////////////////////////////////////////

uint64_t Timer::GetSeqNo()
{
    return seqNo_;
}

uint64_t Timer::GetTimeoutAtUs()
{
    return timeoutAtUs_;
}

void Timer::OnTimeout()
{
    if (visibleInTimeline_)
    {
        Timeline::Global().Event(cbSetFromFn_);
    }

    ++timeoutCount_;

    cbFn_();

    // interval callback might have canceled itself
    if (isInterval_)
    {
        // no need to cancel this timer, it has just gone off.
        // the callback has either:
        // - canceled (and it won't get here)
        // - taken no timer action (and so interval remains)
        // - changed to At/In (and it won't get here)
        // - changed to interval again (at which point everything is fine)
            // hmm, not quite fine actually, just don't do that one

        // don't stamp the registeredAt, let the original persist

        // work out absolute time of expiry
        timeoutAtUs_ += durationIntervalUs_;

        // no need to set up interval details
        // no need to update duration, it is unchanged

        // register
        Register();
    }
}


/////////////////////////////////////////////////////////////////
// Debug
/////////////////////////////////////////////////////////////////

void Timer::SetVisibleInTimeline(bool tf)
{
    visibleInTimeline_ = tf;
}

void Timer::Print(uint64_t timeNowUs)
{
    if (timeNowUs == 0)
    {
        timeNowUs = PAL.Micros();
    }

    int64_t durationRemainingUs = (int64_t)(timeoutAtUs_ - timeNowUs);

    Log("cbSetFromFn_            : ", cbSetFromFn_);
    Log("isInterval_             : ", isInterval_);
    Log("ptr                     : ", Commas((uint32_t)this));
    Log("id_                     : ", id_);
    Log("seqNo_                  : ", Commas(seqNo_));
    Log("snapToUs_               : ", Commas(snapToUs_));
    Log("snapToUsAtRegistration_ : ", Commas(snapToUsAtRegistration_));
    Log("timeoutAtUs_            : ", Time::GetNotionalTimeAtSystemUs(timeoutAtUs_),       " - ", StrUtl::PadLeft(Commas(timeoutAtUs_),        ' ', 13));
    Log("registeredAtUs_         : ", Time::GetNotionalTimeAtSystemUs(registeredAtUs_),    " - ", StrUtl::PadLeft(Commas(registeredAtUs_),     ' ', 13));
    Log("durationUs_             : ", Time::MakeTimeFromUs(durationUs_),                   " - ", StrUtl::PadLeft(Commas(durationUs_),         ' ', 13));
    Log("durationRemainingUs     : ", Time::MakeTimeFromUs((uint64_t)durationRemainingUs), " - ", StrUtl::PadLeft(Commas(durationRemainingUs), ' ', 13));
    Log("timeoutCount_           : ", Commas(timeoutCount_));
}


/////////////////////////////////////////////////////////////////
// Common Registration Logic
/////////////////////////////////////////////////////////////////

void Timer::Register()
{
    // update seqno
    seqNo_ = seqNoNext_;
    ++seqNoNext_;

    Evm::RegisterTimer(this);
}
