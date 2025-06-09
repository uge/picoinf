#pragma once

#include "ArduinoSensorCore.h"
#include "I2C.h"


class MMC56x3
{
public:

    // Address is 0x30
    // 24 ms
    MMC56x3(I2C::Instance instance)
    : tw_(0x30, instance)
    {
        sensor_.begin(0x30, &tw_);
    }

    static uint8_t GetAddr()
    {
        return 0x30;
    }

    bool IsAlive()
    {
        return tw_.GetI2C().IsAlive();
    }

    // 11ms
    double GetMagXMicroTeslas()
    {
        sensors_event_t e;
        sensor_.getEvent(&e);

        return e.magnetic.x;
    }

    // 11ms
    double GetMagYMicroTeslas()
    {
        sensors_event_t e;
        sensor_.getEvent(&e);

        return e.magnetic.y;
    }

    // 11ms
    double GetMagZMicroTeslas()
    {
        sensors_event_t e;
        sensor_.getEvent(&e);

        return e.magnetic.z;
    }


public:

    static void SetupShell();

private:

    TwoWire tw_;
    Adafruit_MMC5603 sensor_;
};