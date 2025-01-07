#pragma once

#include "ArduinoSensorCore.h"
#include "I2C.h"


class SI7021
{
public:

    // Address is only ever 0x40
    // 50ms
    SI7021(I2C::Instance instance)
    : tw_(0x40, instance)
    , sensor_(&tw_)
    {
        sensor_.begin();
    }

    // 20ms
    double GetTemperatureCelsius()
    {
        // sensor returns celsius
        return sensor_.readTemperature();
    }

    // 20ms
    double GetTemperatureFahrenheit()
    {
        return (GetTemperatureCelsius() * (9.0 / 5.0)) + 32;
    }

    // 20ms
    double GetHumidityPct()
    {
        return sensor_.readHumidity();
    }


public:

    static void SetupShell();

private:

    TwoWire tw_;
    Adafruit_Si7021 sensor_;
};