#pragma once

#include "MMC56x3.h"
#include "I2C.h"
#include "JerryScriptUtl.h"

#include <string>
#include <vector>
using namespace std;


class JSObj_MMC56x3
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
            JerryScript::SetGlobalPropertyNoFree("MMC56x3", jsFnObj);

            JerryScript::UseThenFreeNewObj([&](auto prototype){
                JerryScript::SetPropertyNoFree(jsFnObj, "prototype", prototype);

                JerryScript::SetPropertyToFunction(prototype, "GetMagXMicroTeslas", JsFnGetHandler);
                JerryScript::SetPropertyToFunction(prototype, "GetMagYMicroTeslas", JsFnGetHandler);
                JerryScript::SetPropertyToFunction(prototype, "GetMagZMicroTeslas", JsFnGetHandler);
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

        MMC56x3 *obj = (MMC56x3 *)JerryScript::GetNativePointer(callInfo->this_value, &typeInfo_);

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

            if (fnName == "GetMagXMicroTeslas")
            {
                retVal = jerry_number(obj->GetMagXMicroTeslas());
            }
            else if (fnName == "GetMagYMicroTeslas")
            {
                retVal = jerry_number(obj->GetMagYMicroTeslas());
            }
            else if (fnName == "GetMagZMicroTeslas")
            {
                retVal = jerry_number(obj->GetMagZMicroTeslas());
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
            // Create a new I2C object
            MMC56x3 *obj = new MMC56x3(instance_);

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
            MMC56x3 *obj = (MMC56x3 *)native;

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



