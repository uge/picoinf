#include "PowerManagerSoC.h"


/////////////////////////////////////////////////////////////////////
//
// Power Management integration
// - Act as the interface between the kernel idle power reduction
//   system and the actual SoC implementation details to achieve
//   the power reduction
//
// https://docs.zephyrproject.org/latest/services/pm/system.html
// https://docs.zephyrproject.org/latest/services/pm/api/index.html
//
// The kernel enters the idle state when it has nothing to schedule.
// If enabled via the CONFIG_PM Kconfig option,
// the Power Management Subsystem can put an idle system in one of
// the supported power states, based on the selected power management
// policy and the duration of the idle time allotted by the kernel
//
// Supported states:
// - PM_STATE_ACTIVE
// - PM_STATE_RUNTIME_IDLE
// - PM_STATE_SUSPEND_TO_IDLE
// - PM_STATE_STANDBY
// - PM_STATE_SUSPEND_TO_RAM
// - PM_STATE_SUSPEND_TO_DISK
// - PM_STATE_SOFT_OFF
//
// It is an application responsibility to set up a wake up event.
// (typically an interrupt)
//
// Once a state is decided upon, the state is distributed
// to all listeners, both pm and runtime pm handlers.
//
// These functions are discovered by being compiled in and exposed
// via extern.
//
/////////////////////////////////////////////////////////////////////


#ifdef CONFIG_PM_POLICY_CUSTOM
// Power State Decider
// - the kernel is idle, what power state do you want to go into?
const struct pm_state_info *pm_policy_next_state(uint8_t cpu, int32_t ticks)
{
    static pm_state_info info;

    info.state = PM_STATE_ACTIVE;

    pm_state_info *retVal = &info;

    if (PowerManagerSoC::cbOnPickNextPowerState_)
    {
        retVal = PowerManagerSoC::cbOnPickNextPowerState_(cpu, ticks);
    }

    return retVal;
}
#endif

// Power State Doer
// - Put the processor into the given power state
// 
// This function implements the SoC specific details necessary to
// put the processor into available power states
void pm_state_set(enum pm_state state, uint8_t substate_id)
{
    if (PowerManagerSoC::cbOnApplyPowerState_)
    {
        PowerManagerSoC::cbOnApplyPowerState_(state, substate_id);
    }
}


// Power State Undoer
// - Do any SoC or architecture-specific post-ops after sleep state exits
// 
// This function is a place holder to do any operations that may be needed
// to be done after sleep state exits.
void pm_state_exit_post_ops(enum pm_state state, uint8_t substate_id)
{
    if (PowerManagerSoC::cbOnUnApplyPowerState_)
    {
        PowerManagerSoC::cbOnUnApplyPowerState_(state, substate_id);
    }

    // must do this, kernel doesn't do it for you
	irq_unlock(0);
}



/////////////////////////////////////////////////////////////////////
// Public interface
/////////////////////////////////////////////////////////////////////

void PowerManagerSoC::SetCallbackOnPickNextPowerState(CbOnPickNextPowerState cbOnPickNextPowerState)
{
    cbOnPickNextPowerState_ = cbOnPickNextPowerState;
}

void PowerManagerSoC::SetCallbackOnApplyPowerState(CbOnApplyPowerState cbOnApplyPowerState)
{
    cbOnApplyPowerState_ = cbOnApplyPowerState;
}

void PowerManagerSoC::SetCallbackOnUnApplyPowerState(CbOnUnApplyPowerState cbOnUnApplyPowerState)
{
    cbOnUnApplyPowerState_ = cbOnUnApplyPowerState;
}
