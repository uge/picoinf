#pragma once

#include <string>


class Version
{
public:
    static std::string GetVersion();
    static std::string GetVersionShort();
    static uint64_t GetVersionAsInt();

    static void SetupShell();
};
