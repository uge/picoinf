#pragma once

#include "ArduinoSensorCore.h"
#include "I2C.h"


class BMP280
{
public:

    // Address is either 0x76 or 0x77
    BMP280(uint8_t addr, I2C::Instance instance);
    double GetTemperatureCelsius();
    double GetTemperatureFahrenheit();
    double GetPressurehPa();
    double GetPressureBar();
    double GetAltitudeMeters();
    double GetAltitudeFeet();


public:

    static void SetupShell();

private:

    TwoWire tw_;
    Adafruit_BMP280 bmp280_;
};