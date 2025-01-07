#pragma once

#include "ArduinoSensorCore.h"
#include "I2C.h"


class MMC56x3
{
public:

    // Address is either 0x30
    MMC56x3(uint8_t addr, I2C::Instance instance)
    : tw_(addr, instance)
    {
        sensor_.begin(addr, &tw_);
    }

    // 11ms
    double GetMagXMicroTesla()
    {
        sensors_event_t e;
        sensor_.getEvent(&e);

        return e.magnetic.x;
    }

    // 11ms
    double GetMagYMicroTesla()
    {
        sensors_event_t e;
        sensor_.getEvent(&e);

        return e.magnetic.y;
    }

    // 11ms
    double GetMagZMicroTesla()
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