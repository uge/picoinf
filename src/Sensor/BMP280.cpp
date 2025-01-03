#include <string>
#include <vector>
using std::string;
using std::vector;

#include "Adafruit_BMP280.h"
#include "BMP280.h"
#include "Shell.h"
#include "Timeline.h"

#include "StrictMode.h"


void BMP280::SetupShell()
{
    Timeline::Global().Event("BMP280::SetupShell");

    // static uint8_t addr = 0x76;

    Shell::AddCommand("sensor.bmp280.i2c.addr", [](vector<string> argList){
        // uint8_t addr = (uint8_t)atoi(argList[0].c_str());
    }, { .argCount = 1, .help = "set i2c addr" });

    Shell::AddCommand("sensor.bmp280.i2c.instance", [](vector<string> argList){
        // uint8_t instance = (uint8_t)atoi(argList[0].c_str());
    }, { .argCount = 1, .help = "set i2c instance" });
}