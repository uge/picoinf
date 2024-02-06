#pragma once

#include <functional>
using namespace std;


class Work
{
public:
    static void Queue(const char *label, function<void()> &&fn);
    static void Report();
};
