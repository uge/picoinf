#pragma once

#include "ArduinoSensorCore.h"
#include "I2C.h"
#include "PAL.h"


class BH1750
{
public:

    // Address is either 0x23 or 0x5C
    BH1750(uint8_t addr, I2C::Instance instance)
    : tw_(addr, instance)
    , sensor_(addr, &tw_)
    {
        sensor_.powerOn();
    }

    // 200ms
    double GetLuxLowRes()
    {
        sensor_.setOnceLowRes();
        PAL.DelayBusy(200);
        return sensor_.getLux();
    }

    // 200ms
    double GetLuxHighRes()
    {
        sensor_.setOnceHighRes();
        PAL.DelayBusy(200);
        return sensor_.getLux();
    }

    // 200ms
    double GetLuxHigh2Res()
    {
        sensor_.setOnceHigh2Res();
        PAL.DelayBusy(200);
        return sensor_.getLux();
    }


public:

    static void SetupShell();

private:

    TwoWire tw_;
    BH1750FVI sensor_;
};