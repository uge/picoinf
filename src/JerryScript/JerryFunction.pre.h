#pragma once


// base template
template<typename Signature>
class JerryFunction;

// specialization
template <typename R, typename... Args>
class JerryFunction<R(Args...)>
{
public:

    // capture std::function
    JerryFunction(function<R(Args...)> fn)
    : fn_(fn)
    {
        // nothing to do
    }

    // Constructor for function pointers (and non-capturing lambdas?)
    JerryFunction(R(*fn)(Args...))
    : fn_(fn)
    {
        // nothing to do
    }

    // Constructor for capturing lambdas
    template <typename Lambda>
    JerryFunction(Lambda&& fn)
    : fn_(fn)
    {
        // nothing to do
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

    // forward declaration of handler
    static R Handler(Args... args);


private:

    function<R(Args...)> fn_ = [](Args...) -> R {};
};
