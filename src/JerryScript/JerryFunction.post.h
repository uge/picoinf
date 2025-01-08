#pragma once


template <typename R, typename... Args>
R JerryFunction<R(Args...)>::Handler(Args... args)
{
    // get the "extra" internal property
    double ptrVal = JerryScript::GetNativeFunctionState().extra;

    // convert to 'this' object
    JerryFunction<R(Args...)> *obj = (JerryFunction<R(Args...)> *)(uint32_t)ptrVal;

    // invoke, maybe return a value
    if constexpr (std::is_void_v<R>) {
        if (obj)
        {
            obj->fn_(std::forward<Args>(args)...);
        }
    } else {
        if (obj)
        {
            return obj->fn_(std::forward<Args>(args)...);
        }
        else
        {
            return R{};
        }
    }
}
