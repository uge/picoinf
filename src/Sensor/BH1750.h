#pragma once

#include "ArduinoSensorCore.h"
#include "I2C.h"


// hmm tried a few libs and always get basically the same value from the sensor
// even when shining a light on it. I think bad modules. buying more, will
// update.


class BH1750
{
public:

    // Address is either 0x23 or 0x5C
    BH1750(uint8_t addr, I2C::Instance instance)
    : tw_(addr, instance)
    , sensor_(addr, &tw_)
    {
    }

    double GetLux()
    {
        double val = 0;

        sensor_.powerOn();
        sensor_.setContLowRes();

        sensor_.setOnceLowRes();
        delay(200);
        val = sensor_.getLux();
        Log("LowRes  : ", val);
        Log("Err     : ", sensor_.getError());

        sensor_.setOnceHighRes();
        delay(200);
        val = sensor_.getLux();
        Log("HighRes : ", val);
        Log("Err     : ", sensor_.getError());

        sensor_.setOnceHigh2Res();
        delay(200);
        val = sensor_.getLux();
        Log("High2Res: ", val);
        Log("Err     : ", sensor_.getError());

        sensor_.setOnceHighRes();
        delay(200);
        val = sensor_.getRaw();
        Log("RawRes  : ", val);
        Log("Err     : ", sensor_.getError());

        return val;
    }


public:

    static void SetupShell();

private:

    TwoWire tw_;
    BH1750FVI sensor_;
};