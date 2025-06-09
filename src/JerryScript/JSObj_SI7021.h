#pragma once

#include "SI7021.h"
#include "I2C.h"
#include "JerryScriptUtl.h"
#include "Utl.h"

#include <string>
#include <vector>
using namespace std;


class JSObj_SI7021
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
            JerryScript::SetGlobalPropertyNoFree("SI7021", jsFnObj);

            JerryScript::UseThenFreeNewObj([&](auto prototype){
                JerryScript::SetPropertyNoFree(jsFnObj, "prototype", prototype);

                JerryScript::SetPropertyToJerryNativeFunction(prototype, "GetTemperatureCelsius",    JsFnGetHandler);
                JerryScript::SetPropertyToJerryNativeFunction(prototype, "GetTemperatureFahrenheit", JsFnGetHandler);
                JerryScript::SetPropertyToJerryNativeFunction(prototype, "GetHumidityPct",           JsFnGetHandler);
            });
        });
    }


private:

    ///////////////////////////////////////////////////////////////////////////
    // JavaScript Function Handlers
    ///////////////////////////////////////////////////////////////////////////

    static jerry_value_t JsFnGetHandler(const jerry_call_info_t *callInfo,
                                        const jerry_value_t      argv[],
                                        const jerry_length_t     argc)
    {
        jerry_value_t retVal = jerry_undefined();

        SI7021 *obj = (SI7021 *)JerryScript::GetNativePointer(callInfo->this_value, &typeInfo_);

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

            if (fnName == "GetTemperatureCelsius")
            {
                retVal = jerry_number(obj->GetTemperatureCelsius());
            }
            else if (fnName == "GetTemperatureFahrenheit")
            {
                retVal = jerry_number(obj->GetTemperatureFahrenheit());
            }
            else if (fnName == "GetHumidityPct")
            {
                retVal = jerry_number(obj->GetHumidityPct());
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
        else if (argc != 0)
        {
            retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Invalid arguments to constructor");
        }
        else
        {
            // Create a new object
            SI7021 *obj = new SI7021(instance_);

            if (!obj)
            {
                retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Failed to allocate memory for object");
            }
            else if (obj->IsAlive() == false)
            {
                string error = string{"SI7021 I2C address ("} + ToHex(obj->GetAddr()) + ") is unresponsive";
                retVal = jerry_throw_sz(JERRY_ERROR_COMMON, error.c_str());

                delete obj;
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
            SI7021 *obj = (SI7021 *)native;

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



