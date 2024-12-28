#pragma once

#include <zephyr/device.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/pm/pm.h>


extern "C" {

extern const pm_state_info *pm_policy_next_state(uint8_t cpu, int32_t ticks);
extern void pm_state_set(pm_state state, uint8_t substate_id);
extern void pm_state_exit_post_ops(pm_state state, uint8_t substate_id);

}

class PowerManagerSoC
{
public:

    using CbOnPickNextPowerState = pm_state_info *(*)(uint8_t cpu, int32_t ticks);
    using CbOnApplyPowerState    = void(*)(pm_state state, uint8_t substate_id);
    using CbOnUnApplyPowerState  = void(*)(pm_state state, uint8_t substate_id);

public:
    static void SetCallbackOnPickNextPowerState(CbOnPickNextPowerState cbOnPickNextPowerState);
    static void SetCallbackOnApplyPowerState(CbOnApplyPowerState cbOnApplyPowerState);
    static void SetCallbackOnUnApplyPowerState(CbOnUnApplyPowerState cbOnUnApplyPowerState);

    inline static CbOnPickNextPowerState cbOnPickNextPowerState_ = nullptr;
    inline static CbOnApplyPowerState    cbOnApplyPowerState_    = nullptr;
    inline static CbOnUnApplyPowerState  cbOnUnApplyPowerState_  = nullptr;
};

