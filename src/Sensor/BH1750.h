#pragma once

#include "ArduinoSensorCore.h"
#include "I2C.h"
#include "PAL.h"


class BH1750
{
public:

    // Address is either 0x23 or 0x5C
    // 1ms
    BH1750(uint8_t addr, I2C::Instance instance)
    : tw_(addr, instance)
    , sensor_(addr, &tw_)
    {
        sensor_.powerOn();
    }

    static bool IsValidAddr(uint8_t addr)
    {
        return addr == 0x23 || addr == 0x5C;
    }

    bool IsAlive()
    {
        return tw_.GetI2C().IsAlive();
    }

    // default sensor temperature is 20C / 68F
    // 0 ms - internal calculation
    void SetTemperatureCelsius(int temp = 20)
    {
        sensor_.setTemperature(temp);
    }

    // default sensor temperature is 20C / 68F
    // 0 ms - internal calculation
    void SetTemperatureFahrenheit(int temp = 68)
    {
        int tempC = (int)round(((double)temp - 32.0) * (5.0 / 9.0));

        sensor_.setTemperature(tempC);
    }

    // 4.0 lux resolution
    // 40ms safety over 24ms datasheet max
    double GetLuxLowRes()
    {
        sensor_.setOnceLowRes();
        PAL.Delay(40);
        return sensor_.getLux();
    }

    // 1.0 lux resolution
    // 200ms safety over 180ms datasheet max
    double GetLuxHighRes()
    {
        sensor_.setOnceHighRes();
        PAL.Delay(200);
        return sensor_.getLux();
    }

    // 0.5 lux resolution
    // 200ms safety over 180ms datasheet max
    double GetLuxHigh2Res()
    {
        sensor_.setOnceHigh2Res();
        PAL.Delay(200);
        return sensor_.getLux();
    }


public:

    static void SetupShell();

private:

    TwoWire tw_;
    BH1750FVI sensor_;
};