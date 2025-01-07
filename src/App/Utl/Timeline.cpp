#include "PAL.h"
#include "Shell.h"
#include "Timeline.h"
#include "Utl.h"
#include "Work.h"

#include <cstring>
using namespace std;

#include "StrictMode.h"


Timeline::Timeline(const char *str)
{
    IrqLock lock;

    if (!strcmp(str, "GLOBAL"))
    {
        iAmTheGlobal_ = true;
    }

    Reset();
}

void Timeline::SetMaxEvents(uint32_t maxEvents)
{
    IrqLock lock;
    
    eventList_.SetCapacity(maxEvents);
}

uint32_t Timeline::GetMaxEvents()
{
    return eventList_.GetCapacity();
}

void Timeline::KeepOldest()
{
    keepOldest_ = true;
}

void Timeline::KeepNewest()
{
    keepOldest_ = false;
}

uint64_t Timeline::Event(const char *name)
{
    name = name ? name : "NULLPTR";

    uint64_t timeUs = PAL.Micros();
    
    if (ccGlobal_ && !iAmTheGlobal_)
    {
        Global().Event(name);
    }

    if (!currentlyReporting_)
    {
        IrqLock lock;
        
        // We want to add another element.
        // That will either be possible or not.
        // If we're keeping the oldest, and we're at our limit, we don't add.

        bool weAreAtOurLimit = eventList_.Size() == eventList_.GetCapacity();
        bool addItem = true;

        if (weAreAtOurLimit)
        {
            if (keepOldest_)
            {
                addItem = false;
            }
        }

        if (addItem)
        {
            eventList_.PushBack((EventData){
                .name = name,
                .timeUs = timeUs,
            });
        }
        else
        {
            ++eventsLost_;
        }
    }
    else
    {
        ++eventsLost_;
    }

    return timeUs;
}

void Timeline::Report(const char *title)
{
    Work::Queue(title, [=, this](){
        ReportNow(title);
    });
}

void Timeline::ReportNow(const char *title)
{
    currentlyReporting_ = true;
    
    LogNL();
    if (title)
    {
        if (iAmTheGlobal_)
        {
            LogNNL("[Timeline (global) ", title, ": ");
        }
        else
        {
            LogNNL("[Timeline ", title, ": ");
        }
    }
    else
    {
        if (iAmTheGlobal_)
        {
            LogNNL("[Timeline (global): ");
        }
        else
        {
            LogNNL("[Timeline: ");
        }
    }
    Log(eventList_.Size(), " Events (", CommasStatic(eventsLost_), " lost)]");
    if (eventList_.Size())
    {
        // find lengths
        size_t len = 0;
        for (uint32_t i = 0; i < eventList_.Size(); ++i)
        {
            auto &evt = eventList_[i];

            if (strlen(evt.name) > len)
            {
                len = strlen(evt.name);
            }
        }

        size_t lenMs = 0;
        size_t lenUs = 0;
        {
            // walk the list
            uint32_t idxLast = 0;
            uint32_t idxThis = 1;
            while (idxThis != eventList_.Size())
            {
                auto &evtLast = eventList_[idxLast];
                auto &evtThis = eventList_[idxThis];

                uint64_t diffUs = evtThis.timeUs - evtLast.timeUs;
                uint64_t diffMs = diffUs / 1000;

                size_t lenDiffMs = strlen(CommasStatic(diffMs).c_str());
                if (lenDiffMs > lenMs)
                {
                    lenMs = lenDiffMs;
                }

                size_t lenDiffUs = strlen(CommasStatic(diffUs).c_str());
                if (lenDiffUs > lenUs)
                {
                    lenUs = lenDiffUs;
                }

                ++idxLast;
                ++idxThis;
            }
        }

        // walk the list
        uint32_t idxLast = 0;
        uint32_t idxThis = 1;
        while (idxThis < eventList_.Size())
        {
            auto &evtLast = eventList_[idxLast];
            auto &evtThis = eventList_[idxThis];

            uint64_t diffUs = evtThis.timeUs - evtLast.timeUs;
            uint64_t diffMs = diffUs / 1000;

            // print it
            const uint8_t BUF_SIZE = 50;
            char formatStr[BUF_SIZE];
            char completedStr[BUF_SIZE];

            auto fnPrint = [&](const char *fmt, size_t len, const char *val) {
                snprintf(formatStr,    BUF_SIZE, fmt, len);
                snprintf(completedStr, BUF_SIZE, formatStr, val);
                LogNNL(completedStr);
            };

            fnPrint("%%%us - ",    len,   evtLast.name);
            fnPrint("%%%us: ",     len,   evtThis.name);
            fnPrint("%%%us ms, ",  lenMs, CommasStatic(diffMs).c_str());
            fnPrint("%%%us us - ", lenUs, CommasStatic(diffUs).c_str());
            fnPrint("%%12s\n",     12,    TimestampFromUs(evtThis.timeUs));

            ++idxLast;
            ++idxThis;
        }

        if (eventList_.Size() >= 2)
        {
            uint64_t totalUs = eventList_[eventList_.Size() - 1].timeUs - eventList_[0].timeUs;
            uint64_t totalMs = (uint64_t)round((double)totalUs / 1'000.0);

            LogNNL("[Total Duration: ");
            LogNNL(CommasStatic(totalMs));
            LogNNL(" ms, ");
            LogNNL(CommasStatic(totalUs));
            LogNNL(" us]");
            LogNL();
        }
    }
    LogNL();

    currentlyReporting_ = false;
}

void Timeline::Reset()
{
    IrqLock lock;
    
    eventList_.Clear();
    eventsLost_ = 0;

    // record a reset event, but don't CC to the global.
    // accomplish this by pretending to be the global for
    // a short while, even when you actually are.
    bool iAmTheGlobalCache = iAmTheGlobal_;
    iAmTheGlobal_ = true;
    Event("[Reset]");
    iAmTheGlobal_ = iAmTheGlobalCache;
}

uint64_t Timeline::Use(function<void(Timeline &t)> fn, const char *title)
{
    Timeline t;

    uint64_t timeStart = t.Event("Start");
    fn(t);
    uint64_t timeEnd = t.Event("End");
    t.ReportNow(title);

    return timeEnd - timeStart;
}


void Timeline::EnableCcGlobal()
{
    ccGlobal_ = true;
}

void Timeline::DisableCcGlobal()
{
    ccGlobal_ = false;
}

Timeline &Timeline::Global()
{
    Timeline &retVal = timelineGlobal_;

    return retVal;
}


////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////

void Timeline::Init()
{
    Timeline::EnableCcGlobal();
    Timeline::Global().SetMaxEvents(80);
    Timeline::Global().Reset();

    Timeline::Global().Event("Timeline::Init");
}

void Timeline::SetupShell()
{
    Timeline::Global().Event("Timeline::SetupShell");

    Shell::AddCommand("t.max", [&](vector<string> argList){
        if (argList.empty())
        {
            Log("Max: ", Timeline::Global().GetMaxEvents());
        }
        else
        {
            Timeline::Global().SetMaxEvents((uint32_t)atoi(argList[0].c_str()));
        }
    }, { .argCount = -1, .help = "Global Timeline See/Set max events" });

    Shell::AddCommand("t.top", [&](vector<string> argList){
        Timeline::Global().KeepOldest();
    }, { .argCount = 0, .help = "Global Timeline Keep Oldest" });

    Shell::AddCommand("t.bot", [&](vector<string> argList){
        Timeline::Global().KeepNewest();
    }, { .argCount = 0, .help = "Global Timeline Keep Newest" });

    Shell::AddCommand("t.reset", [&](vector<string> argList){
        Timeline::Global().Reset();
    }, { .argCount = 0, .help = "Global Timeline Reset" });

    Shell::AddCommand("t.report", [&](vector<string> argList){
        Timeline::Global().Report();
    }, { .argCount = 0, .help = "Global Timeline Report" });

    Shell::AddCommand("t.reportnow", [&](vector<string> argList){
        Timeline::Global().ReportNow();
    }, { .argCount = 0, .help = "Global Timeline Report, Now" });
}

