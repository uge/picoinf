#include "PowerManager.h"

#include "Log.h"
#include "Shell.h"
#include "Timeline.h"
#include "Utl.h"
#include "RP2040.h"

#include "PowerManagerNotify.h"



const char *PowerManager::StateStr(pm_state state)
{
    const char *retVal = "";

    switch (state)
    {
    case PM_STATE_ACTIVE:          retVal = "PM_STATE_ACTIVE";          break;
    case PM_STATE_RUNTIME_IDLE:    retVal = "PM_STATE_RUNTIME_IDLE";    break;
    case PM_STATE_SUSPEND_TO_IDLE: retVal = "PM_STATE_SUSPEND_TO_IDLE"; break;
    case PM_STATE_STANDBY:         retVal = "PM_STATE_STANDBY";         break;
    case PM_STATE_SUSPEND_TO_RAM:  retVal = "PM_STATE_SUSPEND_TO_RAM";  break;
    case PM_STATE_SUSPEND_TO_DISK: retVal = "PM_STATE_SUSPEND_TO_DISK"; break;
    case PM_STATE_SOFT_OFF:        retVal = "PM_STATE_SOFT_OFF";        break;
    default: break;
    }

    return retVal;
}


const char *PowerManager::ActionStr(pm_device_action action)
{
    const char *retVal = "";

    switch (action)
    {
    case PM_DEVICE_ACTION_SUSPEND:  retVal = "PM_DEVICE_ACTION_SUSPEND";  break;
    case PM_DEVICE_ACTION_RESUME:   retVal = "PM_DEVICE_ACTION_RESUME";   break;
    case PM_DEVICE_ACTION_TURN_OFF: retVal = "PM_DEVICE_ACTION_TURN_OFF"; break;
    case PM_DEVICE_ACTION_TURN_ON:  retVal = "PM_DEVICE_ACTION_TURN_ON";  break;
    default: break;
    }

    return retVal;
}




Timeline t;

static uint32_t sleepUs = 0;


pm_state_info *PowerManager::OnPickNextPowerState(uint8_t cpu, int32_t ticks)
{
    static pm_state_info info;

    t.Event("SoC Decide");

    info.state = PM_STATE_RUNTIME_IDLE;

    // how long do we have in us?
    uint32_t us = k_ticks_to_us_floor32(ticks);
    sleepUs = us;

    if (sleepUs < 1'000'000)
    {
        info.state = PM_STATE_ACTIVE;
    }

    LogModeSync();
    Log("PNPS ", Commas(us), "   ");

    return &info;
}

void PowerManager::OnApplyPowerState(pm_state state, uint8_t substate_id)
{
    t.Event("SoC Set");

    Log("B");
    RP2040::Clock::DeepSleep(sleepUs);
    Log("A");
}

void PowerManager::OnUnApplyPowerState(pm_state state, uint8_t substate_id)
{
    t.Event("SoC Unset");
    Log("U");
}








void PowerManager::OnNotifyPowerStateEnter(pm_state state)
{
    t.Event("Enter");
}

void PowerManager::OnNotifyPowerStateExit(pm_state state)
{
    t.Event("Exit");
}

void PowerManager::OnRuntimeNotifyInit()
{
    t.Event("RuntimeInit");
    Log("OnRuntimeNotifyInit");
}

void PowerManager::OnRuntimeNotifyPmAction(pm_device_action action)
{
    t.Event("RuntimePmAction");
    Log("OnRuntimeNotifyPmAction: ", ActionStr(action));
}




/////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////

void PowerManager::Init()
{
    Timeline::Global().Event("PowerManagerInit");

    PowerManagerSoC::SetCallbackOnPickNextPowerState(OnPickNextPowerState);
    PowerManagerSoC::SetCallbackOnApplyPowerState(OnApplyPowerState);
    PowerManagerSoC::SetCallbackOnUnApplyPowerState(OnUnApplyPowerState);

    PowerManagerNotify::SetCallbackOnPowerStateEnter(OnNotifyPowerStateEnter);
    PowerManagerNotify::SetCallbackOnPowerStateExit(OnNotifyPowerStateExit);
    PowerManagerNotify::Register();

    PowerManagerRuntimeNotify::SetCallbackOnInit(OnRuntimeNotifyInit);
    PowerManagerRuntimeNotify::SetCallbackOnPmAction(OnRuntimeNotifyPmAction);
}

void PowerManager::SetupShell()
{
    Timeline::Global().Event("PowerManagerSetupShell");

    Shell::AddCommand("pm.rt.open", [](vector<string> argList) {
        PowerManagerRuntimeNotify::Open();
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("pm.rt.close", [](vector<string> argList) {
        PowerManagerRuntimeNotify::Close();
    }, { .argCount = 0, .help = "" });



    Shell::AddCommand("pm.state", [](vector<string> argList) {
        // 0 = PM_STATE_ACTIVE,
        // 1 = PM_STATE_RUNTIME_IDLE,
        // 2 = PM_STATE_SUSPEND_TO_IDLE,
        // 3 = PM_STATE_STANDBY,
        // 4 = PM_STATE_SUSPEND_TO_RAM,
        // 5 = PM_STATE_SUSPEND_TO_DISK,
        // 6 = PM_STATE_SOFT_OFF,

        uint8_t state = atoi(argList[0].c_str());

        switch (state)
        {
        case PM_STATE_ACTIVE: Log("PM_STATE_ACTIVE"); break;
        case PM_STATE_RUNTIME_IDLE: Log("PM_STATE_RUNTIME_IDLE"); break;
        case PM_STATE_SUSPEND_TO_IDLE: Log("PM_STATE_SUSPEND_TO_IDLE"); break;
        case PM_STATE_STANDBY: Log("PM_STATE_STANDBY"); break;
        case PM_STATE_SUSPEND_TO_RAM: Log("PM_STATE_SUSPEND_TO_RAM"); break;
        case PM_STATE_SUSPEND_TO_DISK: Log("PM_STATE_SUSPEND_TO_DISK"); break;
        case PM_STATE_SOFT_OFF: Log("PM_STATE_SOFT_OFF"); break;
        }
    }, { .argCount = 1, .help = "reply with state <num>" });
}

int PowerManagerInit()
{
    PowerManager::Init();

    return 1;
}

int PowerManagerSetupShell()
{
    PowerManager::SetupShell();

    return 1;
}


#include <zephyr/init.h>
SYS_INIT(PowerManagerInit, APPLICATION, 50);
SYS_INIT(PowerManagerSetupShell, APPLICATION, 80);
