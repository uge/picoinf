#pragma once

#include "BME280.h"
#include "BMP280.h"
#include "Timeline.h"


class Sensor
{
public:

    static void SetupShell()
    {
        Timeline::Global().Event("Sensor::SetupShell");

        BME280::SetupShell();
        BMP280::SetupShell();
    }
};