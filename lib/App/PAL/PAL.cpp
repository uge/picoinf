#include "PAL.h"
#include "Log.h"
#include "Timeline.h"
#include "WDT.h"

#include "pico/time.h"
#include "pico/unique_id.h"

#include "FreeRTOS.h"
#include "task.h"

#include <vector>
#include <string>
using namespace std;



string PlatformAbstractionLayer::GetAddress()
{
    uint8_t BUF_SIZE = (2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES) + 1;
    char buf[BUF_SIZE];

    pico_get_unique_board_id_string(buf, BUF_SIZE);

    string retVal;
    retVal += "0x";
    retVal += buf;

    return retVal;
}

uint64_t PlatformAbstractionLayer::Millis()
{
    return Micros() / 1000;
}

uint64_t PlatformAbstractionLayer::Micros()
{
    return time_us_64();
}

void PlatformAbstractionLayer::Delay(uint64_t ms)
{
    // thread gets descheduled
    DelayUs(ms * 1000);
}

void PlatformAbstractionLayer::DelayUs(uint64_t us)
{
    // thread gets descheduled
    sleep_us(us);
}

void PlatformAbstractionLayer::DelayBusy(uint64_t ms)
{
    // thread doesn't get descheduled
    DelayBusyUs(ms * 1000);
}

void PlatformAbstractionLayer::DelayBusyUs(uint64_t us)
{
    // thread doesn't get descheduled
    busy_wait_us(us);
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

// These two functions are being left in place but muted.
// Their function was originally to indicate that the running thread should
// not be interrupted by another thread, given the operations in question.
// However, because this works differently (and inconveniently) in FreeRTOS,
// I will simply be ensuring threads are prioritized equally where needed
// and that the scheduler is in cooperative mode and not preemptive mode.
// These functions will be left in the code so in the future I can more
// easily find areas affected by a potential switch to SMP where more than
// one core can be running code at a time and that this problem would
// re-arise.
void PlatformAbstractionLayer::SchedulerLock()
{
    // vTaskSuspendAll();
}
void PlatformAbstractionLayer::SchedulerUnlock()
{
    // xTaskResumeAll();
}

void PlatformAbstractionLayer::YieldToAll()
{
    taskYIELD();
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

// https://forums.raspberrypi.com/viewtopic.php?t=318747
void PlatformAbstractionLayer::Reset()
{
    volatile uint32_t *AIRCR_Register = ((volatile uint32_t*)(PPB_BASE + 0x0ED0C));

    *AIRCR_Register = 0x5FA0004;
}

#include "pico/bootrom.h"
// https://www.raspberrypi.com/documentation/pico-sdk/runtime.html#function-documentation34
void PlatformAbstractionLayer::ResetToBootloader()
{
    reset_usb_boot(0, 0);
}

string PlatformAbstractionLayer::GetResetReason()
{
    return resetReason_;
}

// #include <hardware/flash.h>
#include <hardware/structs/vreg_and_chip_reset.h>

void PlatformAbstractionLayer::CaptureResetReasonAndClear()
{
    // https://github.com/zephyrproject-rtos/zephyr/pull/42558/commits/ee98c4e62458473b9953d83ccc0083ba47804f0c
    // at time of writing, rpi_pico only supported
    // RESET_POR
    // RESET_PIN
    // RESET_DEBUG
    //
    // Not very granular...

    resetReason_ = "";
    string sep = "";

	uint32_t reset_register = vreg_and_chip_reset_hw->chip_reset;

	if (reset_register & VREG_AND_CHIP_RESET_CHIP_RESET_HAD_POR_BITS)
    {
        resetReason_ += sep;
        resetReason_ += "POWER_OR_BROWNOUT";
        sep = ", ";
	}

	if (reset_register & VREG_AND_CHIP_RESET_CHIP_RESET_HAD_RUN_BITS)
    {
        resetReason_ += sep;
        resetReason_ += "RESET_PIN";
        sep = ", ";
	}

	if (reset_register & VREG_AND_CHIP_RESET_CHIP_RESET_HAD_PSM_RESTART_BITS)
    {
        resetReason_ += sep;
        resetReason_ += "DEBUG_RESET";
        sep = ", ";
	}

    if (Watchdog::CausedReboot())
    {
        resetReason_ += sep;
        resetReason_ += "WATCHDOG";
        sep = ", ";
    }

    if (resetReason_ == "")
    {
        resetReason_ = "UNKNOWN";
    }
}

double PlatformAbstractionLayer::MeasureVCC()
{
    // TODO
    // return RP2040::MeasureVCC();
    return 0;
}



PlatformAbstractionLayer PAL;


// Called when something bad has happened.
// The default implementation of this function halts the system unconditionally.
// I'm overriding it because why would I want to just hang?  Restart!

// stop hanging in the libc-hooks.c when, for example, and empty
// function<> is called.
extern "C" {
void _exit(int status)
{
    LogModeSync();

    Log("_exit(", status, ") from libc-hooks.c");

    PAL.Fatal("_exit");

    while (true) {}
}
}


// ISR hard fault handler
// https://forums.raspberrypi.com/viewtopic.php?t=335571
// https://forums.raspberrypi.com/viewtopic.php?t=335571
extern "C"
{
void HardFault_Handler()
{
    LogModeSync();

    Log("HardFault_Handler");

    PAL.Fatal("HardFault_Handler");

    while (true) {}
}
}



////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////

#include "Shell.h"
void PALInit()
{
    Timeline::Global().Event("PALInit");

    LogNL(5);  // Give some visual space from prior run
    Log("----------------------------------------");

    PAL.CaptureResetReasonAndClear();

    LogNL();
    Log("Reset reason: ", PAL.GetResetReason());
    Log("Device ID   : ", PAL.GetAddress());
    LogNL();
}

void PALSetupShell()
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
}

#include "JSONMsgRouter.h"
void PALSetupJSON()
{
    Timeline::Global().Event("PALSetupJSON");

    JSONMsgRouter::RegisterHandler("REQ_SYS_RESET", [](auto &in, auto &out){
        Shell::Eval("sys.reset");
    });
}
