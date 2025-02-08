#include "Container.h"
#include "Evm.h"
#include "KStats.h"
#include "Log.h"
#include "Shell.h"
#include "TimeClass.h"
#include "Timeline.h"
#include "Utl.h"

#include "FreeRTOS.Wrapped.h"
#include "task.h"

#include <map>
using namespace std;

#include "StrictMode.h"



// We're feeding a 32-bit microsecond value to the FreeRTOS stats
// capturing system.  This will wrap every 71 min.  Not a problem.
// We'll be capturing periodically and simply taking the diff
// between the capture points, the math will all work out.


static string ToString(eTaskState state)
{
    string retVal = "INVALID";

    if      (state == eReady)     { retVal = "READY";     }
    else if (state == eRunning)   { retVal = "RUNNING";   }
    else if (state == eBlocked)   { retVal = "BLOCKED";   }
    else if (state == eSuspended) { retVal = "SUSPENDED"; }
    else if (state == eDeleted)   { retVal = "DELETED";   }

    return retVal;
}



/////////////////////////////////////////////////////////////////////
// CPU Time Stats
//
// Each captured frame consists of the CPU time for all tasks in that
// period, for that period.
/////////////////////////////////////////////////////////////////////

struct KTaskCpuTime
{
    string   name;
    uint64_t runDuration;
    uint64_t totalRunDuration;
};

struct KTaskCpuTimeFrame
{
    uint64_t timeAtCapture;
    uint64_t duration;
    map<string, KTaskCpuTime> taskCpuTimeList;

    void Print() const
    {
        Log("Time at capture: ", Time::GetNotionalTimeAtSystemUs(timeAtCapture));
        Log("Duration       : ", Time::MakeDurationFromUs(duration));

        for (const auto &[name, taskCpuTime]: taskCpuTimeList)
        {
            string nameFormatted = FormatStr("%-15s", taskCpuTime.name.c_str());

            Log("  ", nameFormatted, " : ",
                StrUtl::PadLeft(Commas(taskCpuTime.runDuration), ' ', 9),
                " of total ",
                StrUtl::PadLeft(Commas(taskCpuTime.totalRunDuration), ' ', 15));
        }
    }
};

static CircularBuffer<KTaskCpuTimeFrame> frameList_;
static Timer timer_;


static void CaptureStats()
{
    // Get current tasks
    vector<KStats::KTaskStats> taskStatsList = KStats::GetTaskStats();

    // Start building new frame
    KTaskCpuTimeFrame frame;

    // time at capture is now
    frame.timeAtCapture = PAL.Micros();

    // duration covered is the time since the last if there was one
    uint64_t timeLast = 0;
    if (frameList_.Size())
    {
        timeLast = frameList_[frameList_.Size() - 1].timeAtCapture;
    }
    frame.duration = frame.timeAtCapture - timeLast;

    // get last frame if there was one
    KTaskCpuTimeFrame frameLast;
    if (frameList_.Size())
    {
        frameLast = frameList_[frameList_.Size() - 1];
    }

    // get delta of total runtime from last frame
    for (const auto &taskStats : taskStatsList)
    {
        // create new entry for this task
        KTaskCpuTime taskCpuTime;

        // fill out easy to know fields
        taskCpuTime.name             = taskStats.name;
        taskCpuTime.totalRunDuration = taskStats.totalRunDuration;

        // if we have a prior entry for this task, use it to determine
        // the delta in run time
        if (frameLast.taskCpuTimeList.contains(taskStats.name))
        {
            const auto &taskCpuTimeLast = frameLast.taskCpuTimeList[taskStats.name];

            taskCpuTime.runDuration = taskCpuTime.totalRunDuration - taskCpuTimeLast.totalRunDuration;
        }
        else
        {
            taskCpuTime.runDuration = taskCpuTime.totalRunDuration;
        }

        // add it
        frame.taskCpuTimeList[taskCpuTime.name] = taskCpuTime;
    }

    frameList_.PushBack(frame);
}

static void DumpStats()
{
    Log(frameList_.Size(), " historical Task CPU Stats");

    for (uint32_t i = 0; i < frameList_.Size(); ++i)
    {
        const auto &frame_ = frameList_[i];

        LogNL();
        frame_.Print();
    }
}


/////////////////////////////////////////////////////////////////////
// Interface
/////////////////////////////////////////////////////////////////////

void KStats::Init()
{
    Timeline::Global().Event("KStats::Init");

    static const uint8_t HISTORY_COUNT = 5;
    frameList_.SetCapacity(HISTORY_COUNT);

    static const uint64_t STATS_INTERVAL_MS = 5'000;
    timer_.SetCallback([]{
        CaptureStats();
    });

    timer_.SetSnapToMs(STATS_INTERVAL_MS);
    timer_.TimeoutIntervalMs(STATS_INTERVAL_MS);
}

void KStats::SetupShell()
{
    Timeline::Global().Event("KStats::SetupShell");

    Shell::AddCommand("k.tasks", [](vector<string>){
        vector<KTaskStats> ktsList = GetTaskStats();

        Log(ktsList.size(), " Kernel Tasks");
        for (const auto &kts : ktsList)
        {
            LogNL();
            kts.Print();
        }
    }, { .argCount = 0, .help = "Get Kernel Tasks" });

    Shell::AddCommand("k.stats", [](vector<string>){
        DumpStats();
    }, { .argCount = 0, .help = "Get Kernel Tasks" });
}

vector<KStats::KTaskStats> KStats::GetTaskStats()
{
    vector<KTaskStats> retVal;

    // get count of current tasks
    uint32_t taskCount = uxTaskGetNumberOfTasks();

    // ensure we have a contiguous array of structures to have FreeRTOS
    // fill in
    vector<TaskStatus_t> taskStatusList;
    taskStatusList.resize(taskCount);

    // get state
    uint32_t timeAtCapture = 0;
    uint32_t tasksCaptured =
        uxTaskGetSystemState(taskStatusList.data(),
                             taskStatusList.size(),
                             &timeAtCapture);

    // zero if failure
    if (tasksCaptured)
    {
        for (const auto &ts : taskStatusList)
        {
            KTaskStats s;

            s.name             = ts.pcTaskName;
            s.number           = ts.xTaskNumber;
            s.state            = ToString(ts.eCurrentState);
            s.currentPriority  = ts.uxCurrentPriority;
            s.basePriority     = ts.uxBasePriority;
            s.totalRunDuration = ts.ulRunTimeCounter;
            s.stackBase        = ts.pxStackBase;
            s.stackTop         = ts.pxTopOfStack;
            s.stackEnd         = ts.pxEndOfStack;
            s.highWaterMark    = ts.usStackHighWaterMark * sizeof(configSTACK_DEPTH_TYPE);

            retVal.push_back(s);
        }
    }
    else
    {
        Log("GetTaskStats Err: Failed to capture");
    }

    return retVal;
}


void KStats::KTaskStats::Print() const
{
    Log("Task");
    Log("-----------------------------------------");
    Log("name            : ", name);
    Log("number          : ", number);
    Log("state           : ", state);
    Log("currentPriority : ", currentPriority);
    Log("basePriority    : ", basePriority);
    Log("totalRunDuration: ", Commas(totalRunDuration));
    Log("stackEnd        : ", ToHex((uint32_t)stackEnd));
    Log("stackTop        : ", ToHex((uint32_t)stackTop));
    Log("stackBase       : ", ToHex((uint32_t)stackBase));
    Log("stackSize       : ", Commas((uint32_t)stackEnd - (uint32_t)stackBase));
    Log("  stackUsed     : ", Commas((uint32_t)stackEnd - (uint32_t)stackTop));
    Log("  stackRem      : ", Commas((uint32_t)stackTop - (uint32_t)stackBase));
    Log("  stackUsedMax  : ", Commas((uint32_t)stackEnd - (uint32_t)stackBase - (uint32_t)(highWaterMark)));
    Log("  stackRemMin   : ", Commas((uint32_t)(highWaterMark)));
}
