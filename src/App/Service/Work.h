#pragma once

#include <functional>


class Work
{
public:
    static void Queue(const char *label, std::function<void()> &&fn);
    static void Report();

    static void SetupShell();
};
