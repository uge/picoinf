#pragma once

#include "ArduinoSensorCore.h"
#include "I2C.h"


class BME280
{
public:

    // Address is either 0x76 or 0x77
    // 120ms
    BME280(uint8_t addr, I2C::Instance instance)
    : tw_(addr, instance)
    {
        // this calls a default setSampling()
        bme280_.begin(addr, &tw_);
    }

    // 1ms
    double GetTemperatureCelsius()
    {
        // sensor returns celsius
        return bme280_.readTemperature();
    }

    // 1ms
    double GetTemperatureFahrenheit()
    {
        return (GetTemperatureCelsius() * (9.0 / 5.0)) + 32;
    }

    // 1ms
    double GetPressureHectopascals()
    {
        // sensor returns Pa
        return bme280_.readPressure() / 100;
    }

    // 1ms
    double GetPressureBars()
    {
        return GetPressureHectopascals() * 1'000;
    }

    // 1ms
    double GetAltitudeMeters()
    {
        // sensor returns meters
        // calibrated to nominal pressure at sea level (hPa)
        return bme280_.readAltitude(1013.25);
    }

    // 1ms
    double GetAltitudeFeet()
    {
        return GetAltitudeMeters() * 3.28084;
    }

    // 1ms
    double GetHumidityPct()
    {
        return bme280_.readHumidity();
    }


public:

    static void SetupShell();

private:

    TwoWire tw_;
    Adafruit_BME280 bme280_;
};