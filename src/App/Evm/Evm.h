#pragma once

#include <cstdint>
#include <set>
#include <list>
#include <functional>
using namespace std;

#include "PAL.h"
#include "Log.h"
#include "TimedEventHandler.h"
#include "Pin.h"
#include "HeapAllocators.h"
#include "KMessagePassing.h"
#include "Container.h"
#include "WDT.h"



class Evm
{
public:
    static void Init();
    static void SetupShell();

    //////////////////////////////////////////////////////////////////////
    // Evm Core
    //////////////////////////////////////////////////////////////////////

public:
    static void DisableAutoLogAsync();
    static void MainLoop();
    static void MainLoopRunFor(uint64_t durationMs);

private:
    inline static bool autoLogAsync_ = true;
    inline static bool mainLoopKeepRunning_ = true;

    static uint64_t GetTimeToNextTimedEvent();


    //////////////////////////////////////////////////////////////////////
    // Work Events (safe to use as Interrupt Events)
    //////////////////////////////////////////////////////////////////////

public:
    using FnWork = function<void()>;
private:
    struct WorkData
    {
        const char *label = nullptr;
        FnWork fnWork;
    };
    static const uint8_t MAX_WORK_ITEMS = 50;

public:
    static void QueueWork(const char *label, FnWork &fnWork);
    static void QueueWork(const char *label, FnWork &&fnWork);
    static void QueueLowPriorityWork(const char *label, FnWork &fnWork);
    static void QueueLowPriorityWork(const char *label, FnWork &&fnWork);
    static uint32_t ClearLowPriorityWorkByLabel(const char *label);

private:
    static void QueueWorkInternal(const char *label, FnWork &fnWork);
    static uint32_t ServiceWork();
    static void QueueLowPriorityWorkInternal(const char *label, FnWork &fnWork);
    static uint32_t ServiceLowPriorityWork();

private:
    static KMessagePipe<FnWork,   MAX_WORK_ITEMS> fnWorkList_;
    static KMessagePipe<WorkData, MAX_WORK_ITEMS> fnLowPriorityWorkList_;
    static KSemaphore sem_;


    //////////////////////////////////////////////////////////////////////
    // Timed Events
    //////////////////////////////////////////////////////////////////////

public:
    static bool RegisterTimedEventHandler(TimedEventHandler *teh, uint64_t timeout, uint64_t timeNow = PAL.Micros());
    static void DeRegisterTimedEventHandler(TimedEventHandler *teh);
    static bool IsRegisteredTimedEventHandler(TimedEventHandler *teh);
    static void DebugTimedEventHandler(const char *str, TimedEventHandler *obj = nullptr);
    static void TestTimedEventHandler();

private:
    static uint32_t ServiceTimedEventHandlers();


private:
    class CmpTimedEventHandler
    {
    public:
        // never let different objects compare equivalent.
        // meaning, if both set to expire at the same time, then
        // fine, but decide one is "less" than the other by looking
        // at the pointer value.  otherwise the insert/delete api
        // for multiset winds up operating on groups at a time
        // when all I want is individual object access.
        bool operator()(TimedEventHandler * const &teh1,
                        TimedEventHandler * const &teh2) const
        {
            bool retVal;

            if (teh1 == teh2)
            {
                retVal = false;
            }
            else
            {
                // get time in 64-bit signed.  we subtract the time queued from this
                // value, which will give the time passed since then, as a positive
                // number.
                uint64_t timeNow = PAL.Micros();

                uint64_t timeSinceSchedule1 = timeNow - teh1->timeQueued_;
                uint64_t timeSinceSchedule2 = timeNow - teh2->timeQueued_;

                // do time remaining calculations in 64-bit signed space to allow
                // for overdue timers to be a negative value.  This allows for
                // very large timeout periods, and ensures the possible difference
                // to fit fully within the negative and positive space in the signed
                // int.
                int64_t timeRemaining1 = (int64_t)teh1->timeout_ - timeSinceSchedule1;
                int64_t timeRemaining2 = (int64_t)teh2->timeout_ - timeSinceSchedule2;

                if (timeRemaining1 < timeRemaining2)
                {
                    retVal = true;
                }
                else if (timeRemaining1 == timeRemaining2)
                {
                    // in a scenario where two timers are both set to go off at the
                    // same time, we want the one which was scheduled first to be
                    // fired first.
                    // This is helpful in a scenario where there are zero-length
                    // timers, we don't want one to starve out another.
                    if (timeSinceSchedule1 < timeSinceSchedule2)
                    {
                        retVal = false;
                    }
                    else if (timeSinceSchedule1 == timeSinceSchedule2)
                    {
                        // ok, so they were scheduled at the same time, and have
                        // the same duration remaining.
                        // we know they're not the same object, so let pointer
                        // value decide
                        if (teh1 < teh2)
                        {
                            retVal = true;
                        }
                        else
                        {
                            retVal = false;
                        }
                    }
                    else    // timeSinceSchedule1 > timeSinceSchedule2
                    {
                        retVal = true;
                    }
                }
                else    // expiryOne > expiryTwo
                {
                    retVal = false;
                }
            }
            
            return retVal;
        }
    };
    static multiset<TimedEventHandler *, CmpTimedEventHandler> timedEventHandlerList_;


    //////////////////////////////////////////////////////////////////////
    // Observability
    //////////////////////////////////////////////////////////////////////
    
    static const uint32_t STATS_INTERVAL_MS = 5'000;
    static const uint32_t STATS_HISTORY_COUNT = 2;

    struct Stats
    {
        uint32_t HANDLED_WORK = 0;
        uint32_t HANDLED_TIMED = 0;

        uint32_t SKIPPED_SLEEP_FOR_WORK = 0;

        uint32_t TIME_IN_WORK = 0;
        uint32_t TIME_IN_TIMED = 0;
        uint32_t TIME_IN_SLEEP = 0;

        uint32_t COUNT_LATENT_WAKE = 0;
        uint32_t TIME_SUM_LATENT   = 0;

        uint32_t LOOPS = 0;
    };

    struct StatsSnapshot
    {
        uint64_t snapshotTime = 0;
        Stats stats;
    };

    static Stats stats_;
    static CircularBuffer<StatsSnapshot> statsHistory_;
    static TimedEventHandlerDelegate tedStats_;

    static void DumpStats();
    static Stats GetStatsDelta(Stats &s1, Stats &s2);
    static void DumpStats(Stats &stats, uint32_t duration);
    static const Stats &GetStats();

    static TimedEventHandlerDelegate tedWatchdog_;


    static TimedEventHandlerDelegate tedTest_;
    static TimedEventHandlerDelegate tedTest2_;
};
