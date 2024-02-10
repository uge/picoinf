#pragma once

#include <functional>
#include <string>
using namespace std;

#include "hardware/sync.h"


extern void PALInit();
extern void PALSetupShell();
extern void PALSetupJSON();

class PlatformAbstractionLayer
{
public:
    static string GetAddress();
    static std::string GetAddressHex();
    static std::string GetPart();
    static std::string GetPackageVariant();
    static std::string GetFullPartName();

    static uint64_t Millis();
    static uint64_t Micros();
    static void Delay(uint64_t ms);
    static void DelayUs(uint64_t us);
    static void DelayBusy(uint64_t ms);
    static void DelayBusyUs(uint64_t us);
    static void EnableForcedInIsrYes(bool force);
    inline __attribute__((always_inline))
    static bool InIsr()
    {
        return forceInIsrYes_ ? true : InIsrReal();
    }
    inline __attribute__((always_inline))
    static bool InIsrReal()
    {
        // taken from https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/7284d84dc88c5aaf2dc8337044177728b8bdae2d/portable/ThirdParty/GCC/RP2040/include/portmacro.h#L146
        uint32_t ulIPSR;
        __asm volatile ( "mrs %0, IPSR" : "=r" ( ulIPSR )::);
        return (uint8_t)ulIPSR > 0;
    }
    inline __attribute__((always_inline))
    static uint32_t IrqLock()
    {
        return save_and_disable_interrupts();
    }
    inline __attribute__((always_inline))
    static void IrqUnlock(uint32_t key)
    {
        restore_interrupts(key);
    }
    static void SchedulerLock();
    static void SchedulerUnlock();
    static void YieldToAll();
    static void RegisterOnFatalHandler(const char *title, std::function<void()> cbFnOnFatal);
    static void Fatal(const char *title);
    static void Reset();
    static void ResetToBootloader();
    static void CaptureResetReasonAndClear();
    static std::string GetResetReason();
    static double MeasureVCC();

private:
    inline static string resetReason_;

    inline static bool forceInIsrYes_ = false;

    struct FatalHandlerData
    {
        const char *title = nullptr;
        std::function<void()> cbFnOnFatal;
    };

    inline static std::vector<FatalHandlerData> fatalHandlerDataList_;
};

extern PlatformAbstractionLayer PAL;


class IrqLock
{
public:
    inline __attribute__((always_inline))
    IrqLock()
    {
        key_ = PlatformAbstractionLayer::IrqLock();
    }

    inline __attribute__((always_inline))
    ~IrqLock()
    {
        PlatformAbstractionLayer::IrqUnlock(key_);
    }

private:
    uint32_t key_;
};

