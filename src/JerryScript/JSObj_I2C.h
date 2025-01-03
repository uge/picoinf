#pragma once

#include <string>
using namespace std;

#include "I2C.h"

#include "JerryScriptUtl.h"


class JSObj_I2C
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
            JerryScript::SetGlobalPropertyNoFree("I2C", jsFnObj);

            JerryScript::UseThenFreeNewObj([&](auto prototype){
                JerryScript::SetPropertyNoFree(jsFnObj, "prototype", prototype);

                JerryScript::SetPropertyToFunction(prototype, "IsAlive",     IsAliveHandler);
                JerryScript::SetPropertyToFunction(prototype, "ReadReg8",    ReadHandler);
                JerryScript::SetPropertyToFunction(prototype, "ReadReg16",   ReadHandler);
                JerryScript::SetPropertyToFunction(prototype, "WriteReg8",   WriteHandler);
                JerryScript::SetPropertyToFunction(prototype, "WriteReg16",  WriteHandler);
            });
        });
    }


private:

    ///////////////////////////////////////////////////////////////////////////
    // JavaScript Function Handlers
    ///////////////////////////////////////////////////////////////////////////

    static jerry_value_t IsAliveHandler(const jerry_call_info_t *callInfo,
                                        const jerry_value_t      argv[],
                                        const jerry_length_t     argc)
    {
        jerry_value_t retVal = jerry_undefined();

        I2C *obj = (I2C *)JerryScript::GetNativePointer(callInfo->this_value, &typeInfo_);

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
            retVal = jerry_boolean(obj->IsAlive());
        }

        return retVal;
    }

    static jerry_value_t ReadHandler(const jerry_call_info_t *callInfo,
                                     const jerry_value_t      argv[],
                                     const jerry_length_t     argc)
    {
        jerry_value_t retVal = jerry_undefined();

        I2C *obj = (I2C *)JerryScript::GetNativePointer(callInfo->this_value, &typeInfo_);

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
            
            uint8_t reg = (int)jerry_value_as_number(argv[0]);

            if (fnName == "ReadReg8")
            {
                uint8_t val = obj->ReadReg8(reg);

                retVal = jerry_number(val);
            }
            else if (fnName == "ReadReg16")
            {
                uint16_t val = obj->ReadReg16(reg);

                retVal = jerry_number(val);
            }
        }

        return retVal;
    }

    static jerry_value_t WriteHandler(const jerry_call_info_t *callInfo,
                                      const jerry_value_t      argv[],
                                      const jerry_length_t     argc)
    {
        jerry_value_t retVal = jerry_undefined();

        I2C *obj = (I2C *)JerryScript::GetNativePointer(callInfo->this_value, &typeInfo_);

        if (argc != 2 || !jerry_value_is_number(argv[0]) || !jerry_value_is_number(argv[1]))
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
            
            uint8_t reg = (int)jerry_value_as_number(argv[0]);

            if (fnName == "WriteReg8")
            {
                uint8_t val = (int)jerry_value_as_number(argv[0]);

                obj->WriteReg8(reg, val);
            }
            else if (fnName == "WriteReg16")
            {
                uint16_t val = (int)jerry_value_as_number(argv[0]);

                obj->WriteReg16(reg, val);
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
            uint8_t addr = (int)jerry_value_as_number(argv[0]);

            // Create a new I2C object
            I2C *obj = new I2C(addr, instance_);

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
            I2C *obj = (I2C *)native;

            delete obj;
        }
    }


private:

    // Default to I2C0 instance
    inline static I2C::Instance instance_ = I2C::Instance::I2C0;

    // JavaScript Type Identifier
    static inline const jerry_object_native_info_t typeInfo_ =
    {
        .free_cb = OnGarbageCollected,
    };
};



