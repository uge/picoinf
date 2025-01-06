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
    static BME280        *bme280 = nullptr;

    static auto GetBME280 = []{
        if (bme280 == nullptr)
        {
            bme280 = new BME280(addr, instance);
        }

        return bme280;
    };

    static auto UpdateBME280 = []{
        if (bme280 != nullptr)
        {
            delete bme280;
            bme280 = nullptr;
        }

        GetBME280();
    };

    Shell::AddCommand("sensor.bme280.i2c.addr", [](vector<string> argList){
        addr = (uint8_t)atoi(argList[0].c_str());
        Log("Addr set to ", ToHex(addr));
        UpdateBME280();
    }, { .argCount = 1, .help = "set i2c <hexAddr>" });

    Shell::AddCommand("sensor.bme280.i2c.instance", [](vector<string> argList){
        uint8_t instNum = (uint8_t)atoi(argList[0].c_str());
        Log("Instance set to ", instNum);
        instance = instNum == 0 ? I2C::Instance::I2C0 : I2C::Instance::I2C1;
        UpdateBME280();
    }, { .argCount = 1, .help = "set i2c instance" });

    Shell::AddCommand("sensor.bme280.get.temp", [](vector<string> argList){
        BME280 *bme280 = GetBME280();

        double tempC = bme280->GetTemperatureCelsius();
        double tempF = bme280->GetTemperatureFahrenheit();

        Log("TempC: ", tempC);
        Log("TempF: ", tempF);
    }, { .argCount = 0, .help = "get temperature" });

    Shell::AddCommand("sensor.bme280.get.pressure", [](vector<string> argList){
        BME280 *bme280 = GetBME280();

        double hPa = bme280->GetPressurehPa();
        double bar = bme280->GetPressureBar();

        Log("hPa: ", Commas(hPa));
        Log("bar: ", Commas(bar));
    }, { .argCount = 0, .help = "get pressure" });

    Shell::AddCommand("sensor.bme280.get.altitude", [](vector<string> argList){
        BME280 *bme280 = GetBME280();

        double altM  = bme280->GetAltitudeMeters();
        double altFt = bme280->GetAltitudeFeet();

        Log("altM : ", Commas(altM));
        Log("altFt: ", Commas(altFt));
    }, { .argCount = 0, .help = "get altitude" });

    Shell::AddCommand("sensor.bme280.get.humidity", [](vector<string> argList){
        BME280 *bme280 = GetBME280();

        double humPct = bme280->GetAltitudeMeters();

        Log("humPct: ", Commas(humPct));
    }, { .argCount = 0, .help = "get altitude" });

    Shell::AddCommand("sensor.bme280.get.all", [](vector<string> argList){
        BME280 *bme280 = GetBME280();

        double tempC  = bme280->GetTemperatureCelsius();
        double tempF  = bme280->GetTemperatureFahrenheit();
        double hPa    = bme280->GetPressurehPa();
        double bar    = bme280->GetPressureBar();
        double altM   = bme280->GetAltitudeMeters();
        double altFt  = bme280->GetAltitudeFeet();
        double humPct = bme280->GetAltitudeMeters();

        Log("TempC : ", tempC);
        Log("TempF : ", tempF);
        Log("hPa   : ", Commas(hPa));
        Log("bar   : ", Commas(bar));
        Log("altM  : ", Commas(altM));
        Log("altFt : ", Commas(altFt));
        Log("humPct: ", humPct);
    }, { .argCount = 0, .help = "get all" });
}