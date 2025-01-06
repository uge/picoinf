#pragma once

#include "BH1750.h"
#include "BME280.h"
#include "BMP280.h"
#include "SI7021.h"
#include "Timeline.h"


class Sensor
{
public:

    static void SetupShell()
    {
        Timeline::Global().Event("Sensor::SetupShell");

        BH1750::SetupShell();
        BME280::SetupShell();
        BMP280::SetupShell();
        SI7021::SetupShell();
    }
};