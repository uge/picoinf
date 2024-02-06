#pragma once

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>

#include <functional>
#include <string>
using namespace std;


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
    static uint64_t Delay(uint64_t ms);
    static uint64_t DelayUs(uint64_t us);
    static void DelayBusy(uint64_t ms);
    static void DelayBusyUs(uint64_t us);
    static void EnableForcedInIsrYes(bool force);
    static bool InIsr();
    inline __attribute__((always_inline))
    static unsigned int IrqLock()
    {
        return irq_lock();
    }
    inline __attribute__((always_inline))
    static void IrqUnlock(unsigned int key)
    {
        irq_unlock(key);
    }
    static void SchedulerLock();
    static void SchedulerUnlock();
    static uint64_t YieldToAll();
    static k_tid_t GetThreadId();
    static void SetThreadPriority(int priority);
    static void WakeThread(k_tid_t id);
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
    unsigned int key_;
};


extern void k_sys_fatal_error_handler(unsigned int reason, const z_arch_esf_t *esf);
extern "C" {
extern void _exit(int);
}