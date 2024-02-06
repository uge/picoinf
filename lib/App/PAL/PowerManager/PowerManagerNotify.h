#pragma once


#include <zephyr/device.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/pm/pm.h>

#include "Log.h"

/////////////////////////////////////////////////////////////////////
//
// Power Manager API user - PM State Notification Callbacks
//
// This interface is for parts of the system which use the
// non-runtime PM interface
// (they only get told the state to change into, and don't
//  pre-emptively go as low as possible).
//
// Opt-in to this system is done via the register/unregister
// mechanism.
//
/////////////////////////////////////////////////////////////////////


class PowerManagerNotify
{
public:

    static void SetCallbackOnPowerStateEnter(function<void(pm_state state)> cbFn)
    {
        cbFnOnEnter_ = cbFn;
    }
    
    static void SetCallbackOnPowerStateExit(function<void(pm_state state)> cbFn)
    {
        cbFnOnExit_ = cbFn;
    }

    static void Register()
    {
        pm_notifier_register(&notifierConfig_);
    }

    static void UnRegister()
    {
        pm_notifier_unregister(&notifierConfig_);
    }

private:

    static void OnPowerStateEnter(pm_state state)
    {
        cbFnOnEnter_(state);
    }

    static void OnPowerStateExit(pm_state state)
    {
        cbFnOnExit_(state);
    }


private:

    inline static pm_notifier notifierConfig_ = {
        .state_entry = OnPowerStateEnter,
        .state_exit  = OnPowerStateExit,
    };

    inline static function<void(pm_state state)> cbFnOnEnter_ = [](pm_state){};
    inline static function<void(pm_state state)> cbFnOnExit_  = [](pm_state){};
};