#pragma once

#include "PAL.h"
#include "Log.h"

#include "PowerManagerSoC.h"
#include "PowerManagerRuntimeNotify.h"
#include "Utl.h"


class PowerManager
{
public:

    static void Test()
    {
        uint64_t timeStart;
        uint64_t timeEnd;
        while (true)
        {
            Log("Busy Start");
            timeStart = PAL.Micros();
            PAL.DelayBusy(5'000);
            timeEnd = PAL.Micros();
            Log("Busy from ", Commas(timeStart), " to ", Commas(timeEnd));
            Log("Busy took ", Commas(timeEnd - timeStart), " us");

            Log("Sleep Start");
            timeStart = PAL.Micros();
            PAL.Delay(5'000);
            timeEnd = PAL.Micros();
            Log("Sleep from ", Commas(timeStart), " to ", Commas(timeEnd));
            Log("Sleep took ", Commas(timeEnd - timeStart), " us");
        }
    }


public:
    static void Init();
    static void SetupShell();

private:
    static const char *StateStr(pm_state state);
    static const char *ActionStr(pm_device_action action);

    static pm_state_info *OnPickNextPowerState(uint8_t cpu, int32_t ticks);
    static void OnApplyPowerState(pm_state state, uint8_t substate_id);
    static void OnUnApplyPowerState(pm_state state, uint8_t substate_id);

    static void OnNotifyPowerStateEnter(pm_state state);
    static void OnNotifyPowerStateExit(pm_state state);

    static void OnRuntimeNotifyInit();
    static void OnRuntimeNotifyPmAction(pm_device_action action);
};






