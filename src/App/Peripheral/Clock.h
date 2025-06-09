#pragma once


class Clock
{
public:
    static void SetClockMHz(double mhz,
                            bool   lowPowerPriority = false,
                            bool   mustBeExact      = false);
    static void EnableUSB();
    static void DisableUSB(); // saves 4mA
    static bool IsEnabledUSB();
    static void PrepareClockMHz(double mhz, bool lowPowerPriority = false, bool mustBeExact = false);
    static void PrintAll();
    static void SetVerbose(bool verbose);

    static void Init();
    static void SetupShell();
};

