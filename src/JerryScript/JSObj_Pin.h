#pragma once

#include <string>
using namespace std;

#include "Pin.h"

#include "JerryScriptUtl.h"


class JSObj_Pin
{
public:

    ///////////////////////////////////////////////////////////////////////////
    // Public Registration Interface
    ///////////////////////////////////////////////////////////////////////////

    static void Register()
    {
        JerryScript::UseThenFree(jerry_function_external(OnConstructed), [&](auto jsFnObj){
            JerryScript::SetGlobalPropertyNoFree("Pin", jsFnObj);

            JerryScript::UseThenFree(jerry_object(), [&](auto prototype){
                JerryScript::SetPropertyNoFree(jsFnObj, "prototype", prototype);

                JerryScript::SetPropertyToFunction(prototype, "On",  JsFnHandler);
                JerryScript::SetPropertyToFunction(prototype, "Off", JsFnHandler);
            });
        });
    }


private:

    ///////////////////////////////////////////////////////////////////////////
    // JavaScript Function Handlers
    ///////////////////////////////////////////////////////////////////////////

    static jerry_value_t JsFnHandler(const jerry_call_info_t *callInfo,
                                     const jerry_value_t      argv[],
                                     const jerry_length_t     argc)
    {
        jerry_value_t retVal = jerry_undefined();

        Pin *obj = (Pin *)JerryScript::GetNativePointer(callInfo->this_value, &typeInfo_);

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

                 if (fnName == "On")  { obj->DigitalWrite(1); }
            else if (fnName == "Off") { obj->DigitalWrite(0); }
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
            uint8_t pin = (int)jerry_value_as_number(argv[0]);

            // Create a new I2C object
            Log("new Pin(", pin, ")");
            Pin *obj = new Pin(pin);

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
            Pin *obj = (Pin *)native;

            Log("~Pin(", obj->GetPin(), ")");

            delete obj;
        }
    }


private:

    ///////////////////////////////////////////////////////////////////////////
    // JavaScript Type Identifier
    ///////////////////////////////////////////////////////////////////////////

    static inline const jerry_object_native_info_t typeInfo_ =
    {
        .free_cb = OnGarbageCollected,
    };
};



