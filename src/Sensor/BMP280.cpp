#include "BMP280.h"
#include "Shell.h"
#include "Timeline.h"
#include "Utl.h"

#include <string>
#include <vector>
using namespace std;

#include "StrictMode.h"


// Address is either 0x76 or 0x77
BMP280::BMP280(uint8_t addr, I2C::Instance instance)
: tw_(addr, instance)
, bmp280_(&tw_)
{
    // startup sequence taken from example ino.
    // "Default settings from datasheet."
    bmp280_.begin();
    bmp280_.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                        Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                        Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                        Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                        Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
}

double BMP280::GetTemperatureCelsius()
{
    // sensor returns celsius
    return bmp280_.readTemperature();
}

double BMP280::GetTemperatureFahrenheit()
{
    return (GetTemperatureCelsius() * (9.0 / 5.0)) + 32;
}

double BMP280::GetPressurehPa()
{
    // sensor returns Pa
    return bmp280_.readPressure() / 100;
}

double BMP280::GetPressureBar()
{
    return GetPressurehPa() * 1'000;
}

double BMP280::GetAltitudeMeters()
{
    // sensor returns meters
    // calibrated to nominal pressure at sea level (hPa)
    return bmp280_.readAltitude(1013.25);
}

double BMP280::GetAltitudeFeet()
{
    return GetAltitudeMeters() * 3.28084;
}


void BMP280::SetupShell()
{
    Timeline::Global().Event("BMP280::SetupShell");

    static uint8_t       addr     = 0x77;
    // static I2C::Instance instance = I2C::Instance::I2C0;
    static I2C::Instance instance = I2C::Instance::I2C1;
    static BMP280        *bmp280 = nullptr;

    static auto GetBMP280 = []{
        if (bmp280 == nullptr)
        {
            bmp280 = new BMP280(addr, instance);
        }

        return bmp280;
    };

    static auto UpdateBMP280 = []{
        if (bmp280 != nullptr)
        {
            delete bmp280;
            bmp280 = nullptr;
        }

        GetBMP280();
    };

    Shell::AddCommand("sensor.bmp280.i2c.addr", [](vector<string> argList){
        addr = (uint8_t)atoi(argList[0].c_str());
        Log("Addr set to ", ToHex(addr));
        UpdateBMP280();
    }, { .argCount = 1, .help = "set i2c <hexAddr>" });

    Shell::AddCommand("sensor.bmp280.i2c.instance", [](vector<string> argList){
        uint8_t instNum = (uint8_t)atoi(argList[0].c_str());
        Log("Instance set to ", instNum);
        instance = instNum == 0 ? I2C::Instance::I2C0 : I2C::Instance::I2C1;
        UpdateBMP280();
    }, { .argCount = 1, .help = "set i2c instance" });

    Shell::AddCommand("sensor.bmp280.get.temp", [](vector<string> argList){
        BMP280 *bmp280 = GetBMP280();

        double tempC = bmp280->GetTemperatureCelsius();
        double tempF = bmp280->GetTemperatureFahrenheit();

        Log("TempC: ", tempC);
        Log("TempF: ", tempF);
    }, { .argCount = 0, .help = "get temperature" });

    Shell::AddCommand("sensor.bmp280.get.pressure", [](vector<string> argList){
        BMP280 *bmp280 = GetBMP280();

        double hPa = bmp280->GetPressurehPa();
        double bar = bmp280->GetPressureBar();

        Log("hPa: ", Commas(hPa));
        Log("bar: ", Commas(bar));
    }, { .argCount = 0, .help = "get pressure" });

    Shell::AddCommand("sensor.bmp280.get.altitude", [](vector<string> argList){
        BMP280 *bmp280 = GetBMP280();

        double altM  = bmp280->GetAltitudeMeters();
        double altFt = bmp280->GetAltitudeFeet();

        Log("altM : ", Commas(altM));
        Log("altFt: ", Commas(altFt));
    }, { .argCount = 0, .help = "get altitude" });

    Shell::AddCommand("sensor.bmp280.get.all", [](vector<string> argList){
        BMP280 *bmp280 = GetBMP280();

        double tempC = bmp280->GetTemperatureCelsius();
        double tempF = bmp280->GetTemperatureFahrenheit();
        double hPa   = bmp280->GetPressurehPa();
        double bar   = bmp280->GetPressureBar();
        double altM  = bmp280->GetAltitudeMeters();
        double altFt = bmp280->GetAltitudeFeet();

        Log("TempC: ", tempC);
        Log("TempF: ", tempF);
        Log("hPa  : ", Commas(hPa));
        Log("bar  : ", Commas(bar));
        Log("altM : ", Commas(altM));
        Log("altFt: ", Commas(altFt));
    }, { .argCount = 0, .help = "get all" });

}