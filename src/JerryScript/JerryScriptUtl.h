#pragma once

#include <functional>
#include <string>
using namespace std;

#include "jerryscript.h"
#include "jerryscript-port.h"
#include "jerryscript-ext/handlers.h"
#include "jerryscript-ext/properties.h"


// Notes on how this all works.
//
// JavaScript variables:
// - have names
// - are associated with a given scope
//   - are properties of that scope
// - there is a global scope, represented by a global object
//   - names of vars that just "exist" are properties of that global scope
//
// JavaScript knows about two types:
// - Objects ({}, [], functions, Map(), etc)
// - Scalars (numbers, strings, etc)
//
// JavaScript Objects
// - have properties
//   - they can be Scalars or Objects
//
// JavaScript Object Instances
// - let a = {}
//   - a is an instance of an object, but 'new' wasn't called
// - let m = new Map()
//   - a is an instance of an object, and 'new' was called
//
//
// Javascript 'new'
// - creates a new instance of an object according to definition of object
//   - you define the object by defining a function
// - you instantiate using keyword 'new', the function name, followed by ()
// - eg
//   - function A() {}
//   - let a = new A()
//   - now you have an "instance" of the A "class"
// - (side note, A() can do stuff, like print, and this still works, it's just a function)
//   - (as in, if function A() { console.log('hi') }, let a = new A(), you'll get a new A object and see a print)
// 
//
// JavaScript Constructors
// - hook into JavaScript convention by having a 'prototype' member object
//   - the properties of which are passed down to new instances of this class
//     - as in, define functions on the prototype, all instances share them
// - classes are just syntactic sugar for the above
//
//
// JavaScript Property Set/Get/Accessors
// - can be set/get as normal
// - can be wrapped in getter/setter functions that auto-fire transparent to caller
//
//
//
// C++ interfacing with Objects
// - can create generic objects
//   - c++ can tie data to objects
//     - can jerry_object_set_native_ptr, which associates userData with that object
//       - how you come to posses that object is irrelevant
//       - can also use this to tie an object destructed callback to this object
// - function objects
//   - c++ lib supports creating those specially, but they're still just objects
//   - the c++ function is passed information when invoked
//     - arguments
//     - 'this' pointer
//       - refers to the object which the function is a property of, not the function object iself!
//         - as in, a.B(), the 'this' refers to a, not the B function object
//       - this way, functions can get access to the 'this' pointer of the owning object
//
//
// C++ interfacing with JavaScript Constructors
// - recall that constructors are just functions
// - when the function is called, there is a c++ invocation of that function
// - normally, with a regular function call, the 'this' parameter is the owning object
//   - but, with 'new', the 'this' parameter is the new object itself
//     - this is a constructor (call it constructor function)
//     - need to check the way the function is called to determine if being called as a
//       regular function vs constructor (api below supports this)
// - so, the constructor function then
//   - can instantiate c++ state and associate with the 'this' pointer
//     - knowing that it can be freed when object destructed
//   - also knows that functions on this object have access to this object
//     - because, recall, they get access to the 'this' of their owner, which is this new object
//
//
// C++ interfacing with JavaScript Accessors
// - are just c++ functions which are invoked when JavaScript syntax says read/assign
// - behave the same ways as explicitly-defined member function objects
//   - therefore, have access to the 'this' parameter of their owning object as well
//     - ie can look up state
// - special care given in this code to also ensure the field accessors have state themselves
//   via internal properties
//   - specifically, when an accessor is called, it has access to the name of the property it
//     is being called on, so it can decide what to do based on that information.
//
//
// C++ names
// - tie object/scalar names to the global scope object as property to make global
// - tie object/scalar names to other objects to have as properties of that object
//


class JerryScript
{
public:

    ///////////////////////////////////////////////////////////////////////////
    // Runtime management
    ///////////////////////////////////////////////////////////////////////////

    static void EnableJerryX()
    {
        /* Register 'print' function from the extensions */
        jerryx_register_global ("print", jerryx_handler_print);
    }

    static void UseVM(function<void()> fn)
    {
        // init
        jerry_init(JERRY_INIT_MEM_STATS);

        // user code
        fn();

        {
            jerry_heap_stats_t stats = {0};
            bool get_stats_ret = jerry_heap_stats (&stats);

            if (get_stats_ret)
            {
                printf("Heap Stats:\n");
                printf("Size                : %i\n", stats.size);
                printf("Allocated Bytes     : %i\n", stats.allocated_bytes);
                printf("Peak Allocated Bytes: %i\n", stats.peak_allocated_bytes);
            }
            else
            {
                printf("ERR --------- could not get heap stats!!\n");
            }

            jerry_log_set_level(JERRY_LOG_LEVEL_DEBUG);
        }

        // cleanup
        jerry_cleanup();
    }

    static string ParseAndExecuteScript(const string &script, uint64_t timeoutMs = 0)
    {
        string retVal;
        jerry_value_t parsedCode = jerry_undefined();

        retVal = ParseScript(script, &parsedCode);
        UseThenFree(parsedCode, [&](auto parsedCode){
            if (retVal == "")
            {
                retVal = RunParsedCode(parsedCode, timeoutMs);
            }
        });

        return retVal;
    }

    // parses script and returns any exception or error in the return value.
    // parsed code is automatically freed.
    // if a parsedCodeRet value is passed, a reference is returned, and must later be freed.
    static string ParseScript(const string &script, jerry_value_t *parsedCodeRet = nullptr)
    {
        string retVal;

        jerry_parse_options_t options;
        options.options = JERRY_PARSE_STRICT_MODE;

        jerry_value_t parsedCode = jerry_parse((const jerry_char_t *)script.c_str(), script.size(), &options);

        UseThenFree(parsedCode, [&](auto parsedCode){
            if (parsedCodeRet)
            {
                *parsedCodeRet = jerry_value_copy(parsedCode);
            }

            if (jerry_value_is_exception(parsedCode))
            {
                retVal += "[Script parse exception]";
                retVal += "\n";
                retVal += GetExceptionDetails(parsedCode);
            }
        });

        return retVal;
    }

    static string RunParsedCode(jerry_value_t parsedCode, uint64_t timeoutMs = 0)
    {
        string retVal;

        SetExecutionTimeoutMs(timeoutMs);

        UseThenFree(jerry_run(parsedCode), [&](auto result){
            if (jerry_value_is_exception(result))
            {
                retVal += "[Runtime exception]";
                retVal += "\n";
                retVal += GetExceptionDetails(result);
            }
        });

        return retVal;
    }


private:

    inline static uint64_t executionTimeoutMs_ = 0;
    inline static uint64_t timeStart_ = 0;
    static void SetExecutionTimeoutMs(uint64_t ms)
    {
        timeStart_ = TimeNow();
        executionTimeoutMs_ = ms;

        jerry_halt_handler(1, [](void *) -> jerry_value_t {
            jerry_value_t retVal = jerry_undefined();

            if (executionTimeoutMs_ != 0)
            {
                uint64_t timeNow = TimeNow();
                uint64_t timeDiff = timeNow - timeStart_;

                if (timeDiff >= executionTimeoutMs_)
                {
                    retVal = jerry_throw_sz(JERRY_ERROR_EVAL, "Script execution timeout");
                }
            }

            return retVal;
        }, nullptr);
    }

public:


    ///////////////////////////////////////////////////////////////////////////
    // Automatic storage management
    ///////////////////////////////////////////////////////////////////////////

    static void UseThenFree(jerry_value_t value, function<void(jerry_value_t value)> fnUse)
    {
        fnUse(value);
        jerry_value_free(value);
    }


    ///////////////////////////////////////////////////////////////////////////
    // Property getters and setters
    ///////////////////////////////////////////////////////////////////////////

    static void SetGlobalPropertyNoFree(const string &propertyName, jerry_value_t propertyValue)
    {
        UseThenFree(jerry_current_realm(), [&](auto globalObj){
            SetPropertyNoFree(globalObj, propertyName, propertyValue);
        });
    }

    static void SetGlobalProperty(const string &propertyName, jerry_value_t propertyValue)
    {
        SetGlobalPropertyNoFree(propertyName, propertyValue);

        jerry_value_free(propertyValue);
    }

    static void SetGlobalFunction(const string &name, jerry_external_handler_t handler)
    {
        UseThenFree(jerry_current_realm(), [&](auto globalObj){
            SetPropertyToFunction(globalObj, name, handler);
        });
    }

    // set a property on an object. the property will not be freed.
    static void SetPropertyNoFree(jerry_value_t objTarget, const string &propertyName, jerry_value_t propertyValue)
    {
        UseThenFree(jerry_string_sz(propertyName.c_str()), [&](auto jerryPropertyName){
            jerry_value_free(jerry_object_set(objTarget, jerryPropertyName, propertyValue));
        });
    }

    // set a property on an object. the property will be freed before returning.
    static void SetProperty(jerry_value_t objTarget, const string &propertyName, jerry_value_t propertyValue)
    {
        SetPropertyNoFree(objTarget, propertyName, propertyValue);

        jerry_value_free(propertyValue);
    }

    // set an object's property to be a function object.
    // automatically also:
    // - tag the function object with its own name as an internal property
    // - associate native pointer data with nullptr type
    //   - same as property getter/setter
    static void SetPropertyToFunction(jerry_value_t             objTarget,
                                      const string             &propertyName,
                                      jerry_external_handler_t  handler,
                                      void                     *data = nullptr)
    {
        // get handler function object
        jerry_value_t handlerFn = jerry_function_external(handler);

        // set it as the given property
        SetPropertyNoFree(objTarget, propertyName, handlerFn);

        // also tag the function object itself with its own property name.
        // this way the function, when called, if not obvious, can know which
        // function object it is acting on behalf of
        SetInternalPropertyToString(handlerFn, "name", propertyName);

        if (data)
        {
            SetNativePointer(handlerFn, nullptr, data);
        }

        // release function object
        jerry_value_free(handlerFn);
    }

    static string GetPropertyAsString(jerry_value_t obj, const string &property)
    {
        string retVal;

        UseThenFree(jerry_string_sz(property.c_str()), [&](auto jerryPropertyName){
            UseThenFree(jerry_object_get(obj, jerryPropertyName), [&](auto jerryPropertyValue){
                retVal = GetValueAsString(jerryPropertyValue);
            });
        });

        return retVal;
    }


    ///////////////////////////////////////////////////////////////////////////
    // Internal Property getters and setters
    ///////////////////////////////////////////////////////////////////////////

    // set an internal property on an object. the property will be freed before returning.
    static void SetInternalProperty(jerry_value_t objTarget, const string &propertyName, jerry_value_t propertyValue)
    {
        UseThenFree(jerry_string_sz(propertyName.c_str()), [&](auto jerryPropertyName){
            jerry_object_set_internal(objTarget, jerryPropertyName, propertyValue);
        });

        jerry_value_free(propertyValue);
    }

    static void SetInternalPropertyToString(jerry_value_t objTarget, const string &propertyName, string propertyValue)
    {
        SetInternalProperty(objTarget, propertyName, jerry_string_sz(propertyValue.c_str()));
    }

    static bool GetInternalProperty(jerry_value_t obj, const string &propertyName, function<void(jerry_value_t val)> fn)
    {
        bool retVal = false;

        UseThenFree(jerry_string_sz(propertyName.c_str()), [&](auto jerryPropertyName){
            if (jerry_object_has_internal(obj, jerryPropertyName))
            {
                retVal = true;

                jerry_value_t val = jerry_object_get_internal(obj, jerryPropertyName);

                fn(val);

                jerry_value_free(val);
            }
        });

        return retVal;
    }

    static string GetInternalPropertyAsString(jerry_value_t obj, const string &propertyName)
    {
        string retVal;

        GetInternalProperty(obj, propertyName, [&](jerry_value_t val){
            retVal = GetValueAsString(val);
        });

        return retVal;
    }


    ///////////////////////////////////////////////////////////////////////////
    //
    // Property Descriptors
    // - setting up getters/setters
    // - making properties readonly
    //
    // ok, seemingly there is no re-defining properties
    // eg "TypeError: Cannot redefine property: val1"
    // - as in, if you set it to be a value, no changing to be setter/getter later
    // - if you set a getter, can't add a getter later
    //   - so set setter, getter, or both at the same time
    // - notably, you can set an object's property value, then make it read-only
    //   - the first set isn't via property descriptor (but could be)
    //     - so it's ok to set as read-only in that case, because the setting
    //       of the value didn't change the descriptor, so the set to readonly
    //       isn't a "re-definition"
    //
    ///////////////////////////////////////////////////////////////////////////
    
    static void PrintPropertyDescriptor(const string &propertyName, jerry_property_descriptor_t desc)
    {
        printf("Property: %s\n", propertyName.c_str());
        printf("value: %f\n", jerry_value_as_number(desc.value));
        printf("flags: %02X\n", desc.flags);

        printf("setter: %s\n", GetValueAsString(desc.setter).c_str());
        printf("getter: %s\n", GetValueAsString(desc.getter).c_str());

        printf("JERRY_PROP_NO_OPTS                : %i\n", desc.flags & JERRY_PROP_NO_OPTS                 ? 1 : 0);
        printf("JERRY_PROP_IS_CONFIGURABLE        : %i\n", desc.flags & JERRY_PROP_IS_CONFIGURABLE         ? 1 : 0);
        printf("JERRY_PROP_IS_ENUMERABLE          : %i\n", desc.flags & JERRY_PROP_IS_ENUMERABLE           ? 1 : 0);
        printf("JERRY_PROP_IS_WRITABLE            : %i\n", desc.flags & JERRY_PROP_IS_WRITABLE             ? 1 : 0);

        printf("JERRY_PROP_IS_CONFIGURABLE_DEFINED: %i\n", desc.flags & JERRY_PROP_IS_CONFIGURABLE_DEFINED ? 1 : 0);
        printf("JERRY_PROP_IS_ENUMERABLE_DEFINED  : %i\n", desc.flags & JERRY_PROP_IS_ENUMERABLE_DEFINED   ? 1 : 0);
        printf("JERRY_PROP_IS_WRITABLE_DEFINED    : %i\n", desc.flags & JERRY_PROP_IS_WRITABLE_DEFINED     ? 1 : 0);

        printf("JERRY_PROP_IS_VALUE_DEFINED       : %i\n", desc.flags & JERRY_PROP_IS_VALUE_DEFINED        ? 1 : 0);
        printf("JERRY_PROP_IS_GET_DEFINED         : %i\n", desc.flags & JERRY_PROP_IS_GET_DEFINED          ? 1 : 0);
        printf("JERRY_PROP_IS_SET_DEFINED         : %i\n", desc.flags & JERRY_PROP_IS_SET_DEFINED          ? 1 : 0);

        printf("JERRY_PROP_SHOULD_THROW           : %i\n", desc.flags & JERRY_PROP_SHOULD_THROW            ? 1 : 0);
    }

    // safe if used on a property which doesn't exist yet
    static void UseDescriptorThenFree(jerry_value_t objTarget, const string &propertyName, function<void(jerry_property_descriptor_t *desc)> fn)
    {
        UseThenFree(jerry_string_sz(propertyName.c_str()), [&](auto jerryPropertyName){
            jerry_property_descriptor_t desc = jerry_property_descriptor();

            if (jerry_object_get_own_prop(objTarget, jerryPropertyName, &desc))
            {
                // run user code
                fn(&desc);

                // update with any changes
                jerry_value_t ret = jerry_object_define_own_prop(objTarget, jerryPropertyName, &desc);
                if (jerry_value_is_exception(ret))
                {
                    // printf("********** Exception **********\n");
                    // printf("%s\n", GetExceptionAsString(ret).c_str());
                }
                else if (jerry_value_is_false(ret))
                {
                    // printf("======== was false ========\n");
                }
                else
                {
                    // nothing to do
                }
                jerry_value_free(ret);
            }

            jerry_property_descriptor_free(&desc);
        });
    }

    // you can set a property value, and then call this to make it read-only
    static void SetPropertyDescriptorReadonly(jerry_value_t objTarget, const string &propertyName)
    {
        UseDescriptorThenFree(objTarget, propertyName, [](jerry_property_descriptor_t *prop){
            // state that writable has been indicated
            prop->flags |= JERRY_PROP_IS_WRITABLE_DEFINED;

            // set the writable state to off
            prop->flags &= (uint16_t) ~JERRY_PROP_IS_WRITABLE;
        });
    }

    static void SetPropertyDescriptorGetterSetter(jerry_value_t             objTarget,
                                                  const string             &propertyName,
                                                  jerry_external_handler_t  handlerGetter,
                                                  jerry_external_handler_t  handlerSetter,
                                                  void                     *data)
    {
        UseDescriptorThenFree(objTarget, propertyName, [&](jerry_property_descriptor_t *prop){
            auto Set = [&](jerry_value_t *getterSetter, uint16_t bit, jerry_external_handler_t handler){
                if (handler)
                {
                    prop->flags |= bit;

                    // set the getter/setter (no free needed)
                    *getterSetter = jerry_function_external(handler);

                    // associate data with the function object itself
                    SetNativePointer(*getterSetter, nullptr, data);

                    // give it a name, now handler knows which property being accessed
                    SetInternalPropertyToString(*getterSetter, "name", propertyName);
                }
            };

            Set(&prop->getter, JERRY_PROP_IS_GET_DEFINED, handlerGetter);
            Set(&prop->setter, JERRY_PROP_IS_SET_DEFINED, handlerSetter);
        });
    }

    static void SetPropertyDescriptorGetterOnly(jerry_value_t             objTarget,
                                                const string             &propertyName,
                                                jerry_external_handler_t  handler,
                                                void                     *data)
    {
        SetPropertyDescriptorGetterSetter(objTarget, propertyName, handler, nullptr, data);
    }

    static void SetPropertyDescriptorSetterOnly(jerry_value_t             objTarget,
                                                const string             &propertyName,
                                                jerry_external_handler_t  handler,
                                                void                     *data)
    {
        SetPropertyDescriptorGetterSetter(objTarget, propertyName, nullptr, handler, data);
    }


    ///////////////////////////////////////////////////////////////////////////
    //
    // State Management Helpers
    //
    // Stateless handlers - ie Proxy (knows-a)
    // - these are
    //   - function objects
    //   - attribute getter/setters
    // - they get, automatically, a native pointer to the c++ object they operate on
    //   - stored on the function object itself
    //   - there must be 1:1 function objects with c++ object
    //     - or, at least for function objects, just don't set the data pointer if
    //       not going to be used that way (optional)
    // - look up this pointer via function object and type nullptr
    //   - function object = callInfo->function
    //
    // Stateful handlers - ie Constructor (is-a)
    // - get the same as stateless handlers, but must not use that
    //   - they are part of prototype, and they are not 1:1 with c++ objects
    // - instead, the ctor of the c++ constructor will also
    //   - store on the instantiated js object a pointer to state
    //   - look up this pointer via the 'this' object and type c++ type
    //     - 'this' object = callInfo->this_value
    //
    ///////////////////////////////////////////////////////////////////////////

    static void SetNativePointer(jerry_value_t                     obj,
                                 const jerry_object_native_info_t *type,
                                 void                             *data)
    {
        jerry_object_set_native_ptr(obj, type, data);
    }

    static void *GetNativePointer(jerry_value_t obj, const jerry_object_native_info_t *type = nullptr)
    {
        void *retVal = nullptr;

        if (jerry_object_has_native_ptr(obj, type))
        {
            retVal = jerry_object_get_native_ptr(obj, type);
        }

        return retVal;
    }


    ///////////////////////////////////////////////////////////////////////////
    // Function Helpers
    ///////////////////////////////////////////////////////////////////////////

    // functions can be called as a function, or as an 'new' constructor,
    // this function tells you which.
    static bool CalledAsConstructor(const jerry_call_info_t *callInfo)
    {
        bool retVal = false;

        if (callInfo)
        {
            // when called as a regular function, the new_target is undefined
            retVal = jerry_value_is_undefined(callInfo->new_target) != true;
        }

        return retVal;
    }


    ///////////////////////////////////////////////////////////////////////////
    // Value transformation
    ///////////////////////////////////////////////////////////////////////////

    static string GetValueAsString(jerry_value_t value)
    {
        string retVal;

        UseThenFree(jerry_value_to_string(value), [&](auto jerryStr){
            // calculate number of bytes of characters (terminator not included)
            jerry_size_t size = jerry_string_size(jerryStr, JERRY_ENCODING_UTF8);

            // fills size chars with '\0', plus internally has terminator beyond that
            retVal.resize(size);

            // decode directly into string buffer
            jerry_string_to_buffer(jerryStr,
                                   JERRY_ENCODING_UTF8,
                                   (jerry_char_t *)retVal.data(),
                                   size);
        });

        return retVal;
    }


    ///////////////////////////////////////////////////////////////////////////
    // Exceptions
    ///////////////////////////////////////////////////////////////////////////

    // works, read-only, doesn't change anything, just looks
    static const char *GetExceptionErrorType(jerry_value_t exception)
    {
        const char *retVal;

        if (!jerry_value_is_exception(exception))
        {
            retVal = "NotAnError";
        }
        else
        {
            jerry_error_t error_type = jerry_error_type(exception);

            switch (error_type)
            {
                case JERRY_ERROR_NONE     : retVal = "NoneError";      break;
                case JERRY_ERROR_COMMON   : retVal = "CommonError";    break;
                case JERRY_ERROR_EVAL     : retVal = "EvalError";      break;
                case JERRY_ERROR_RANGE    : retVal = "RangeError";     break;
                case JERRY_ERROR_REFERENCE: retVal = "ReferenceError"; break;
                case JERRY_ERROR_SYNTAX   : retVal = "SyntaxError";    break;
                case JERRY_ERROR_TYPE     : retVal = "TypeError";      break;
                case JERRY_ERROR_URI      : retVal = "URIError";       break;
                case JERRY_ERROR_AGGREGATE: retVal = "AggregateError"; break;
                default:                    retVal = "UnknownError";   break;
            }
        }

        return retVal;
    }

    static string GetExceptionAsString(jerry_value_t exception)
    {
        string retVal;

        UseThenFree(jerry_exception_value(exception, false), [&](auto exceptionValue){
            retVal = GetValueAsString(exceptionValue);
        });

        return retVal;
    }

    static string GetExceptionPropertyAsString(jerry_value_t exception, const string &property)
    {
        string retVal;

        UseThenFree(jerry_exception_value(exception, false), [&](auto exceptionValue){
            retVal = GetPropertyAsString(exceptionValue, property);
        });

        return retVal;
    }

    static string GetExceptionDetails(jerry_value_t exception)
    {
        string retVal;

        if (!jerry_value_is_exception(exception))
        {
            // printf("print_exception_details: The provided value is not an exception.\n");
        }
        else
        {
            // Seems that GetExceptionAsString returns effectively the same thing as GetExceptionErrorType + e.message
            // So pick one.
            //
            // Then include the stack

            if (true)
            {
                // eg "ReferenceError: one is not defined"
                retVal += GetExceptionAsString(exception);
            }
            else
            {
                // eg "ErrorType: ReferenceError"
                printf("ErrorType: %s\n", GetExceptionErrorType(exception));
                // eg "one is not defined"
                printf("e.message: %s\n", GetExceptionPropertyAsString(exception, "message").c_str());
            }

            // eg "<anonymous>:5:5,<anonymous>:10:1"
            string exceptionStr = GetExceptionPropertyAsString(exception, "stack");
            if (exceptionStr != "")
            {
                retVal += "\n";
                retVal += exceptionStr;
            }


            // both undefined
            // printf("e.line: %s\n", GetExceptionPropertyAsString(exception, "line").c_str());
            // printf("e.column: %s\n", GetExceptionPropertyAsString(exception, "column").c_str());
        }

        return retVal;
    }


    ///////////////////////////////////////////////////////////////////////////
    // Utility
    ///////////////////////////////////////////////////////////////////////////

    static uint64_t TimeNow()
    {
        return (uint64_t)jerry_port_current_time();
    }


private:
};


