#include "Evm.h"
#include "PAL.h"
#include "Utl.h"
#include "Shell.h"
#include "TimeClass.h"
#include "Timeline.h"

#include <vector>
using namespace std;

static bool verboseTimeline_ = false;
static Timeline timeline_;

static void MaybeEvent(const char *str)
{
    if (verboseTimeline_)
    {
        timeline_.Event(str);
    }
}


//////////////////////////////////////////////////////////////////////
// Evm Core
//////////////////////////////////////////////////////////////////////

uint64_t Evm::GetDurationUsToNextTimerTimeout(uint8_t expectedStackDepth)
{
    uint64_t sleepUs;

    if (!timerList_.empty())
    {
        Timer *timer = *timerList_.begin();

        uint64_t timeNow = PAL.Micros();

        if (timer->GetTimeoutAtUs() <= timeNow)
        {
            sleepUs = 0;
        }
        else
        {
            sleepUs = timer->GetTimeoutAtUs() - timeNow;
        }
    }
    else
    {
        // arbitrary
        sleepUs = 10'000'000;
    }

    // fast breakout for when a MainLoop stack exits, we want to
    // not have a long sleep period with (say) no, or distant, timers.
    if (mainLoopStackDepth_ != expectedStackDepth)
    {
        sleepUs = 0;
    }

    return sleepUs;
}

void Evm::DisableAutoLogAsync()
{
    autoLogAsync_ = false;
}

void Evm::MainLoop()
{
    // I'm handling two types of pre-emption corruption sources:
    // - ISRs
    // - Other threads
    //
    // By default, the main thread is pre-emptable, running at priority 0.
    // This leaves it open to being suspended for ISRs or other high-priority
    // threads.
    //
    // ISRs can fire for my thread, or another thread.
    //   In both cases, I stop them from happening during the short period where
    //   ISR structures are being operated.
    //
    // Other threads can preempt the main thread
    //   This is likely due to them getting an ISR, becoming ready, and punching
    //   through my execution.
    //
    //   I actually want them to get their ISR, do some work, become ready.
    //   I don't want them punching through when I'm executing code, because the
    //   callbacks they have lead to my code, which doesn't know the main thread
    //   is possibly running, and so corruption of state may occur.
    //
    // Ideally I want to run my tasks without another thread breaking in, but
    // between tasks other threads can do their work, even if I'm not sleeping
    // at that time.
    //
    // To do this, I'm going to temporarily suspend my pre-emptable state while
    // executing my tasks.  I don't care if other threads punch through into
    // the Main loop, there's nothing going on here but scheduling tasks.
    //
    // I also want to make sure though that I don't wind up going to sleep when
    // an ISR has fired but I haven't re-looked at it.
    //   As in, I finish handling ISRs, move toward sleeping, ISR occurs,
    //   I sleep anyway becaues I didn't notice.  That'd be bad.
    //
    // Once I can be sure other threads only execute between tasks, I don't even
    // have to send their callbacks through the ISR handler anymore.
    //   Update -- not totally true.
    //   Lower priority threads, like shell, can actually get interrupted by main thread.
    //   So if you, by hand, are executing some main-thread code, you don't want main
    //   to jump in (by timer), pre-empt shell and start executing.
    //   That's exactly the problem you had with higher priority threads doing to you.
    //   So, rule is, any lower-priority thread has to send commands via IRQ callback
    //   to get them safely within main before executing.
    //   Higher priority threads can no longer interrupt execept when I declare safe
    //   via YieldToAll, when I know I'm not executing anything.
    //
    // Presently only the shell is a thread providing callbacks at a lower
    // priority than main.

    // App starts system as being a cooperative thread (-1), so we don't have to worry
    // about locking out other higher-priority threads via SchedulerLock/Unlock

    // Put in async if not already
    if (autoLogAsync_)
    {
        LogModeAsync();
    }

    ++mainLoopStackDepth_;
    uint8_t expectedStackDepth = mainLoopStackDepth_;

    while (mainLoopStackDepth_ == expectedStackDepth)
    {
        uint64_t timeStart = PAL.Micros();

        stats_.HANDLED_WORK += ServiceWork();
        stats_.HANDLED_WORK += ServiceLowPriorityWork();
        uint64_t timePostIsr = PAL.Micros();
        stats_.HANDLED_TIMED += ServiceTimers();
        uint64_t timePostTimed = PAL.Micros();

        // Determine whether to keep processing or if sleep is an option
        if (fnWorkList_.Count() == 0 && fnLowPriorityWorkList_.Count() == 0)
        {
            // is there time to sleep?
            uint64_t timeToSleep = GetDurationUsToNextTimerTimeout(expectedStackDepth);
            if (timeToSleep)
            {
                uint64_t timeToWake = PAL.Micros() + timeToSleep;
                sem_.Take(timeToSleep);
                int64_t timeDiff = timeToWake - PAL.Micros();

                if (timeDiff < 0)
                {
                    ++stats_.COUNT_LATENT_WAKE;
                    stats_.TIME_SUM_LATENT += -timeDiff;
                }
            }
        }
        else
        {
            ++stats_.SKIPPED_SLEEP_FOR_WORK;
        }

        // actually structure this better so I know if I'm actually skipping
        // sleep or not.
        // I want to know how often each outcome is reached


        ++stats_.LOOPS;

        uint64_t timePostSleep = PAL.Micros();
        stats_.TIME_IN_WORK  += timePostIsr   - timeStart;
        stats_.TIME_IN_TIMED += timePostTimed - timePostIsr;
        stats_.TIME_IN_SLEEP += timePostSleep - timePostTimed;
    }
}

void Evm::ExitMainLoop()
{
    --mainLoopStackDepth_;
}

uint8_t Evm::GetStackDepth()
{
    return mainLoopStackDepth_;
}


//////////////////////////////////////////////////////////////////////
// Interrupts
//////////////////////////////////////////////////////////////////////

void Evm::QueueWork(const char *label, FnWork &fnWork)
{
    Evm::QueueWorkInternal(label, fnWork);
}

void Evm::QueueWork(const char *label, FnWork &&fnWork)
{
    Evm::QueueWorkInternal(label, fnWork);
}

void Evm::QueueWorkInternal(const char *label, FnWork &fnWork)
{
    timeline_.Event(label);

    // Caution must be used when sending work.
    //
    // Lambdas must capture only [this], a single reference, or
    // some 4-byte-or-less value.
    //
    // This functionality relies on small-buffer-optimization to work,
    // and so must not trigger dynamic memory allocation in either the
    // lambda or the function<> object.
    //
    // The work queue byte-wise copying a function<> object and so any
    // allocations will be ultimately a use-after-free operation.
    //
    // An example of this limit being broken was evm.delaybusy capturing
    // a uint64_t by value and it coming out garbage on the other side.
    //
    // (The exact limits have not been not exhaustively tested, eg, can
    // you capture 4 1-byte values? Is the limit actually 4?)
    fnWorkList_.Put(fnWork);
    sem_.Give();
}

uint32_t Evm::ServiceWork()
{
    const uint8_t MAX_EVENTS_HANDLED = 4;
    
    uint8_t remainingEvents = MAX_EVENTS_HANDLED;
    
    while (fnWorkList_.Count() && remainingEvents)
    {
        FnWork fnWork;
        fnWorkList_.Get(fnWork);
        
        // Execute
        timeline_.Event("EVM_WORK_START");
        fnWork();
        timeline_.Event("EVM_WORK_END");
 
        // Keep track of remaining events willing to handle
        --remainingEvents;
    }
    
    // Return number handled
    return MAX_EVENTS_HANDLED - remainingEvents;
}

//////////////////////////////////////////////////////////////////////
// Interrupts (missable)
//////////////////////////////////////////////////////////////////////

void Evm::QueueLowPriorityWork(const char *label, FnWork &fnWork)
{
    Evm::QueueLowPriorityWorkInternal(label, fnWork);
}

void Evm::QueueLowPriorityWork(const char *label, FnWork &&fnWork)
{
    Evm::QueueLowPriorityWorkInternal(label, fnWork);
}

void Evm::QueueLowPriorityWorkInternal(const char *label, FnWork &fnWork)
{
    timeline_.Event(label);

    // if full, drop the oldest to make room for newest
    if (fnLowPriorityWorkList_.IsFull())
    {
        WorkData tmp;
        fnLowPriorityWorkList_.Get(tmp, 0);
    }

    fnLowPriorityWorkList_.Put(WorkData{label, fnWork});
    sem_.Give();
}

uint32_t Evm::ClearLowPriorityWorkByLabel(const char *label)
{
    uint32_t numDeleted = 0;

    IrqLock lock;
    PAL.SchedulerLock();

    // get a list of the work you want to keep
    vector<WorkData> wdListKeep;
    while (fnLowPriorityWorkList_.Count())
    {
        WorkData wd;
        fnLowPriorityWorkList_.Get(wd, 0);

        if (wd.label != label)
        {
            wdListKeep.push_back(wd);
        }
        else
        {
            ++numDeleted;
        }
    }

    // should be empty but why not
    fnLowPriorityWorkList_.Flush();

    // re-populate
    for (auto &wd : wdListKeep)
    {
        fnLowPriorityWorkList_.Put(wd, 0);
    }

    PAL.SchedulerUnlock();

    return numDeleted;
}

uint32_t Evm::ServiceLowPriorityWork()
{
    const uint8_t MAX_EVENTS_HANDLED = 4;
    
    uint8_t remainingEvents = MAX_EVENTS_HANDLED;
    
    while (fnLowPriorityWorkList_.Count() && remainingEvents)
    {
        WorkData workData;
        fnLowPriorityWorkList_.Get(workData);
        FnWork &fnWork = workData.fnWork;
        
        // Execute
        MaybeEvent("EVM_LOW_PRIO_WORK_START");
        timeline_.Event(workData.label);
        fnWork();
        MaybeEvent("EVM_LOW_PRIO_WORK_END");
 
        // Keep track of remaining events willing to handle
        --remainingEvents;
    }
    
    // Return number handled
    return MAX_EVENTS_HANDLED - remainingEvents;
}



//////////////////////////////////////////////////////////////////////
// Timed Events
//////////////////////////////////////////////////////////////////////

void Evm::RegisterTimer(Timer *timer)
{
    timerList_.insert(timer);
}

void Evm::DeRegisterTimer(Timer *timer)
{
    timerList_.erase(timer);
}

bool Evm::IsTimerRegistered(const Timer *timer)
{
    return timerList_.contains((Timer *)timer);
}

uint32_t Evm::ServiceTimers()
{
    const uint8_t MAX_EVENTS_HANDLED = 1;
    uint8_t remainingEvents = MAX_EVENTS_HANDLED;
    
    if (!timerList_.empty())
    {
        bool   keepGoing = true;
        Timer *timer     = NULL;
        
        do {
            timer = *timerList_.begin();
            
            // check if the timer expiry is in the past
            if (timer->GetTimeoutAtUs() <= PAL.Micros())
            {
                // drop this element from the list
                timerList_.erase(timer);
                
                // invoke the IdleTimeEventHandler
                MaybeEvent("EVM_TIMED_START");
                timer->OnTimeout();
                MaybeEvent("EVM_TIMED_END");
                
                // only keep going if remaining quota of events remains
                --remainingEvents;
                keepGoing = remainingEvents;
            }
            else
            {
                keepGoing = false;
            }
        } while (keepGoing && !timerList_.empty());
    }

    // Return events handled
    return MAX_EVENTS_HANDLED - remainingEvents;
}


//////////////////////////////////////////////////////////////////////
// Observability
//////////////////////////////////////////////////////////////////////

void Evm::SetTimelineVerbose(bool tf)
{
    verboseTimeline_ = tf;
}

void Evm::DumpStats()
{
    uint64_t timeNow = PAL.Micros();

    LogNL();
    Log("Evm Stats");
    LogNL();

    // dump historical
    for (uint32_t i = 0; i < statsHistory_.Size(); ++i)
    {
        StatsSnapshot &ss = statsHistory_[i];

        Log("Stats from ", Time::GetNotionalTimeAtSystemUs(ss.snapshotTime));
        DumpStats(ss.stats, STATS_INTERVAL_MS * 1'000);
        LogNL();
    }

    // dump current
    uint64_t currentStatsStartedAt = 0;
    if (statsHistory_.Size())
    {
        currentStatsStartedAt = statsHistory_[statsHistory_.Size() - 1].snapshotTime;
    }
    uint64_t durationCurrentStats = timeNow - currentStatsStartedAt;

    Log("Current Stats: ", Time::GetNotionalTimeAtSystemUs(timeNow));
    DumpStats(stats_, durationCurrentStats);
    LogNL();
}

Evm::Stats Evm::GetStatsDelta(Stats &s1, Stats &s2)
{
    Stats s;

    s.LOOPS                  = s1.LOOPS                  - s2.LOOPS;
    s.HANDLED_WORK           = s1.HANDLED_WORK           - s2.HANDLED_WORK;
    s.HANDLED_TIMED          = s1.HANDLED_TIMED          - s2.HANDLED_TIMED;
    s.SKIPPED_SLEEP_FOR_WORK = s1.SKIPPED_SLEEP_FOR_WORK - s2.SKIPPED_SLEEP_FOR_WORK;
    s.TIME_IN_WORK           = s1.TIME_IN_WORK           - s2.TIME_IN_WORK;
    s.TIME_IN_TIMED          = s1.TIME_IN_TIMED          - s2.TIME_IN_TIMED;
    s.TIME_IN_SLEEP          = s1.TIME_IN_SLEEP          - s2.TIME_IN_SLEEP;
    s.COUNT_LATENT_WAKE      = s1.COUNT_LATENT_WAKE      - s2.COUNT_LATENT_WAKE;
    s.TIME_SUM_LATENT        = s1.TIME_SUM_LATENT        - s2.TIME_SUM_LATENT;

    return s;
}

void Evm::DumpStats(Stats &stats, uint32_t duration)
{
    auto fnFormat = [](uint32_t val){
        return StrUtl::PadLeft(Commas(val), ' ', 9);
    };

    uint32_t rateIsr     = 0;
    uint32_t rateTimed   = 0;
    uint32_t rateSkipped = 0;
    uint32_t rateLatent  = 0;

    if (duration)
    {
        rateIsr     = stats.HANDLED_WORK           / ((double)duration / 1'000'000);
        rateTimed   = stats.HANDLED_TIMED          / ((double)duration / 1'000'000);
        rateSkipped = stats.SKIPPED_SLEEP_FOR_WORK / ((double)duration / 1'000'000);
        rateLatent  = stats.COUNT_LATENT_WAKE      / ((double)duration / 1'000'000);
    }

    uint32_t totalTime = stats.TIME_IN_WORK + stats.TIME_IN_TIMED + stats.TIME_IN_SLEEP;
    uint8_t pctIsr   = 0;
    uint8_t pctTimed = 0;
    uint8_t pctSleep = 0;
    uint8_t pctLatent = 0;

    uint32_t timeUnaccountedFor = 0;
    if (duration > totalTime)
    {
        timeUnaccountedFor = duration - totalTime;
    }

    if (totalTime)
    {
        pctIsr   = (stats.TIME_IN_WORK  * 100) / totalTime;
        pctTimed = (stats.TIME_IN_TIMED * 100) / totalTime;
        pctSleep = (stats.TIME_IN_SLEEP * 100) / totalTime;
        pctLatent = (stats.TIME_SUM_LATENT * 100) / totalTime;
    }

    Log("HANDLED_WORK          : ", fnFormat(stats.HANDLED_WORK),           " (", Commas(rateIsr),     " / sec)");
    Log("HANDLED_TIMED         : ", fnFormat(stats.HANDLED_TIMED),          " (", Commas(rateTimed),   " / sec)");
    Log("SKIPPED_SLEEP_FOR_WORK: ", fnFormat(stats.SKIPPED_SLEEP_FOR_WORK), " (", Commas(rateSkipped), " / sec)");
    Log("TIME_IN_WORK          : ", fnFormat(stats.TIME_IN_WORK),           " (", pctIsr,              " %)");
    Log("TIME_IN_TIMED         : ", fnFormat(stats.TIME_IN_TIMED),          " (", pctTimed,            " %)");
    Log("TIME_IN_SLEEP         : ", fnFormat(stats.TIME_IN_SLEEP),          " (", pctSleep,            " %)");
    Log("COUNT_LATENT_WAKE     : ", fnFormat(stats.COUNT_LATENT_WAKE),      " (", Commas(rateLatent),  " / sec)");
    Log("TIME_SUM_LATENT       : ", fnFormat(stats.TIME_SUM_LATENT),        " (", pctLatent,           " %)");
    Log("LOOPS                 : ", fnFormat(stats.LOOPS));
    Log("Unaccounted Time      : ", fnFormat(timeUnaccountedFor));
}


//////////////////////////////////////////////////////////////////////
// Testing
//////////////////////////////////////////////////////////////////////

void Evm::DebugTimer(const char *str)
{
    LogNL();
    Log(str);
    LogNL();

    // log current time
    uint64_t timeNowUs = PAL.Micros();
    Log("Current time - ", Time::GetNotionalTimeAtSystemUs(timeNowUs), " - ", Commas(timeNowUs));

    Log("Timers pending: ", timerList_.size());
    LogNL();
    for (auto &timer : timerList_)
    {
        timer->Print(timeNowUs);
        LogNL();
    }
}

void Evm::TestTimer()
{
    // Timer functionality to test. Timers:
    // - can bet set At a given moment in time
    //   - including in the past
    // - can be set In a given duration in the future (only 0 or positive duration)
    //   - equates to an expiry At a given time
    // - multiple timers can be due to expire at the same time
    //   - when they do, they expire in the sequential order of registration
    //     - earliest-scheduler-wins
    // - timers which fire and re-schedule for the same expiry time cannot
    //   starve out other timers scheduled for the same time due to the
    //   earliest-scheduler-wins ordering


    // testing that follows snaps the ms-timers to the ms, eliminating any
    // us component. this makes testing aligning equal times easier.
    // this is not the default behavior.


    // align to a millisecond to start, to help make timer setting
    // more certain. the tests below unfortunately suffer from the clock
    // running during their setup and registration.
    while (PAL.Micros() % 1'000 != 0);


    uint64_t timeNowUs = PAL.Micros();
    uint64_t timeNowMs = timeNowUs / 1'000;
    uint64_t durationGapMs = 2;
    int count = 0;
    int quitAfter = 6;


    // time out in the past
    // should see this go absolutely first
    Timer t0("t0");
    t0.SetCallback([&]{
        Log('[', PAL.Micros(), ", ", t0.GetTimeoutAtUs(), "] ", t0.GetName());
    });
    t0.TimeoutAtMs(0);





    // should follow t0 since this is scheduled for the past.
    // they should go one after the other scheduled for the same time.

    // time out in the past
    Timer t0_1("t0_1");
    t0_1.SetCallback([&]{
        Log('[', PAL.Micros(), ", ", t0_1.GetTimeoutAtUs(), "] ", t0_1.GetName());
    });
    t0_1.SetSnapToMs(1);
    t0_1.TimeoutAtMs(timeNowMs - durationGapMs);

    Timer t0_2("t0_2");
    t0_2.SetCallback([&]{
        Log('[', PAL.Micros(), ", ", t0_2.GetTimeoutAtUs(), "] ", t0_2.GetName());
    });
    t0_2.SetSnapToMs(1);
    t0_2.TimeoutAtMs(timeNowMs - durationGapMs);




    // interval timer in microseconds
    // should go first from 'now', and repeat, starting at some us past the ms mark
    Timer t0_i_us("t0_i_us");
    t0_i_us.SetCallback([&]{
        Log('[', PAL.Micros(), ", ", t0_i_us.GetTimeoutAtUs(), "] ", t0_i_us.GetName());
    });
    t0_i_us.TimeoutIntervalUs(1'000, 0);




    // these two interval timers should go next, then alternate with each other
    // every millisecond.
    Timer t1_i_1("t1_i_1");
    t1_i_1.SetCallback([&]{
        Log('[', PAL.Micros(), ", ", t1_i_1.GetTimeoutAtUs(), "] ", t1_i_1.GetName());
    });
    t1_i_1.SetSnapToMs(1);
    t1_i_1.TimeoutIntervalMs(1, 0);

    Timer t1_i_2("t1_i_2");
    t1_i_2.SetCallback([&]{
        Log('[', PAL.Micros(), ", ", t1_i_2.GetTimeoutAtUs(), "] ", t1_i_2.GetName());

        ++count;
        if (count == quitAfter)
        {
            t0_i_us.Cancel();
            t1_i_1.Cancel();
            t1_i_2.Cancel();
        }
    });
    t1_i_2.SetSnapToMs(1);
    t1_i_2.TimeoutIntervalMs(1, 0);





    // t2, t3 time out in the future, clash/follow t2 and beat t1_i_x to same timeout
    Timer t2("t2");
    t2.SetCallback([&]{
        Log('[', PAL.Micros(), ", ", t2.GetTimeoutAtUs(), "] ", t2.GetName());
    });
    t2.SetSnapToMs(1);
    t2.TimeoutAtMs(timeNowMs + durationGapMs);

    Timer t3("t3");
    t3.SetCallback([&]{
        Log('[', PAL.Micros(), ", ", t3.GetTimeoutAtUs(), "] ", t3.GetName());
    });
    t3.SetSnapToMs(1);
    t3.TimeoutAtMs(timeNowMs + durationGapMs);






    // interval timer cancels itself and beats all others to this time
    Timer t4_i_cancel("t4_i_cancel");
    t4_i_cancel.SetCallback([&]{
        Log('[', PAL.Micros(), ", ", t4_i_cancel.GetTimeoutAtUs(), "] ", t4_i_cancel.GetName());
        t4_i_cancel.Cancel();
    });
    t4_i_cancel.SetSnapToMs(1);
    t4_i_cancel.TimeoutIntervalMs(1, durationGapMs + 2);




    // interval timer converts to In timer
    // should see interval timer fire at immediately subsequently to t4_i_cancel
    // should see In timer fire some time after
    Timer t5_i_to_a("t5_i_to_a");
    t5_i_to_a.SetCallback([&]{
        Log('[', PAL.Micros(), ", ", t5_i_to_a.GetTimeoutAtUs(), "] ", "t5_i_to_a as Interval");

        t5_i_to_a.SetCallback([&]{
            Log('[', PAL.Micros(), ", ", t5_i_to_a.GetTimeoutAtUs(), "] ", "t5_i_to_a as In");
        });
        t5_i_to_a.TimeoutInMs(0);
    });
    t5_i_to_a.SetSnapToMs(1);
    t5_i_to_a.TimeoutIntervalMs(1, durationGapMs + 2);









    DebugTimer();
    LogNL();
    Log("Now: ", timeNowUs, ", ", timeNowMs);
    LogNL();

    Evm::MainLoop();
}




//////////////////////////////////////////////////////////////////////
// Storage
//////////////////////////////////////////////////////////////////////

KMessagePipe<Evm::FnWork,   Evm::MAX_WORK_ITEMS> Evm::fnWorkList_;
KMessagePipe<Evm::WorkData, Evm::MAX_WORK_ITEMS> Evm::fnLowPriorityWorkList_;

KSemaphore Evm::sem_;
multiset<Timer *, Evm::CmpTimer> Evm::timerList_;

Evm::Stats Evm::stats_;
CircularBuffer<Evm::StatsSnapshot> Evm::statsHistory_;
Timer Evm::timerStats_("TIMER_EVM_STATS_HISTORY");
Timer Evm::timerWatchdog_("TIMER_EVM_WATCHDOG");

Timer Evm::timerTest1_;
Timer Evm::timerTest2_;





////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////

void Evm::Init()
{
    Timeline::Global().Event("Evm::Init");

    // register with ktime for callbacks
    KTime::RegisterCallbackScalingFactorChange([]{
        Timeline::Global().Event("EVM_HANDLE_SCALING_FACTOR");
        // wake up evm main loop
        sem_.Give();
    });

    // Keep track of the stats on a rolling basis
    statsHistory_.SetCapacity(STATS_HISTORY_COUNT);

    timerStats_.SetCallback([]{
        // capture existing stats
        statsHistory_.PushBack({
            .snapshotTime = PAL.Micros(),
            .stats = stats_,
        });

        // reset current stats
        stats_ = Stats{};
    });
    timerStats_.SetSnapToMs(STATS_INTERVAL_MS);
    timerStats_.TimeoutIntervalMs(STATS_INTERVAL_MS);

    static const uint64_t WATCHDOG_TIMEOUT_MS = 5'000;
    timerWatchdog_.SetCallback([]{
        static bool started = false;

        if (!started)
        {
            Watchdog::SetTimeout(WATCHDOG_TIMEOUT_MS);
            Watchdog::Start();

            started = true;
        }
        else
        {
            Watchdog::Feed();
        }
    });
    // timerWatchdog_.TimeoutIntervalMs(WATCHDOG_TIMEOUT_MS - 2'000, 0);

    PAL.RegisterOnFatalHandler("Evm Fatal Handler", []{
        LogModeSync();
        DumpStats();
        LogModeAsync();
        timeline_.ReportNow();
    });
}

void Evm::SetupShell()
{
    Timeline::Global().Event("Evm::SetupShell");

    Shell::AddCommand("evm.history", [&](vector<string> argList){
        uint8_t historyCount = atoi(argList[0].c_str());
        Log("Setting history count to ", historyCount);
        statsHistory_.SetCapacity(historyCount);
    }, { .argCount = 1, .help = "" });

    Shell::AddCommand("evm.t.verbose", [&](vector<string> argList){
        bool verbose = (bool)atoi(argList[0].c_str());
        SetTimelineVerbose(verbose);
    }, { .argCount = 1, .help = "set whether timeline includes detailed events" });

    Shell::AddCommand("evm.stack", [&](vector<string> argList){
        Log("Stack Depth: ", mainLoopStackDepth_);
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("evm.stats", [&](vector<string> argList){
        DumpStats();
        LogNL();
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("evm.statsnow", [&](vector<string> argList){
        LogModeSync();

        DumpStats();
        LogNL();

        LogModeAsync();
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("evm.timer.debug", [&](vector<string> argList){
        DebugTimer("evm.timer.debug");
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("evm.timer.debugNow", [&](vector<string> argList){
        LogModeSync();
        DebugTimer("evm.timer.debugnow");
        LogModeAsync();
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("evm.delay", [&](vector<string> argList){
        uint32_t ms = atol(argList[0].c_str());
        Log("Delaying for ", ms, " ms");

        QueueWork("evm.delay", [=]{
            PAL.Delay(ms);
            Log("Done");
        });
    }, { .argCount = 1, .help = "run delay in evm thread" });

    Shell::AddCommand("evm.delaybusy", [&](vector<string> argList){
        uint32_t ms = atol(argList[0].c_str());
        LogModeSync();
        Log("Delaying Busy for ", ms, " ms");
        LogModeAsync();

        QueueWork("evm.delay", [=]{
            PAL.DelayBusy(ms);
            Log("Done");
        });
    }, { .argCount = 1, .help = "run delay busy in evm thread" });

    Shell::AddCommand("evm.test.work", [&](vector<string> argList){
        QueueWork("evm.test.work", []{
            Log("evm.test.work handled");
        });
    }, { .argCount = 0, .help = "test submitting work to the evm queue" });

    Shell::AddCommand("evm.test.timer.ms", [&](vector<string> argList){
        static uint64_t timeStart;

        uint64_t ms = atoi(argList[0].c_str());

        Log("Setting timer for ", ms, " ms");
        timerTest2_.SetName("TIMER_EVM_TEST_TIMER_MS_CALLER");
        timerTest2_.SetCallback([=]{
            timerTest1_.SetName("TIMER_EVM_TEST_TIMER_MS");
            timerTest1_.SetCallback([=]{
                uint64_t timeNow = PAL.Micros();
                Log("evm.test.timer.ms handled - ", Commas((timeNow - timeStart) / 1'000), " ms, ", Commas(timeNow - timeStart), " us");
            });

            timeStart = PAL.Micros();
            timerTest1_.TimeoutInMs(ms);
        });
        timerTest2_.TimeoutInMs(0);
    }, { .argCount = 1, .help = "test submitting an <x> ms timer to get serviced" });

    Shell::AddCommand("evm.test.timer.us", [&](vector<string> argList){
        static uint64_t timeStart;

        uint64_t us = atoi(argList[0].c_str());

        Log("Setting timer for ", us, " us");
        timerTest2_.SetName("TIMER_EVM_TEST_TIMER_US_CALLER");
        timerTest2_.SetCallback([=]{
            timerTest1_.SetName("TIMER_EVM_TEST_TIMER_US");
            timerTest1_.SetCallback([=]{
                uint64_t timeNow = PAL.Micros();
                Log("evm.test.timer.us handled - ", Commas(timeNow - timeStart), " us");
            });

            timeStart = PAL.Micros();
            timerTest1_.TimeoutInUs(us);
        });
        timerTest2_.TimeoutInMs(0);
    }, { .argCount = 1, .help = "test submitting an <x> us timer to get serviced" });

    Shell::AddCommand("evm.timer.cancel", [&](vector<string> argList){
        Timer *timerSearch = (Timer *)atoi(argList[0].c_str());

        bool found = false;
        for (auto &timer : timerList_)
        {
            if (timer == timerSearch)
            {
                timer->Cancel();
                found = true;

                break;
            }
        }

        if (found) { Log("Timer ", timerSearch, " found and dereigistered"); }
        else       { Log("Timer ", timerSearch, " not found");               }
    }, { .argCount = 1, .help = "cancel timer <ptr>" });
}
