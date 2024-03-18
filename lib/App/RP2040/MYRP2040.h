#pragma once

/*

This class represents all the pico-sdk functionality that I shouldn't
be using in a portable implementation.

*/

#include <stdint.h>

#include "RP2040_Clock.h"
#include "RP2040_Peripheral.h"


class RP2040
{
public:

    using Clock = RP2040_Clock;
    using Peripheral = RP2040_Peripheral;

public:
    static void DebugPrint();

    static void Init();
    static void SetupShell();

    static void InitUart1();
};

