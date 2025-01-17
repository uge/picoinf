#pragma once

#include <cstdint>
#include <functional>
#include <source_location>


//////////////////////////////////////////////////////////////////////
//
// Timed Events
//
//////////////////////////////////////////////////////////////////////

class Micros
{
public:
    Micros(uint64_t value)
    : value_(value)
    {
        // nothing to do
    }

    uint64_t value_;
};

class TimedEventHandler
{
    friend class Evm;

public:

public:
    TimedEventHandler()
    : id_(idNext_)
    , isInterval_(0)
    {
        ++idNext_;
    }
    virtual ~TimedEventHandler() { Cancel(); }

    bool TimeoutAtMs(uint64_t absTimeMs);
    bool TimeoutInMs(uint64_t durationMs);
    bool TimeoutIntervalMs(uint64_t durationIntervalMs);
    bool TimeoutIntervalMs(uint64_t durationIntervalMs, uint64_t durationFirstInMs);

    bool TimeoutAtUs(Micros absTimeUs);
    bool TimeoutInUs(Micros durationUs);
    bool TimeoutIntervalUs(Micros durationIntervalUs);
    bool TimeoutIntervalUs(Micros durationIntervalUs, Micros durationFirstInUs);


    // bool TimeoutInMs(uint64_t durationMs);
    // bool TimeoutInUs(uint64_t durationUs);
    // bool TimeoutAtMs(uint64_t absTimeMs);
    // bool TimeoutAtUs(uint64_t absTimeUs);
    // trash Micros concept, name everything explicitly
    // move implementation inside Timer, clean up Evm
    // rename all timer instances to not be tedX but tX or whatever




    void Cancel();
    bool IsPending();
    bool GetTimeQueued();
    uint64_t GetTimeoutTimeUs();
    virtual void OnTimedEvent() = 0;


public:
    uint32_t id_;

protected:
    const char *origin_ = nullptr;

private:
    // Evm uses these for state keeping
    uint64_t timeQueued_;
    uint64_t timeoutAbs_ = 0;
    uint64_t durationUs_ = 0;
    bool     isInterval_;
    uint64_t durationIntervalUs_;
    uint64_t calledCount_ = 0;
    uint64_t seqNo_ = 0;

    inline static uint32_t idNext_ = 1;
    inline static uint64_t seqNoNext_ = 1;
};


//////////////////////////////////////////////////////////////////////
//
// Helpers
//
//////////////////////////////////////////////////////////////////////

class Timer
: public TimedEventHandler
{
public:
    Timer()
    {
        // Nothing to do
    }
    
    // tried to the nice way but none worked
    // https://www.techiedelight.com/find-name-of-the-calling-function-in-cpp/
    void SetCallback(std::function<void()> cbFn, const char *origin = std::source_location::current().function_name())
    {
        cbFn_ = cbFn;
        origin_ = origin;
    }
    
    std::function<void()> GetCallback()
    {
        return cbFn_;
    }
    
    void operator()()
    {
        GetCallback()();
    }

private:
    virtual void OnTimedEvent();

    std::function<void()> cbFn_;
};












