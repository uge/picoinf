#pragma once

#include "ClassTypes.h"
#include "JerryFunction.pre.h"
#include "JerryScriptPORT.h"
#include "Log.h"
#include "Utl.h"

#include "jerryscript.h"
#include "jerryscript-port.h"
#include "jerryscript-ext/handlers.h"
#include "jerryscript-ext/properties.h"

#include <functional>
#include <string>
#include <utility>
using namespace std;



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

//
// Guide to when to use which technique:
// - JSObj_XXX
//   - JavaScript may create/destroy instances on a whim
//   - Supplying-code registers this handler to deal with that
//     - code can still use 'name' property of function to group handlers
//
// - JSProxy_XXX
//   - JavaScript is given access to a fixed quantity of objects
//   - Supplying-code does a proxy-per-object
//     - code can still use 'name' property of function to group handlers
//
// - BareFunction
//   - Supplying code exposes functionality which is sufficiently
//     in scope that no object oriented approach is needed
//

class JerryScript
{
public:

    ///////////////////////////////////////////////////////////////////////////
    // Runtime management
    ///////////////////////////////////////////////////////////////////////////

    static void UseVM(function<void()> fn)
    {
        // init
        uint64_t timePreInit = TimeNow();
        jerry_init(JERRY_INIT_MEM_STATS);
        uint64_t timePostInit = TimeNow();

        // enable printing
        jerryx_register_global ("Log", jerryx_handler_print);

        // user code
        uint64_t timePreCallback = TimeNow();
        fnPreRunHook_();
        fn();
        uint64_t timePostCallback = TimeNow();

        {
            jerry_heap_stats_t stats = { 0 };
            bool get_stats_ret = jerry_heap_stats(&stats);

            if (get_stats_ret)
            {
                // capture
                vmHeapCapacity_ = stats.size;
                vmHeapSizeMax_  = stats.peak_allocated_bytes;

                // debug report
                Log("Heap Stats:\n");
                Log("Size                : ", Commas(vmHeapCapacity_));
                Log("Allocated Bytes     : ", Commas(stats.allocated_bytes));
                Log("Peak Allocated Bytes: ", Commas(vmHeapSizeMax_));
            }



            // When you enable the following, and have set(JERRY_MEM_STATS ON),
            // then you get output like this:
            //
            // Heap stats:
            // Heap size = 16376 bytes
            // Allocated = 0 bytes
            // Peak allocated = 10792 bytes
            // Waste = 0 bytes
            // Peak waste = 569 bytes
            // Allocated byte code data = 0 bytes
            // Peak allocated byte code data = 80 bytes
            // Allocated string data = 0 bytes
            // Peak allocated string data = 3341 bytes
            // Allocated object data = 0 bytes
            // Peak allocated object data = 3340 bytes
            // Allocated property data = 0 bytes
            // Peak allocated property data = 3104 bytes
            // jerry_log_set_level(JERRY_LOG_LEVEL_DEBUG);
        }

        // cleanup
        uint64_t timePreCleanup = TimeNow();
        jerry_cleanup();
        uint64_t timePostCleanup = TimeNow();

        // calculate stats
        vmDurationMs_         = timePostCleanup - timePreInit;
        vmOverheadDurationMs_ = (timePostInit - timePreInit) + (timePostCleanup - timePreCleanup);
        vmCallbackDurationMs_ = timePostCallback - timePreCallback;
    }

    static string ParseAndRunScript(const string &script, uint64_t timeoutMs = 0)
    {
        string err;
        jerry_value_t parsedCode = jerry_undefined();

        err = ParseScript(script, &parsedCode);
        UseThenFree(parsedCode, [&](auto parsedCode){
            if (err == "")
            {
                err = RunParsedCode(parsedCode, timeoutMs);
            }
        });

        return err;
    }

    // parses script and returns any exception or error in the return value.
    // parsed code is automatically freed.
    // if a parsedCodeRet value is passed, a reference is returned, and must later be freed.
    static string ParseScript(const string &script, jerry_value_t *parsedCodeRet = nullptr)
    {
        string err;

        jerry_parse_options_t options;
        options.options = JERRY_PARSE_STRICT_MODE;

        uint64_t timeStart = TimeNow();
        jerry_value_t parsedCode = jerry_parse((const jerry_char_t *)script.c_str(), script.size(), &options);
        scriptParseDurationMs_ = TimeNow() - timeStart;

        UseThenFree(parsedCode, [&](auto parsedCode){
            if (parsedCodeRet)
            {
                *parsedCodeRet = jerry_value_copy(parsedCode);
            }

            if (jerry_value_is_exception(parsedCode))
            {
                err += "[Script parse exception]";
                err += "\n";
                err += GetExceptionDetails(parsedCode);
            }
        });

        return err;
    }

    static string RunParsedCode(jerry_value_t parsedCode, uint64_t timeoutMs = 0)
    {
        string err;

        JerryScriptPORT::ClearOutputBuffer();
        SetExecutionTimeoutMs(timeoutMs);

        uint64_t timeStart = TimeNow();
        UseThenFree(jerry_run(parsedCode), [&](auto result){
            scriptRunDurationMs_ = TimeNow() - timeStart;

            if (jerry_value_is_exception(result))
            {
                err += "[Runtime exception]";
                err += "\n";
                err += GetExceptionDetails(result);
            }
        });

        return err;
    }

    static void SetPreRunHook(function<void()> fn)
    {
        fnPreRunHook_ = fn;
    }

    static string GetScriptOutput()
    {
        return JerryScriptPORT::GetOutputBuffer();
    }

    static uint32_t GetHeapCapacity()
    {
        return vmHeapCapacity_;
    }

    static uint32_t GetHeapSizeMax()
    {
        return vmHeapSizeMax_;
    }

    static uint64_t GetVMDurationMs()
    {
        return vmDurationMs_;
    }

    static uint64_t GetVMOverheadDurationMs()
    {
        return vmOverheadDurationMs_;
    }

    static uint64_t GetVMCallbackDurationMs()
    {
        return vmCallbackDurationMs_;
    }

    static uint64_t GetScriptParseDurationMs()
    {
        return scriptParseDurationMs_;
    }

    static uint64_t GetScriptRunDurationMs()
    {
        return scriptRunDurationMs_;
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

    static void UseThenFree(jerry_value_t value, function<void(jerry_value_t value)> &&fnUse)
    {
        fnUse(value);
        jerry_value_free(value);
    }

    static void UseThenFreeNewObj(function<void(jerry_value_t value)> &&fnUse)
    {
        UseThenFree(jerry_object(), forward<function<void(jerry_value_t value)> &&>(fnUse));
    }


    ///////////////////////////////////////////////////////////////////////////
    // Global Property getters and setters
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


    ///////////////////////////////////////////////////////////////////////////
    // Global Properties as Native Functions
    ///////////////////////////////////////////////////////////////////////////

    // No particular implementation, simply forwards to the non-global function
    // using the global object as target
    template <typename T>
    static void SetGlobalPropertyToNativeFunction(const string &name, T fn, void *extra = nullptr)
    {
        UseThenFree(jerry_current_realm(), [&](auto globalObj){
            SetPropertyToNativeFunction(globalObj, name, fn, extra);
        });
    }


    ///////////////////////////////////////////////////////////////////////////
    // Global Properties as Jerry Functions
    ///////////////////////////////////////////////////////////////////////////

    template <typename R, typename... Args>
    static void SetGlobalPropertyToJerryFunction(const string &name, JerryFunction<R(Args...)> &jerryFn)
    {
        R (*handlerFn)(Args...) = &JerryFunction<R(Args...)>::Handler;

        UseThenFree(jerry_current_realm(), [&](auto globalObj){
            SetPropertyToNativeFunction(globalObj, name, handlerFn, &jerryFn);
        });
    }


    ///////////////////////////////////////////////////////////////////////////
    // Property getters and setters
    ///////////////////////////////////////////////////////////////////////////

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
    // Properties as Jerry Native Functions
    ///////////////////////////////////////////////////////////////////////////

    // set an object's property to be a function object.
    // automatically also:
    // - tag the function object with its own name as an internal property
    // - associate native pointer data with nullptr type
    //   - same as property getter/setter
    static void SetPropertyToJerryNativeFunction(jerry_value_t             objTarget,
                                                 const string             &propertyName,
                                                 jerry_external_handler_t  handler,
                                                 void                     *data = nullptr,
                                                 void                     *extra = nullptr)
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

        if (extra)
        {
            SetInternalPropertyToNumber(handlerFn, "extra", (double)(uint32_t)extra);
        }

        // release function object
        jerry_value_free(handlerFn);
    }

    // Here it is possible to use a bare function (one that is not aware of jerry parameters).
    //
    // This is accomplished by having a dedicated handler function which knows how to
    // call the underlying bare function.
    //
    // The bare function is passed no context, so this allows the 'native' pointer to be
    // used to store the actual function pointer to be invoked.
    //
    // The tradeoffs here are:
    // - callers get simpler functions but with no context
    // - this code requires one handler-per-type, so more work to extend (oh well)
    //
    // This allows more instances where a non-capturing lambda can be used as
    // the function.


    // Give native functions a way to access state since they won't have
    // access to the function object to look it up themselves

private:
    struct NativeFunctionState
    : public NonCopyable
    , public NonMovable
    {
        jerry_value_t retVal;

        double extra;
    };
    static inline NativeFunctionState nativeFunctionState_;

    static NativeFunctionState &SetupNativeFunctionState(const jerry_call_info_t *callInfo)
    {
        nativeFunctionState_.retVal = jerry_undefined();
        nativeFunctionState_.extra  = GetInternalPropertyAsNumber(callInfo->function, "extra");

        return nativeFunctionState_;
    }
public:

    static NativeFunctionState &GetNativeFunctionState()
    {
        return nativeFunctionState_;
    }


    // void()
    static void SetPropertyToNativeFunction(jerry_value_t objTarget, const string &name, void (*fn)(), void *extra = nullptr)
    {
        SetPropertyToJerryNativeFunction(objTarget, name, FnVoidVoidHandler, (void *)fn, extra);
    }

    static jerry_value_t FnVoidVoidHandler(const jerry_call_info_t *callInfo,
                                           const jerry_value_t      argv[],
                                           const jerry_length_t     argc)
    {
        jerry_value_t retVal = jerry_undefined();

        if (argc != 0)
        {
            retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Invalid function arguments");
        }
        else
        {
            void (*fn)() = (void (*)())JerryScript::GetNativePointer(callInfo->function);

            if (fn)
            {
                auto &state = SetupNativeFunctionState(callInfo);
                fn();
                retVal = state.retVal;
            }
            else
            {
                retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Native pointer not set");
            }
        }

        return retVal;
    }

    // void(uint32_t)
    static void SetPropertyToNativeFunction(jerry_value_t objTarget, const string &name, void (*fn)(uint32_t), void *extra = nullptr)
    {
        SetPropertyToJerryNativeFunction(objTarget, name, FnVoidU32Handler, (void *)fn, extra);
    }

    static jerry_value_t FnVoidU32Handler(const jerry_call_info_t *callInfo,
                                          const jerry_value_t      argv[],
                                          const jerry_length_t     argc)
    {
        jerry_value_t retVal = jerry_undefined();

        if (argc != 1 || !jerry_value_is_number(argv[0]))
        {
            retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Invalid function arguments");
        }
        else
        {
            uint32_t arg = (uint32_t)jerry_value_as_number(argv[0]);

            void (*fn)(uint32_t) = (void (*)(uint32_t))JerryScript::GetNativePointer(callInfo->function);

            if (fn)
            {
                auto &state = SetupNativeFunctionState(callInfo);
                fn(arg);
                retVal = state.retVal;
            }
            else
            {
                retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Native pointer not set");
            }
        }

        return retVal;
    }

    // void(uint64_t)
    static void SetPropertyToNativeFunction(jerry_value_t objTarget, const string &name, void (*fn)(uint64_t), void *extra = nullptr)
    {
        SetPropertyToJerryNativeFunction(objTarget, name, FnVoidU64Handler, (void *)fn, extra);
    }

    static jerry_value_t FnVoidU64Handler(const jerry_call_info_t *callInfo,
                                          const jerry_value_t      argv[],
                                          const jerry_length_t     argc)
    {
        jerry_value_t retVal = jerry_undefined();

        if (argc != 1 || !jerry_value_is_number(argv[0]))
        {
            retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Invalid function arguments");
        }
        else
        {
            uint64_t arg = (uint64_t)jerry_value_as_number(argv[0]);

            void (*fn)(uint64_t) = (void (*)(uint64_t))JerryScript::GetNativePointer(callInfo->function);

            if (fn)
            {
                auto &state = SetupNativeFunctionState(callInfo);
                fn(arg);
                retVal = state.retVal;
            }
            else
            {
                retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Native pointer not set");
            }
        }

        return retVal;
    }

    // double(void)
    static void SetPropertyToNativeFunction(jerry_value_t objTarget, const string &name, double (*fn)(), void *extra = nullptr)
    {
        SetPropertyToJerryNativeFunction(objTarget, name, FnDoubleVoidHandler, (void *)fn, extra);
    }

    static jerry_value_t FnDoubleVoidHandler(const jerry_call_info_t *callInfo,
                                             const jerry_value_t      argv[],
                                             const jerry_length_t     argc)
    {
        jerry_value_t retVal = jerry_undefined();

        if (argc != 0)
        {
            retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Invalid function arguments");
        }
        else
        {
            double (*fn)() = (double (*)())JerryScript::GetNativePointer(callInfo->function);

            if (fn)
            {
                auto &state = SetupNativeFunctionState(callInfo);
                retVal = UseFirstDefinedFreeRemaining(state.retVal, jerry_number(fn()));
            }
            else
            {
                retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Native pointer not set");
            }
        }

        return retVal;
    }

    ///////////////////////////////////////////////////////////////////////////
    // Properties as Jerry Functions
    ///////////////////////////////////////////////////////////////////////////

    template <typename R, typename... Args>
    static void SetPropertyToJerryFunction(jerry_value_t objTarget, const string &name, JerryFunction<R(Args...)> &jerryFn)
    {
        R (*handlerFn)(Args...) = &JerryFunction<R(Args...)>::Handler;

        SetPropertyToNativeFunction(objTarget, name, handlerFn, &jerryFn);
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

    static void SetInternalPropertyToNumber(jerry_value_t objTarget, const string &propertyName, double propertyValue)
    {
        SetInternalProperty(objTarget, propertyName, jerry_number(propertyValue));
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

    static double GetInternalPropertyAsNumber(jerry_value_t obj, const string &propertyName)
    {
        double retVal = 0;

        GetInternalProperty(obj, propertyName, [&](jerry_value_t val){
            retVal = jerry_value_as_number(val);
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
        #if 0
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
        #endif
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
                string exceptionStr = GetExceptionAsString(exception);
                retVal += exceptionStr;
            }
            else
            {
                // eg "ErrorType: ReferenceError"
                Log("ErrorType: ", GetExceptionErrorType(exception));
                // eg "one is not defined"
                Log("e.message: ", GetExceptionPropertyAsString(exception, "message").c_str());
            }

            // eg "<anonymous>:5:5,<anonymous>:10:1"
            string stackStr = GetExceptionPropertyAsString(exception, "stack");
            if (stackStr != "")
            {
                retVal += "\n";
                retVal += stackStr;
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

    // return the first value which isn't undefined.
    // return undefined if both undefined.
    // if the not-returned value has a value, free it.
    static jerry_value_t UseFirstDefinedFreeRemaining(jerry_value_t val1, jerry_value_t val2)
    {
        jerry_value_t retVal = jerry_undefined();

        bool freeVal1 = true;
        bool freeVal2 = true;

        if (jerry_value_is_undefined(val1) == false)
        {
            retVal = val1;

            freeVal1 = false;
        }
        else if (jerry_value_is_undefined(val2) == false)
        {
            retVal = val2;

            freeVal2 = false;
        }

        if (freeVal1 && jerry_value_is_undefined(val1) == false)
        {
            jerry_value_free(val1);
        }

        if (freeVal2 && jerry_value_is_undefined(val2) == false)
        {
            jerry_value_free(val2);
        }

        return retVal;
    }


private:

    inline static function<void()> fnPreRunHook_ = []{};

    inline static uint32_t vmHeapCapacity_ = 0;
    inline static uint32_t vmHeapSizeMax_  = 0;

    inline static uint64_t vmDurationMs_ = 0;
    inline static uint64_t vmOverheadDurationMs_ = 0;
    inline static uint64_t vmCallbackDurationMs_ = 0;
    inline static uint64_t scriptParseDurationMs_ = 0;
    inline static uint64_t scriptRunDurationMs_ = 0;
};


#include "JerryFunction.post.h"