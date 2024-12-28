#pragma once


// https://en.cppreference.com/w/cpp/language/constructor
// https://en.cppreference.com/w/cpp/language/default_constructor
// https://en.cppreference.com/w/cpp/language/initialization
// https://en.cppreference.com/w/cpp/utility/initializer_list
// https://en.cppreference.com/w/cpp/language/operators
// https://en.cppreference.com/w/cpp/language/operator_comparison



class NonMovable
{
protected:
    NonMovable() {}
    ~NonMovable() {}

private:
    NonMovable(const NonMovable &&) = delete;
    NonMovable& operator=(const NonMovable &&) = delete;
};


class NonCopyable
{
protected:
    NonCopyable() {}
    ~NonCopyable() {}

private:
    NonCopyable(const NonCopyable &) = delete;
    NonCopyable& operator=(const NonCopyable &) = delete;
};
