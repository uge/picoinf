#include "PAL.h"
#include "Log.h"
#include "Timeline.h"
#include "VersionStr.h"
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

void PlatformAbstractionLayer::SchedulerLock()
{
    vTaskSuspendAll();
}
void PlatformAbstractionLayer::SchedulerUnlock()
{
    xTaskResumeAll();
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

bool PlatformAbstractionLayer::IsPicoW()
{
    return isPicoW_;
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


// Processor fault handlers
// https://www.freertos.org/Debugging-Hard-Faults-On-Cortex-M-Microcontrollers.html
// https://forums.raspberrypi.com/viewtopic.php?t=335571
// https://interrupt.memfault.com/blog/cortex-m-hardfault-debug#registers-prior-to-exception
// https://www.freertos.org/Debugging-Hard-Faults-On-Cortex-M-Microcontrollers.html
extern "C"
{

#define HALT_IF_DEBUGGING()                              \
  do {                                                   \
    if ((*(volatile uint32_t *)0xE000EDF0) & (1 << 0)) { \
      __asm("bkpt 1");                                   \
    }                                                    \
} while (0)

typedef struct __attribute__((packed)) ContextStateFrame {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;  // link register
  uint32_t return_address;
  uint32_t xpsr;
} sContextStateFrame;


#if 0

// I'm going to ultimately get either the MSP or PSP (active stack pointer)
// into R0, such that it appears as the first argument to the fault
// handler.
//
// The original instructions don't work for ARMv6-M, so I'm winging in
// some hastily coded assembly.  I'm sure there's a better way, I don't care.
//
//
// R1 = 0b0100 for comparing to LR
//
// R1 = LR (link register, which tells us which SP to use)
#define HARDFAULT_HANDLING_ASM(_x)               \
    __asm volatile(                                \
        ".thumb \n" \
        ".syntax unified \n" \
        "movs r1, #4 \n" \
        "mov r2, lr \n" \
        "tst r2, r1 \n" \
        "bne is_msp \n" \
        "beq is_psp \n" \
        "is_msp: \n"    \
        "mrs r0, msp \n"    \
        "b do_exec \n" \
        "is_psp: \n"    \
        "mrs r0, psp \n"    \
        "b do_exec \n" \
        "do_exec: \n"  \
        "b my_fault_handler_c \n"   \
    )
#endif

__attribute__((optimize("O0")))
void my_fault_handler_c(sContextStateFrame *frame) {
    HALT_IF_DEBUGGING();

    // application interrupt and reset control register
    // volatile uint32_t aircr = *(volatile uint32_t *)0xE000ED0C;

    // configurable fault status register
    volatile uint32_t cfsr  = *(volatile uint32_t *)0xE000ED28;

    const uint32_t usage_fault_mask = 0xffff0000;

    bool nonUsageFault = (cfsr & ~usage_fault_mask);

    if (nonUsageFault)
    {
        Log("Non-usage fault");
    }
    else
    {
        Log("Usage fault");
    }

    const bool faultedFromException = ((frame->xpsr & 0xFF) != 0);

    Log("faulted from exception: ", faultedFromException);

    Log("CFSR: ", ToBin(cfsr));

    // faultmask
    // control
    // apsr
    // xpsr on the stateframe, return_address shows same as debugger?


}




void UsageFault_Handler()
{
    // HARDFAULT_HANDLING_ASM();
    LogNL(5);
    Log("UsageFault_Handler");
    LogNL(5);
}

void BusFault_Handler()
{
    // HARDFAULT_HANDLING_ASM();
    LogNL(5);
    Log("BusFault_Handler");
    LogNL(5);
}

void MemMang_Handler()
{
    // HARDFAULT_HANDLING_ASM();
    LogNL(5);
    Log("MemMang_Handler");
    LogNL(5);
}



void HardFault_Handler()
{
    // HARDFAULT_HANDLING_ASM();

    uint32_t lr = 0;
    uint32_t msp = 0;
    uint32_t psp = 0;

    __asm volatile (
        "mov %[retVal], lr"
        :
        [retVal] "=r" (lr)
    );

    __asm volatile (
        "mrs r0, msp \n"
        "mov %[retVal], r0"
        :
        [retVal] "=r" (msp)
    );

    __asm volatile (
        "mrs r0, psp \n"
        "mov %[retVal], r0"
        :
        [retVal] "=r" (psp)
    );

    LogModeSync();

    Log("lr : ", ToHex(lr));
    Log("msp: ", ToHex(msp));
    Log("psp: ", ToHex(psp));

    ContextStateFrame *frame = nullptr;
    if (lr & 0b0100)
    {
        Log("Thread mode");
        frame = (ContextStateFrame *)psp;
    }
    else
    {
        Log("Non-Thread mode");
        frame = (ContextStateFrame *)msp;
    }

    my_fault_handler_c(frame);



    LogNL(2);
    Log("==========================================");
    PAL.Reset();



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
    Log("Board       : ", PAL.IsPicoW() ? "pico_w" : "pico");
    Log("Version     : ", Version::GetVersion());
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
    }, { .argCount = 1, .help = "delay x ms" });

    Shell::AddCommand("pal.delaybusy", [](vector<string> argList){
        PAL.DelayBusy(atoi(argList[0].c_str()));
        Log("done");
    }, { .argCount = 1, .help = "delaybusy x ms" });

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
