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
    Micros(uint64_t timeout)
    : timeout_(timeout)
    {
        // nothing to do
    }

    uint64_t timeout_;
};

class TimedEventHandler
{
    friend class Evm;

public:

public:
    TimedEventHandler()
    : id_(idNext_)
    , isInterval_(0)
    , isRigid_(0)
    {
        ++idNext_;
    }
    virtual ~TimedEventHandler() { DeRegisterForTimedEvent(); }

    bool RegisterForTimedEvent(uint64_t timeout);
    bool RegisterForTimedEventInterval(uint64_t timeout);
    bool RegisterForTimedEventInterval(uint64_t timeout, uint64_t firstTimeout);
    bool RegisterForTimedEventIntervalRigid(uint64_t timeout);
    bool RegisterForTimedEventIntervalRigid(uint64_t timeout, uint64_t firstTimeout);

    bool RegisterForTimedEvent(Micros timeout);
    bool RegisterForTimedEventInterval(Micros timeout);
    bool RegisterForTimedEventInterval(Micros timeout, Micros firstTimeout);
    bool RegisterForTimedEventIntervalRigid(Micros timeout);
    bool RegisterForTimedEventIntervalRigid(Micros timeout, Micros firstTimeout);

    void DeRegisterForTimedEvent();
    bool IsRegistered();
    bool GetTimeQueued();
    virtual void OnTimedEvent() = 0;


public:
    uint32_t id_;

protected:
    const char *origin_ = nullptr;

private:
    // Evm uses these for state keeping
    uint64_t timeQueued_;
    uint64_t timeout_;
    bool     isInterval_;
    bool     isRigid_;
    uint64_t intervalTimeout_;
    uint64_t calledCount_ = 0;

    inline static uint32_t idNext_ = 1;
    inline static uint32_t seqNoNext_ = 1;
};


//////////////////////////////////////////////////////////////////////
//
// Helpers
//
//////////////////////////////////////////////////////////////////////

class TimedEventHandlerDelegate
: public TimedEventHandler
{
public:
    TimedEventHandlerDelegate()
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












