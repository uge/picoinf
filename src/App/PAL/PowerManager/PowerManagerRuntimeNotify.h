#pragma once

#include <functional>
using namespace std;

#include "PowerManagerRuntimeNotifyDriver.h"
#include "Log.h"


class PowerManagerRuntimeNotify
{
public:

    static void SetCallbackOnInit(function<void()> cbFn)
    {
        cbOnInit_ = cbFn;

        GetApi()->setCbOnInit(OnInit);
    }
    
    static void SetCallbackOnPmAction(function<void(pm_device_action action)> cbFn)
    {
        cbOnPmAction_ = cbFn;

        GetApi()->setCbOnPmAction(OnPmAction);
    }
    
    static void Open()
    {
        GetApi()->open(GetDev());
    }

    static void Close()
    {
        GetApi()->close(GetDev());
    }

private:

    static const device *GetDev()
    {
        return device_get_binding(DUMMY_DRIVER_NAME);
    }

    static dummy_driver_api *GetApi()
    {
        return (struct dummy_driver_api *)(GetDev())->api;
    }

    static void OnInit()
    {
        cbOnInit_();
    }

    static void OnPmAction(pm_device_action action)
    {
        cbOnPmAction_(action);
    }


private:

    inline static function<void()>                        cbOnInit_     = []{};
    inline static function<void(pm_device_action action)> cbOnPmAction_ = [](pm_device_action action){};
};