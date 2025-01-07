#pragma once

#include "BH1750.h"
#include "I2C.h"
#include "JerryScriptUtl.h"

#include <string>
#include <vector>
using namespace std;


class JSObj_BH1750
{
public:

    ///////////////////////////////////////////////////////////////////////////
    // Public Configuration Interface
    ///////////////////////////////////////////////////////////////////////////

    static void SetI2CInstance(I2C::Instance instance)
    {
        instance_ = instance;
    }


    ///////////////////////////////////////////////////////////////////////////
    // Public Registration Interface
    ///////////////////////////////////////////////////////////////////////////

    static void Register()
    {
        JerryScript::UseThenFree(jerry_function_external(OnConstructed), [](auto jsFnObj){
            JerryScript::SetGlobalPropertyNoFree("BH1750", jsFnObj);

            JerryScript::UseThenFreeNewObj([&](auto prototype){
                JerryScript::SetPropertyNoFree(jsFnObj, "prototype", prototype);

                JerryScript::SetPropertyToFunction(prototype, "SetTemperatureCelsius",    JsFnSetHandler);
                JerryScript::SetPropertyToFunction(prototype, "SetTemperatureFahrenheit", JsFnSetHandler);
                JerryScript::SetPropertyToFunction(prototype, "GetLuxLowRes",             JsFnGetHandler);
                JerryScript::SetPropertyToFunction(prototype, "GetLuxHighRes",            JsFnGetHandler);
                JerryScript::SetPropertyToFunction(prototype, "GetLuxHigh2Res",           JsFnGetHandler);
            });
        });
    }


private:

    ///////////////////////////////////////////////////////////////////////////
    // JavaScript Function Handlers
    ///////////////////////////////////////////////////////////////////////////

    static jerry_value_t JsFnSetHandler(const jerry_call_info_t *callInfo,
                                        const jerry_value_t      argv[],
                                        const jerry_length_t     argc)
    {
        jerry_value_t retVal = jerry_undefined();

        BH1750 *obj = (BH1750 *)JerryScript::GetNativePointer(callInfo->this_value, &typeInfo_);

        if (argc != 1 || !jerry_value_is_number(argv[0]))
        {
            retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Invalid function arguments");
        }
        else if (!obj)
        {
            retVal = jerry_throw_sz(JERRY_ERROR_REFERENCE, "Failed to retrieve object");
        }
        else
        {
            string fnName = JerryScript::GetInternalPropertyAsString(callInfo->function, "name");

            int val = (int)jerry_value_as_number(argv[0]);

                 if (fnName == "SetTemperatureCelsius")    { obj->SetTemperatureCelsius(val);    }
            else if (fnName == "SetTemperatureFahrenheit") { obj->SetTemperatureFahrenheit(val); }
        }

        return retVal;
    }

    static jerry_value_t JsFnGetHandler(const jerry_call_info_t *callInfo,
                                        const jerry_value_t      argv[],
                                        const jerry_length_t     argc)
    {
        jerry_value_t retVal = jerry_undefined();

        BH1750 *obj = (BH1750 *)JerryScript::GetNativePointer(callInfo->this_value, &typeInfo_);

        if (argc != 0)
        {
            retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Invalid function arguments");
        }
        else if (!obj)
        {
            retVal = jerry_throw_sz(JERRY_ERROR_REFERENCE, "Failed to retrieve object");
        }
        else
        {
            string fnName = JerryScript::GetInternalPropertyAsString(callInfo->function, "name");

            if (fnName == "GetLuxLowRes")
            {
                retVal = jerry_number(obj->GetLuxLowRes());
            }
            else if (fnName == "GetLuxHighRes")
            {
                retVal = jerry_number(obj->GetLuxHighRes());
            }
            else if (fnName == "GetLuxHigh2Res")
            {
                retVal = jerry_number(obj->GetLuxHigh2Res());
            }
        }

        return retVal;
    }


    ///////////////////////////////////////////////////////////////////////////
    // JavaScript Construction / Destruction
    ///////////////////////////////////////////////////////////////////////////

    static jerry_value_t OnConstructed(const jerry_call_info_t *callInfo,
                                       const jerry_value_t      argv[],
                                       const jerry_length_t     argc)
    {
        jerry_value_t retVal = jerry_undefined();

        if (JerryScript::CalledAsConstructor(callInfo) == false)
        {
            retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Improperly called as function and not constructor");
        }
        else if (argc != 1 || !jerry_value_is_number(argv[0]))
        {
            retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Invalid arguments to constructor");
        }
        else
        {
            // Extract parameters
            uint8_t addr = (uint8_t)jerry_value_as_number(argv[0]);

            // Create a new I2C object
            BH1750 *obj = new BH1750(addr, instance_);

            if (!obj)
            {
                retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Failed to allocate memory for object");
            }
            else
            {
                // Associate the C state with the JS object
                JerryScript::SetNativePointer(callInfo->this_value, &typeInfo_, obj);
            }
        }

        return retVal;
    }

    static void OnGarbageCollected(void *native, struct jerry_object_native_info_t *callInfo)
    {
        if (native)
        {
            BH1750 *obj = (BH1750 *)native;

            delete obj;
        }
    }


private:

    // I2C instance
    inline static I2C::Instance instance_;

    // JavaScript Type Identifier
    static inline const jerry_object_native_info_t typeInfo_ =
    {
        .free_cb = OnGarbageCollected,
    };
};



