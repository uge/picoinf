#pragma once

#include <string>
using namespace std;

#include "I2C.h"

#include "JerryScriptUtl.h"


class JSObj_I2C
{
public:

    ///////////////////////////////////////////////////////////////////////////
    // Public Registration Interface
    ///////////////////////////////////////////////////////////////////////////

    static void Register()
    {
        JerryScript::UseThenFree(jerry_function_external(OnConstructed), [](auto jsFnObj){
            JerryScript::SetGlobalPropertyNoFree("I2C", jsFnObj);

            JerryScript::UseThenFreeNewObj([&](auto prototype){
                JerryScript::SetPropertyNoFree(jsFnObj, "prototype", prototype);

                JerryScript::SetPropertyToFunction(prototype, "IsAvailable", IsAvailableHandler);
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

    static jerry_value_t IsAvailableHandler(const jerry_call_info_t *callInfo,
                                            const jerry_value_t      argv[],
                                            const jerry_length_t     argc)
    {
        jerry_value_t retVal = jerry_undefined();

        JSObj_I2C *obj = (JSObj_I2C *)JerryScript::GetNativePointer(callInfo->this_value, &typeInfo_);

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
            retVal = jerry_boolean(I2C::CheckAddr(obj->GetAddr()));
        }

        return retVal;
    }

    static jerry_value_t ReadHandler(const jerry_call_info_t *callInfo,
                                     const jerry_value_t      argv[],
                                     const jerry_length_t     argc)
    {
        jerry_value_t retVal = jerry_undefined();

        JSObj_I2C *obj = (JSObj_I2C *)JerryScript::GetNativePointer(callInfo->this_value, &typeInfo_);

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
                uint8_t val = I2C::ReadReg8(obj->GetAddr(), reg);

                retVal = jerry_number(val);
            }
            else if (fnName == "ReadReg16")
            {
                uint16_t val = I2C::ReadReg16(obj->GetAddr(), reg);

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

        JSObj_I2C *obj = (JSObj_I2C *)JerryScript::GetNativePointer(callInfo->this_value, &typeInfo_);

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

                I2C::WriteReg8(obj->GetAddr(), reg, val);
            }
            else if (fnName == "WriteReg16")
            {
                uint16_t val = (int)jerry_value_as_number(argv[0]);

                I2C::WriteReg16(obj->GetAddr(), reg, val);
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
            Log("new I2C(", addr, ")");
            JSObj_I2C *obj = new JSObj_I2C(addr);

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
            JSObj_I2C *obj = (JSObj_I2C *)native;

            Log("~I2C(", obj->addr_, ")");

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


private:

    ///////////////////////////////////////////////////////////////////////////
    // C++ Construction / Destruction
    ///////////////////////////////////////////////////////////////////////////

    JSObj_I2C(uint8_t addr)
    {
        addr_ = addr;
    }

    uint8_t GetAddr() const
    {
        return addr_;
    }

    ~JSObj_I2C()
    {
        // nothing to do
    }


private:

    uint8_t addr_ = 0;
};



