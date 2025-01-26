#include "BME280.h"
#include "Shell.h"
#include "Timeline.h"
#include "Utl.h"

#include <string>
#include <vector>
using namespace std;

#include "StrictMode.h"


void BME280::SetupShell()
{
    Timeline::Global().Event("BME280::SetupShell");

    static uint8_t       addr     = 0x76;
    // static I2C::Instance instance = I2C::Instance::I2C0;
    static I2C::Instance instance = I2C::Instance::I2C1;
    static BME280        *sensor = nullptr;

    static auto GetSensor = []{
        if (sensor == nullptr)
        {
            Timeline::Measure([](Timeline &t){
                sensor = new BME280(addr, instance);
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

    Shell::AddCommand("sensor.bme280.i2c.addr", [](vector<string> argList){
        addr = (uint8_t)atoi(argList[0].c_str());
        Log("Addr set to ", ToHex(addr));
        UpdateSensor();
    }, { .argCount = 1, .help = "set i2c <hexAddr>" });

    Shell::AddCommand("sensor.bme280.i2c.instance", [](vector<string> argList){
        uint8_t instNum = (uint8_t)atoi(argList[0].c_str());
        Log("Instance set to ", instNum);
        instance = instNum == 0 ? I2C::Instance::I2C0 : I2C::Instance::I2C1;
        UpdateSensor();
    }, { .argCount = 1, .help = "set i2c instance" });

    Shell::AddCommand("sensor.bme280.get.temp", [](vector<string> argList){
        GetSensor();

        double tempC = sensor->GetTemperatureCelsius();
        double tempF = sensor->GetTemperatureFahrenheit();

        Log("TempC: ", tempC);
        Log("TempF: ", tempF);
    }, { .argCount = 0, .help = "get temperature" });

    Shell::AddCommand("sensor.bme280.get.pressure", [](vector<string> argList){
        GetSensor();

        double hPa = sensor->GetPressureHectoPascals();
        double mb  = sensor->GetPressureMilliBars();

        Log("hPa: ", Commas(hPa));
        Log("mb : ", Commas(mb));
    }, { .argCount = 0, .help = "get pressure" });

    Shell::AddCommand("sensor.bme280.get.altitude", [](vector<string> argList){
        GetSensor();

        double altM  = sensor->GetAltitudeMeters();
        double altFt = sensor->GetAltitudeFeet();

        Log("altM : ", Commas(altM));
        Log("altFt: ", Commas(altFt));
    }, { .argCount = 0, .help = "get altitude" });

    Shell::AddCommand("sensor.bme280.get.humidity", [](vector<string> argList){
        GetSensor();

        double humPct = sensor->GetHumidityPct();

        Log("humPct: ", Commas(humPct));
    }, { .argCount = 0, .help = "get altitude" });

    Shell::AddCommand("sensor.bme280.get.all", [](vector<string> argList){
        GetSensor();

        Timeline::Measure([](auto &t){
            double tempC  = sensor->GetTemperatureCelsius();
            t.Event("tempC");
            double tempF  = sensor->GetTemperatureFahrenheit();
            t.Event("tempF");
            double hPa    = sensor->GetPressureHectoPascals();
            t.Event("hPa");
            double mb     = sensor->GetPressureMilliBars();
            t.Event("mb");
            double altM   = sensor->GetAltitudeMeters();
            t.Event("altM");
            double altFt  = sensor->GetAltitudeFeet();
            t.Event("altFt");
            double humPct = sensor->GetHumidityPct();
            t.Event("humPct");

            Log("TempC : ", tempC);
            Log("TempF : ", tempF);
            Log("hPa   : ", Commas(hPa));
            Log("mb    : ", Commas(mb));
            Log("altM  : ", Commas(altM));
            Log("altFt : ", Commas(altFt));
            Log("humPct: ", humPct);
        });
    }, { .argCount = 0, .help = "get all" });
}