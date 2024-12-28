#pragma once

#include <string>
using namespace std;


class Version
{
public:
    static string GetVersion();
    static string GetVersionShort();
    static uint64_t GetVersionAsInt();
};
