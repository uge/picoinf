#pragma once

#include "Log.h"
#include "ZephyrSensor.h"

// drivers/sensor/rpi_pico_temp/rpi_pico_temp.c

class TempSensorInternal
{
public:
    static double GetTempC()
    {
        ZephyrSensor sensor("dietemp");

        double tempC = sensor.FetchAndGet(SENSOR_CHAN_DIE_TEMP);

        return tempC;
    }

    static double GetTempF()
    {
        return (GetTempC() * 9.0 / 5.0) + 32.0;
    }
};

