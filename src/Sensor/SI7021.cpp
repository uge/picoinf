#include "SI7021.h"
#include "Shell.h"
#include "Timeline.h"
#include "Utl.h"

#include <string>
#include <vector>
using namespace std;

#include "StrictMode.h"


void SI7021::SetupShell()
{
    Timeline::Global().Event("SI7021::SetupShell");

    // static I2C::Instance instance = I2C::Instance::I2C0;
    static I2C::Instance  instance = I2C::Instance::I2C1;
    static SI7021        *sensor   = nullptr;

    static auto GetSensor = []{
        if (sensor == nullptr)
        {
            Timeline::Use([](Timeline &t){
                sensor = new SI7021(instance);
            }, "Constructor");
        }

        return sensor;
    };

    static auto UpdateSensor = []{
        if (sensor != nullptr)
        {
            delete sensor;
            sensor = nullptr;
        }

        GetSensor();
    };

    Shell::AddCommand("sensor.si7021.i2c.instance", [](vector<string> argList){
        uint8_t instNum = (uint8_t)atoi(argList[0].c_str());
        Log("Instance set to ", instNum);
        instance = instNum == 0 ? I2C::Instance::I2C0 : I2C::Instance::I2C1;
        UpdateSensor();
    }, { .argCount = 1, .help = "set i2c instance" });

    Shell::AddCommand("sensor.si7021.get.temp", [](vector<string> argList){
        GetSensor();

        double tempC = sensor->GetTemperatureCelsius();
        double tempF = sensor->GetTemperatureFahrenheit();

        Log("TempC: ", tempC);
        Log("TempF: ", tempF);
    }, { .argCount = 0, .help = "get temperature" });

    Shell::AddCommand("sensor.si7021.get.humidity", [](vector<string> argList){
        GetSensor();

            double humPct = sensor->GetHumidityPct();
            Log("humPct: ", Commas(humPct));
    }, { .argCount = 0, .help = "get humidity" });

    Shell::AddCommand("sensor.si7021.get.all", [](vector<string> argList){
        GetSensor();

        Timeline::Use([](auto &t){
            double tempC  = sensor->GetTemperatureCelsius();
            t.Event("tempC");
            double tempF  = sensor->GetTemperatureFahrenheit();
            t.Event("tempF");
            double humPct = sensor->GetHumidityPct();
            t.Event("humPct");

            Log("TempC : ", tempC);
            Log("TempF : ", tempF);
            Log("humPct: ", humPct);
        });
    }, { .argCount = 0, .help = "get all" });
}