#include "MMC56x3.h"
#include "Shell.h"
#include "Timeline.h"
#include "Utl.h"

#include <string>
#include <vector>
using namespace std;

#include "StrictMode.h"


void MMC56x3::SetupShell()
{
    Timeline::Global().Event("MMC56x3::SetupShell");

    static uint8_t        addr     = 0x30;
    // static I2C::Instance instance = I2C::Instance::I2C0;
    static I2C::Instance  instance = I2C::Instance::I2C1;
    static MMC56x3       *sensor   = nullptr;

    static auto GetSensor = []{
        if (sensor == nullptr)
        {
            sensor = new MMC56x3(addr, instance);
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

    Shell::AddCommand("sensor.mmc56x3.i2c.instance", [](vector<string> argList){
        uint8_t instNum = (uint8_t)atoi(argList[0].c_str());
        Log("Instance set to ", instNum);
        instance = instNum == 0 ? I2C::Instance::I2C0 : I2C::Instance::I2C1;
        UpdateSensor();
    }, { .argCount = 1, .help = "set i2c instance" });

    Shell::AddCommand("sensor.mmc56x3.get.x", [](vector<string> argList){
        auto *sensor = GetSensor();
        double val = sensor->GetMagXMicroTesla();
        Log("x uTesla: ", Commas(val));
    }, { .argCount = 0, .help = "get x uTesla" });

    Shell::AddCommand("sensor.mmc56x3.get.y", [](vector<string> argList){
        auto *sensor = GetSensor();
        double val = sensor->GetMagYMicroTesla();
        Log("x uTesla: ", Commas(val));
    }, { .argCount = 0, .help = "get y uTesla" });

    Shell::AddCommand("sensor.mmc56x3.get.z", [](vector<string> argList){
        auto *sensor = GetSensor();
        double val = sensor->GetMagZMicroTesla();
        Log("z uTesla: ", Commas(val));
    }, { .argCount = 0, .help = "get z uTesla" });

    Shell::AddCommand("sensor.mmc56x3.get.all", [](vector<string> argList){
        auto *sensor = GetSensor();
        double x = sensor->GetMagXMicroTesla();
        double y = sensor->GetMagYMicroTesla();
        double z = sensor->GetMagZMicroTesla();
        Log("x uTesla: ", Commas(x));
        Log("y uTesla: ", Commas(y));
        Log("z uTesla: ", Commas(z));
    }, { .argCount = 0, .help = "get x,y,z uTesla" });
}