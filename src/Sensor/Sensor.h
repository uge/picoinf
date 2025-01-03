#pragma once

#include "BMP280.h"
#include "Timeline.h"


class Sensor
{
public:

    static void SetupShell()
    {
        Timeline::Global().Event("Sensor::SetupShell");

        BMP280::SetupShell();
    }
};