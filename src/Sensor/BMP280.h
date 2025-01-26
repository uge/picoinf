#pragma once

#include "ArduinoSensorCore.h"
#include "I2C.h"


class BMP280
{
public:

    // Address is either 0x76 or 0x77
    // 105ms
    BMP280(uint8_t addr, I2C::Instance instance)
    : tw_(addr, instance)
    , bmp280_(&tw_)
    {
        // startup sequence taken from example ino.
        // "Default settings from datasheet."
        bmp280_.begin();
        bmp280_.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                            Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                            Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                            Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                            Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
    }

    static bool IsValidAddr(uint8_t addr)
    {
        return addr == 0x76 || addr == 0x77;
    }

    bool IsAlive()
    {
        return tw_.GetI2C().IsAlive();
    }

    // 1ms
    double GetTemperatureCelsius()
    {
        // sensor returns celsius
        return bmp280_.readTemperature();
    }

    // 1ms
    double GetTemperatureFahrenheit()
    {
        return (GetTemperatureCelsius() * (9.0 / 5.0)) + 32;
    }

    // 1ms
    double GetPressureHectoPascals()
    {
        // sensor returns Pa
        return bmp280_.readPressure() / 100;
    }

    // 1ms
    double GetPressureMilliBars()
    {
        return GetPressureHectoPascals();
    }

    // 1ms
    double GetAltitudeMeters()
    {
        // sensor returns meters
        // calibrated to nominal pressure at sea level (hPa)
        return bmp280_.readAltitude(1013.25);
    }

    // 1ms
    double GetAltitudeFeet()
    {
        return GetAltitudeMeters() * 3.28084;
    }


public:

    static void SetupShell();

private:

    TwoWire tw_;
    Adafruit_BMP280 bmp280_;
};