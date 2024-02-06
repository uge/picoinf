#include <list>
using namespace std;

#include "Work.h"
#include "Log.h"
#include "PAL.h"
#include "KMessagePassing.h"
#include "HeapAllocators.h"
#include "Timeline.h"
#include "Shell.h"


static const uint32_t COUNT_LIMIT = 5;

static KSemaphore sem_(0, COUNT_LIMIT);


struct WorkData
{
    function<void()> fn;
    const char *label = nullptr;
};

static list<
    WorkData,
    IsrPoolHeapAllocator<WorkData, COUNT_LIMIT>
> workList_;


static Timeline timeline_;
static uint32_t jobCount_;
static uint64_t durationUs_;

void Work::Report()
{
    Queue("Work::Report", [&](){
        Log("Jobs Run: ", jobCount_);
        Log("Duration: ", Commas(durationUs_ / 1000).c_str(), " ms, ", Commas(durationUs_), " us");
        timeline_.Report("work");
    });
}


void Work::Queue(const char *label, function<void()> &&fn)
{
    IrqLock lock;

    workList_.emplace_back(WorkData{
        .fn    = fn,
        .label = label,
    });
    sem_.Give();
}




////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////

int WorkSetupShell()
{
    Timeline::Global().Event("WorkSetupShell");

    Shell::AddCommand("work.report", [&](vector<string> argList){
        Work::Report();
    }, { .argCount = 0, .help = "Work Report" });

    Shell::AddCommand("work.test", [&](vector<string> argList){
        Work::Queue("work.test", []{
            Log("work.test");
        });
    }, { .argCount = 0, .help = "" });

    return 1;
}


#include <zephyr/init.h>
SYS_INIT(WorkSetupShell, APPLICATION, 80);



////////////////////////////////////////////////////////////////////////////////
// Thread
////////////////////////////////////////////////////////////////////////////////

static void ThreadFnWork()
{
    while (true)
    {
        // block on this
        sem_.Take(K_FOREVER);

        // extract data safely
        unsigned int key = PAL.IrqLock();

        auto wd = workList_.front();
        workList_.pop_front();

        PAL.IrqUnlock(key);

        // make some labels
        const char *preLabel  = wd.label ? wd.label : "pre-work";
        const char *postLabel = wd.label ? wd.label : "post-work";
        
        // execute and update stats
        timeline_.Event(preLabel);
        uint64_t timeStart = PAL.Micros();
        wd.fn();
        uint64_t timeEnd = PAL.Micros();
        timeline_.Event(postLabel);

        durationUs_ += timeEnd - timeStart;
        ++jobCount_;
    }
}

static const uint32_t STACKSIZE = 1024;
static const uint32_t PRIORITY  = 7;

K_THREAD_DEFINE(ThreadWork, STACKSIZE, ThreadFnWork, NULL, NULL, NULL, PRIORITY, 0, 0);

