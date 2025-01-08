#pragma once

#include "Log.h"

#include <functional>
#include <string>
using namespace std;


// base template
template<typename Signature>
class JerryFunction;

// specialization
template <typename R, typename... Args>
class JerryFunction<R(Args...)>
{
public:

    // capture std::function
    JerryFunction(string name, function<R(Args...)> fn)
    : name_(name)
    , fn_(fn)
    {
        Log("Testing ", name_, " - std::function - ", this);


        // auto ptr = fn.target<R(*)(Args...)>();

        // not sure what this extracted, but it is a function pointer
        // R (function<R(Args...)>::*ptr)(Args...) const = &function<R(Args...)>::operator();


        // get local class handler for this type
        R (*ptr)(Args...) = &JerryFunction<R(Args...)>::Handler;

        // Register that handler function as a bare function.
        // This will automatically associate the "name" to the function pointer through
        // the "internal property" value.
        // We also pass along 'this' as the "extra" internal property value.
        // JerryScript::SetGlobalPropertyToNativeFunction(name_, ptr, this);
    }


    // Constructor for function pointers (and non-capturing lambdas?)
    JerryFunction(string name, R(*fn)(Args...))
    : name_(name)
    , fn_(fn)
    {
        Log("Testing ", name_, " - bare pointer - ", this);

        // can just forward directly, whatever matches this is a bare function
        // so get rid of storing it as fn_
        // JerryScript::SetGlobalPropertyToNativeFunction(name_, fn, this);
    }


    template <typename Lambda>
    JerryFunction(string name, Lambda&& fn)
    : name_(name)
    , fn_(fn)
    {
        Log("Testing ", name_, " - lambda - ", this);

        // get local class handler for this type
        R (*ptr)(Args...) = &JerryFunction<R(Args...)>::Handler;

        // Register that handler function as a bare function.
        // This will automatically associate the "name" to the function pointer through
        // the "internal property" value.
        // We also pass along 'this' as the "extra" internal property value.
        // JerryScript::SetGlobalPropertyToNativeFunction(name_, ptr, this);
    }




    static R Handler(Args... args)
    {
        // get the "extra" internal property
        double ptrVal = JerryScript::GetNativeFunctionExtra();
        // double ptrVal = 0;

        // convert to 'this' object
        JerryFunction<R(Args...)> *obj = (JerryFunction<R(Args...)> *)(uint32_t)ptrVal;

        Log("Handling for addr ", obj);

        // Check if return type is void
        if constexpr (std::is_void_v<R>) {
            Log("Is void");
            // If return type is void, we don't return anything, just invoke the function
            if (obj)
            {
                Log("Obj ok");
                obj->fn_(std::forward<Args>(args)...);
            }
            else
            {
                Log("Obj not ok");
            }
        } else {
            Log("Not void");
            // If return type is not void, we return the result of the function call
            if (obj)
            {
                Log("Obj ok");
                return obj->fn_(std::forward<Args>(args)...);
            }
            else
            {
                Log("Obj not ok");
                return R{};
            }
        }
    }


    R operator()(Args&&... args)
    {
        if constexpr (std::is_void_v<R>)
        {
            fn_(std::forward<Args>(args)...);
        }
        else
        {
            return fn_(std::forward<Args>(args)...);
        }
    }


private:

    string name_;
    function<R(Args...)> fn_ = [](Args...) -> R {};
};



