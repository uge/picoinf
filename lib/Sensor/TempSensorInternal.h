#pragma once

#include "ADCInternal.h"


class TempSensorInternal
{
public:
    static double GetTempC()
    {
        uint16_t raw = ADC::ReadTemperature();

        double mv = raw * 3.3f / 4096;

        double tempC = 27.0f - (mv - 0.706f) / 0.001721f;

        return tempC;
    }

    static double GetTempF()
    {
        return (GetTempC() * 9.0 / 5.0) + 32.0;
    }
};

