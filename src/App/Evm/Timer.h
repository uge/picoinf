#pragma once

#include <cstdint>
#include <functional>
#include <source_location>


// rename all timer instances to not be tedX but tX or whatever

class Timer
{
public:

    /////////////////////////////////////////////////////////////////
    // Constructor / Destructor
    /////////////////////////////////////////////////////////////////

    Timer(const char *name = std::source_location::current().function_name());
    ~Timer();


    /////////////////////////////////////////////////////////////////
    // No copy or move
    /////////////////////////////////////////////////////////////////

    Timer(const Timer &)            = delete;
    Timer& operator=(const Timer &) = delete;
    
    Timer(Timer &&)            = delete;
    Timer& operator=(Timer &&) = delete;


    /////////////////////////////////////////////////////////////////
    // Setting and Getting Callback
    /////////////////////////////////////////////////////////////////

    void SetCallback(std::function<void()> cbFn);
    std::function<void()> GetCallback() const;
    void operator()();

    /////////////////////////////////////////////////////////////////
    // Timer Alignment
    /////////////////////////////////////////////////////////////////

    void SetSnapToMs(uint64_t ms);
    void SetSnapToUs(uint64_t us);


    /////////////////////////////////////////////////////////////////
    // Setting Timeout in Milliseconds
    /////////////////////////////////////////////////////////////////

    void TimeoutAtMs(uint64_t timeAtMs);
    void TimeoutInMs(uint64_t durationMs);
    void TimeoutIntervalMs(uint64_t durationIntervalMs);
    void TimeoutIntervalMs(uint64_t durationIntervalMs, uint64_t durationFirstInMs);


    /////////////////////////////////////////////////////////////////
    // Setting Timeout in Microseconds
    /////////////////////////////////////////////////////////////////

    void TimeoutAtUs(uint64_t timeAtUs);
    void TimeoutInUs(uint64_t durationUs);
    void TimeoutIntervalUs(uint64_t durationIntervalUs);
    void TimeoutIntervalUs(uint64_t durationIntervalUs, uint64_t durationFirstInUs);


    /////////////////////////////////////////////////////////////////
    // Timer State
    /////////////////////////////////////////////////////////////////

    bool IsPending() const;
    void Cancel();


    /////////////////////////////////////////////////////////////////
    // EVM Interface
    /////////////////////////////////////////////////////////////////
  
    uint64_t GetSeqNo() const;
    uint64_t GetTimeoutAtUs() const;
    void OnTimeout();


    /////////////////////////////////////////////////////////////////
    // Debug
    /////////////////////////////////////////////////////////////////
    
    void SetName(const char *name);
    const char *GetName() const;
    void SetVisibleInTimeline(bool tf);
    void Print(uint64_t timeNowUs = 0) const;


private:

    /////////////////////////////////////////////////////////////////
    // Common Registration Logic
    /////////////////////////////////////////////////////////////////

    void Register();


private:

    // provide ordering tie-breaking mechanism by giving each new timer
    // registration a unique and incrementing number.
    // this allows any number of timers to share the same expiry and
    // retain an execution order of earliest-to-latest-to-register.
    uint64_t               seqNo_     = 0;
    inline static uint64_t seqNoNext_ = 1;

    // configuration
    uint64_t snapToUs_               = 0;
    uint64_t snapToUsAtRegistration_ = 0;

    // the absolute time this timer expires at.
    uint64_t timeoutAtUs_ = 0;

    // support interval mode.
    bool     isInterval_         = false;
    uint64_t durationIntervalUs_ = 0;

    // callback data
    const char *name_; // put in timeline
    std::function<void()> cbFn_ = []{};

    // debug data
    bool visibleInTimeline_ = true;

    uint64_t               id_     = 0;
    inline static uint64_t idNext_ = 1;

    uint64_t registeredAtUs_ = 0;
    uint64_t durationUs_     = 0;
    uint64_t timeoutCount_   = 0;
};

