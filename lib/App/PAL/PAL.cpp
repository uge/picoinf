#include "PAL.h"

#include <vector>
#include <string>
using namespace std;

#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/hwinfo.h>

#include "KTime.h"
#include "Log.h"
#include "Shell.h"
#include "Utl.h"
#include "RP2040.h"


string PlatformAbstractionLayer::GetAddress()
{
    string retVal = "0x0";

    const uint8_t BYTES = 32;
    array<uint8_t, BYTES + 1> arr;
    arr.fill(0);

    ssize_t len = hwinfo_get_device_id(arr.data(), BYTES);

    if (len > 0)
    {
        retVal = "0x";

        for (int i = 0; i < len; ++i)
        {
            retVal += Format::ToHex(arr[i], false);
        }
    }

    return retVal;
}

uint64_t PlatformAbstractionLayer::Millis()
{
    return Micros() / 1000;
}

uint64_t PlatformAbstractionLayer::Micros()
{
    // rpi pico 1us counter
    return RP2040::TimeUs64();
}

uint64_t PlatformAbstractionLayer::Delay(uint64_t ms)
{
    // thread gets descheduled
    return k_msleep(KTime{ms});
}

uint64_t PlatformAbstractionLayer::DelayUs(uint64_t us)
{
    // thread gets descheduled
    return k_usleep(KTime{us});
}

void PlatformAbstractionLayer::DelayBusy(uint64_t ms)
{
    // thread doesn't get descheduled
    DelayBusyUs(ms * 1000);
}

void PlatformAbstractionLayer::DelayBusyUs(uint64_t us)
{
    // thread doesn't get descheduled
    uint64_t timeNow = PAL.Micros();
    while (PAL.Micros() - timeNow < us)
    {
        // do nothing
    }
}

void PlatformAbstractionLayer::EnableForcedInIsrYes(bool force)
{
    static uint32_t nestLevel = 0;

    if (force)
    {
        ++nestLevel;
        
        forceInIsrYes_ = true;
    }
    else
    {
        // don't go below 0
        if (nestLevel)
        {
            --nestLevel;
        }

        // check if ok to re-enable
        if (nestLevel == 0)
        {
            forceInIsrYes_ = false;
        }
    }
}

bool PlatformAbstractionLayer::InIsr()
{
    return forceInIsrYes_ ? true : k_is_in_isr();
}

void PlatformAbstractionLayer::SchedulerLock()
{
    k_sched_lock();
}

void PlatformAbstractionLayer::SchedulerUnlock()
{
    k_sched_unlock();
}

// Not using k_yield(), it doesn't allow lower-priority threads
// to run, such as shell.
// Instead this microsleeps to force the scheduler to let
// all other threads run.
// The overhead is 150us.
uint64_t PlatformAbstractionLayer::YieldToAll()
{
    return DelayUs(1);
}

k_tid_t PlatformAbstractionLayer::GetThreadId()
{
    return k_current_get();
}

void PlatformAbstractionLayer::SetThreadPriority(int priority)
{
    k_thread_priority_set(GetThreadId(), priority);
}

void PlatformAbstractionLayer::WakeThread(k_tid_t id)
{
    k_wakeup(id);
}

void PlatformAbstractionLayer::RegisterOnFatalHandler(const char *title, function<void()> cbFnOnFatal)
{
    fatalHandlerDataList_.emplace_back(FatalHandlerData{
        .title = title,
        .cbFnOnFatal = cbFnOnFatal,
    });
}

void PlatformAbstractionLayer::Fatal(const char *title)
{
    PAL.EnableForcedInIsrYes(true);

    LogNL();
    LogNL();
    Log("Fatal error from ", title);
    Log("Logging, then Resetting");

    // First the global timeline
    Timeline::Global().ReportNow("Fatal Error");

    // Show how many handlers will get called
    size_t size = fatalHandlerDataList_.size();
    Log(size, " handlers to be called:");
    for (size_t i = 0; i < size; ++i)
    {
        FatalHandlerData &fh = fatalHandlerDataList_[i];

        Log("  Handler ", i + 1, ": ", fh.title);
    }

    LogNL();
    Log("Calling handlers");
    LogNL();

    // Call the handlers

    for (size_t i = 0; i < size; ++i)
    {
        FatalHandlerData &fh = fatalHandlerDataList_[i];

        Log("Handler ", i + 1, ": ", fh.title);
        LogNL();

        fh.cbFnOnFatal();
    }

    // See you next time
    Log("Resetting");
    PAL.Reset();
}

void PlatformAbstractionLayer::Reset()
{
    // notably, the ROSC being on is required otherwise this hangs
    sys_reboot(SYS_REBOOT_COLD);
}

void PlatformAbstractionLayer::ResetToBootloader()
{
    // NVIC_SystemReset();
}

string PlatformAbstractionLayer::GetResetReason()
{
    return resetReason_;
}

void PlatformAbstractionLayer::CaptureResetReasonAndClear()
{
    resetReason_ = "UNKNOWN";

    // https://github.com/zephyrproject-rtos/zephyr/pull/42558/commits/ee98c4e62458473b9953d83ccc0083ba47804f0c
    // at time of writing, rpi_pico only supported
    // RESET_POR
    // RESET_PIN
    // RESET_DEBUG
    //
    // Not very granular...

    unordered_map<uint32_t, const char *> reasonMap = {
        { RESET_PIN,            "RESET_PIN"            },
        { RESET_SOFTWARE,       "RESET_SOFTWARE"       },
        { RESET_BROWNOUT,       "RESET_BROWNOUT"       },
        { RESET_POR,            "RESET_POR"            },
        { RESET_WATCHDOG,       "RESET_WATCHDOG"       },
        { RESET_DEBUG,          "RESET_DEBUG"          },
        { RESET_SECURITY,       "RESET_SECURITY"       },
        { RESET_LOW_POWER_WAKE, "RESET_LOW_POWER_WAKE" },
        { RESET_CPU_LOCKUP,     "RESET_CPU_LOCKUP"     },
        { RESET_PARITY,         "RESET_PARITY"         },
        { RESET_PLL,            "RESET_PLL"            },
        { RESET_CLOCK,          "RESET_CLOCK"          },
        { RESET_HARDWARE,       "RESET_HARDWARE"       },
        { RESET_USER,           "RESET_USER"           },
        { RESET_TEMPERATURE,    "RESET_TEMPERATURE"    },
    };

    uint32_t cause;
    int res = hwinfo_get_reset_cause(&cause);

    if (res == 0)
    {
        if (reasonMap.contains(cause))
        {
            resetReason_ = reasonMap[cause];
        }
    }

    // hwinfo_clear_reset_cause();
}

double PlatformAbstractionLayer::MeasureVCC()
{
    return RP2040::MeasureVCC();
}



PlatformAbstractionLayer PAL;


// Called when something bad has happened.
// The default implementation of this function halts the system unconditionally.
// I'm overriding it because why would I want to just hang?  Restart!

void k_sys_fatal_error_handler(unsigned int reason, const z_arch_esf_t *esf)
{
    PAL.Fatal("k_sys_fatal_error_handler");
}


// stop hanging in the libc-hooks.c when, for example, and empty
// function<> is called.
extern "C" {
void _exit(int status)
{
    Log("_exit(", status, ") from libc-hooks.c");
}
}

// not implementing the following two, they are called instead of the more
// useful zephyr error dumping.
//
// extern void assert_post_action(void);
// extern void assert_post_action(const char *file, unsigned int line);





////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////


int PALInit()
{
    Timeline::Global().Event("PALInit");

    LogNL(5);  // Give some visual space from prior run
    Log("----------------------------------------");

    PAL.CaptureResetReasonAndClear();

    LogNL();
    Log("Reset reason: ", PAL.GetResetReason());
    Log("Device ID   : ", PAL.GetAddress());
    LogNL();

    PAL.SetThreadPriority(-1);

    return 1;
}

int PALSetupShell()
{
    Timeline::Global().Event("PALSetupShell");

    Shell::AddCommand("sys.reset", [](vector<string>){
        PAL.Reset();
    }, { .help = "reset board" });

    Shell::AddCommand("sys.bootloader", [](vector<string>){
        PAL.ResetToBootloader();
    }, { .help = "reset board to bootloader" });

    Shell::AddCommand("sys.crash", [](vector<string>){
        function<void()> fn;
        fn();
    }, { .help = "crash board" });

    Shell::AddCommand("sys.time", [](vector<string>){
        uint64_t time = PAL.Micros();
        Log(Commas(time), " - ", TimestampFromUs(time));
    }, { .help = "time" });

    Shell::AddCommand("pal.delay", [](vector<string> argList){
        PAL.Delay(atoi(argList[0].c_str()));
        Log("done");
    }, { .argCount = 1, .help = "delay x ms", .executeAsync = true });

    Shell::AddCommand("pal.delaybusy", [](vector<string> argList){
        PAL.DelayBusy(atoi(argList[0].c_str()));
        Log("done");
    }, { .argCount = 1, .help = "delaybusy x ms", .executeAsync = true });

    Shell::AddCommand("pal.test.fatal", [](vector<string> argList){
        PAL.Fatal("pal.test.fatal");
    }, { .argCount = 0, .help = "" });

    return 1;
}

#include "JSONMsgRouter.h"
int PALSetupJSON()
{
    Timeline::Global().Event("PALSetupJSON");

    JSONMsgRouter::RegisterHandler("REQ_SYS_RESET", [](auto &in, auto &out){
        Shell::Eval("sys.reset");
    });

    return 1;
}


#include <zephyr/init.h>
SYS_INIT(PALInit, APPLICATION, 13);
SYS_INIT(PALSetupShell, APPLICATION, 80);
SYS_INIT(PALSetupJSON, APPLICATION, 80);
