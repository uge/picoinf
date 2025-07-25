#pragma once

#include "BME280.h"
#include "I2C.h"
#include "JerryScriptUtl.h"
#include "Utl.h"

#include <string>
#include <vector>
using namespace std;


class JSObj_BME280
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
            JerryScript::SetGlobalPropertyNoFree("BME280", jsFnObj);

            JerryScript::UseThenFreeNewObj([&](auto prototype){
                JerryScript::SetPropertyNoFree(jsFnObj, "prototype", prototype);

                JerryScript::SetPropertyToJerryNativeFunction(prototype, "GetTemperatureCelsius",    JsFnGetHandler);
                JerryScript::SetPropertyToJerryNativeFunction(prototype, "GetTemperatureFahrenheit", JsFnGetHandler);
                JerryScript::SetPropertyToJerryNativeFunction(prototype, "GetPressureHectoPascals",  JsFnGetHandler);
                JerryScript::SetPropertyToJerryNativeFunction(prototype, "GetPressureMilliBars",     JsFnGetHandler);
                JerryScript::SetPropertyToJerryNativeFunction(prototype, "GetAltitudeMeters",        JsFnGetHandler);
                JerryScript::SetPropertyToJerryNativeFunction(prototype, "GetAltitudeFeet",          JsFnGetHandler);
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

        BME280 *obj = (BME280 *)JerryScript::GetNativePointer(callInfo->this_value, &typeInfo_);

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
            else if (fnName == "GetPressureHectoPascals")
            {
                retVal = jerry_number(obj->GetPressureHectoPascals());
            }
            else if (fnName == "GetPressureMilliBars")
            {
                retVal = jerry_number(obj->GetPressureMilliBars());
            }
            else if (fnName == "GetAltitudeMeters")
            {
                retVal = jerry_number(obj->GetAltitudeMeters());
            }
            else if (fnName == "GetAltitudeFeet")
            {
                retVal = jerry_number(obj->GetAltitudeFeet());
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
        else if (argc != 1 || !jerry_value_is_number(argv[0]))
        {
            retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Invalid arguments to constructor");
        }
        else
        {
            // Extract parameters
            uint8_t addr = (uint8_t)jerry_value_as_number(argv[0]);

            if (BME280::IsValidAddr(addr) == false)
            {
                string error = string{"BME280 I2C address ("} + ToHex(addr) + ") is invalid";
                retVal = jerry_throw_sz(JERRY_ERROR_COMMON, error.c_str());
            }
            else
            {
                // Create a new object
                BME280 *obj = new BME280(addr, instance_);

                if (!obj)
                {
                    retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Failed to allocate memory for object");
                }
                else if (obj->IsAlive() == false)
                {
                    string error = string{"BME280 I2C address ("} + ToHex(addr) + ") is unresponsive";
                    retVal = jerry_throw_sz(JERRY_ERROR_COMMON, error.c_str());

                    delete obj;
                }
                else
                {
                    // Associate the C state with the JS object
                    JerryScript::SetNativePointer(callInfo->this_value, &typeInfo_, obj);
                }
            }
        }

        return retVal;
    }

    static void OnGarbageCollected(void *native, struct jerry_object_native_info_t *callInfo)
    {
        if (native)
        {
            BME280 *obj = (BME280 *)native;

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



