#pragma once

#include <functional>


extern void WorkSetupShell();

class Work
{
public:
    static void Queue(const char *label, std::function<void()> &&fn);
    static void Report();
};
