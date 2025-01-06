#pragma once

#include "ArduinoSensorCore.h"
#include "I2C.h"


class BMP280
{
public:

    // Address is either 0x76 or 0x77
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

    double GetTemperatureCelsius()
    {
        // sensor returns celsius
        return bmp280_.readTemperature();
    }

    double GetTemperatureFahrenheit()
    {
        return (GetTemperatureCelsius() * (9.0 / 5.0)) + 32;
    }

    double GetPressurehPa()
    {
        // sensor returns Pa
        return bmp280_.readPressure() / 100;
    }

    double GetPressureBar()
    {
        return GetPressurehPa() * 1'000;
    }

    double GetAltitudeMeters()
    {
        // sensor returns meters
        // calibrated to nominal pressure at sea level (hPa)
        return bmp280_.readAltitude(1013.25);
    }

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