#include "JS_I2C.h"

#include <string>
using namespace std;

#include "JerryScriptUtl.h"


///////////////////////////////////////////////////////////////////////////
// Javascript construction/destruction
///////////////////////////////////////////////////////////////////////////

static void OnGarbageCollected(void *native, struct jerry_object_native_info_t *callInfo)
{
    if (native)
    {
        JS_I2C *jsI2c = (JS_I2C *)native;

        printf("Addr(%i) - GarbageCollected (%s)\n", jsI2c->address_, jsI2c->name_.c_str());

        delete jsI2c;
    }
}

static inline const jerry_object_native_info_t typeInfo_ =
{
    .free_cb = OnGarbageCollected,
};

static jerry_value_t OnConstructed(const jerry_call_info_t *callInfo,
                                   const jerry_value_t      argv[],
                                   const jerry_length_t     argc)
{
    jerry_value_t retVal;

    if (argc < 2 || !jerry_value_is_number(argv[0]) || !jerry_value_is_string(argv[1]))
    {
        retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Invalid arguments to I2C constructor");
    }
    else if (JerryScript::CalledAsConstructor(callInfo))
    {
        printf("CALLED AS CTOR\n");

        // Create a new I2C object
        JS_I2C *jsI2c = new JS_I2C();
        if (!jsI2c)
        {
            retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Failed to allocate memory for I2C object");
        }
        else
        {
            retVal = jerry_undefined();

            // Initialize the C object with parameters from the constructor
            jsI2c->address_ = (int)jerry_value_as_number(argv[0]);
            jsI2c->name_ = JerryScript::GetValueAsString(argv[1]);

            printf("Addr(%i) - Constructed (%s)\n", jsI2c->address_, jsI2c->name_.c_str());

            // Associate the C state with the JS object
            JerryScript::SetNativePointer(callInfo->this_value, &typeInfo_, jsI2c);
        }
    }
    else
    {
        printf("WOW CALLED AS JUST A FUNCTION\n");

        // called as a function
        retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Called as function instead of constructor");
    }

    return retVal;
}



///////////////////////////////////////////////////////////////////////////
// Javascript-facing public interface
///////////////////////////////////////////////////////////////////////////

static jerry_value_t JsFnGetAddress(const jerry_call_info_t *callInfo,
                                    const jerry_value_t      argv[],
                                    const jerry_length_t     argc)
{
    jerry_value_t retVal;

    JS_I2C *jsI2c = (JS_I2C *)JerryScript::GetNativePointer(callInfo->this_value, &typeInfo_);
    if (jsI2c)
    {
        retVal = jerry_number(jsI2c->address_);

        printf("Addr(%i) - JsFnGetAddress\n", jsI2c->address_);
    }
    else
    {
        retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Failed to retrieve I2C object");
    }

    return retVal;
}

static jerry_value_t JsFnPrint(const jerry_call_info_t *callInfo,
                               const jerry_value_t      argv[],
                               const jerry_length_t     argc)
{
    jerry_value_t retVal;

    JS_I2C *jsI2c = (JS_I2C *)JerryScript::GetNativePointer(callInfo->this_value, &typeInfo_);
    if (jsI2c)
    {
        printf("Addr(%i) - JsFnPrint (%s)\n", jsI2c->address_, jsI2c->name_.c_str());

        retVal = jerry_undefined();
    }
    else
    {
        retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Failed to retrieve I2C object");
    }

    return retVal;
}






///////////////////////////////////////////////////////////////////////////
// Public interface
///////////////////////////////////////////////////////////////////////////

void JS_I2C::Register()
{
    JerryScript::UseThenFree(jerry_function_external(OnConstructed), [&](auto jsClassObj){
        JerryScript::SetGlobalPropertyNoFree("I2C", jsClassObj);

        JerryScript::UseThenFree(jerry_object(), [&](auto prototype){
            JerryScript::SetPropertyNoFree(jsClassObj, "prototype", prototype);

            JerryScript::SetPropertyToFunction(prototype, "getAddress", JsFnGetAddress);
            JerryScript::SetPropertyToFunction(prototype, "print",      JsFnPrint);
        });
    });
}



