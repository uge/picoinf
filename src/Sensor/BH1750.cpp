#include "BH1750.h"
#include "Shell.h"
#include "Timeline.h"
#include "Utl.h"

#include <string>
#include <vector>
using namespace std;

#include "StrictMode.h"


void BH1750::SetupShell()
{
    Timeline::Global().Event("BH1750::SetupShell");

    static uint8_t        addr     = 0x23;
    // static I2C::Instance instance = I2C::Instance::I2C0;
    static I2C::Instance  instance = I2C::Instance::I2C1;
    static BH1750        *sensor   = nullptr;

    static auto GetSensor = []{
        if (sensor == nullptr)
        {
            sensor = new BH1750(addr, instance);
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

    Shell::AddCommand("sensor.bh1750.i2c.addr", [](vector<string> argList){
        addr = (uint8_t)atoi(argList[0].c_str());
        Log("Addr set to ", ToHex(addr));
        UpdateSensor();
    }, { .argCount = 1, .help = "set i2c <hexAddr>" });

    Shell::AddCommand("sensor.bh1750.i2c.instance", [](vector<string> argList){
        uint8_t instNum = (uint8_t)atoi(argList[0].c_str());
        Log("Instance set to ", instNum);
        instance = instNum == 0 ? I2C::Instance::I2C0 : I2C::Instance::I2C1;
        UpdateSensor();
    }, { .argCount = 1, .help = "set i2c instance" });

    Shell::AddCommand("sensor.bh1750.get.lux", [](vector<string> argList){
        auto *sensor = GetSensor();
        Log("luxLowRes  : ", Commas(sensor->GetLuxLowRes()));
        Log("luxHighRes : ", Commas(sensor->GetLuxHighRes()));
        Log("luxHigh2Res: ", Commas(sensor->GetLuxHigh2Res()));
    }, { .argCount = 0, .help = "get lux" });
}