#pragma once

#include "I2C.h"
#include "Shell.h"


class ADC_ADS1115
{
static const uint8_t ADS1115_DEVICE_ID = 0x48;
static const uint8_t ADS1115_REG_ADDR_CONFIG = 0x01;
static const uint8_t ADS1115_REG_ADDR_CONVERSION = 0x00;

public:

    struct Config
    {
        uint8_t opStatus = 0;
        uint8_t mux = 0;
        uint8_t pga = 0;
        uint8_t mode = 0;
        uint8_t dr = 0;
        uint8_t compMode = 0;
        uint8_t compPol = 0;
        uint8_t compLat = 0;
        uint8_t compQue = 0;
    };

    void Test()
    {
        Log("sizeof(Config) = ", sizeof(Config));

        Config cfg = GetAdcConfig();
        PrintAdcConfig("ADC Config", cfg);
        LogNL();

        Config cfg2 = ConfigureAdc();
        PrintAdcConfig("Configured", cfg2);
        LogNL();

        // without a short delay, the read-back values are unexpected and
        // probably wrong
        PAL.Delay(100);

        Config cfg3 = GetAdcConfig();
        PrintAdcConfig("ADC Config re-read", cfg3);
        LogNL();
    }

    void PrintAdcConfig(string str, Config cfg)
    {
        Log(str);

        Log("opStatus: ", cfg.opStatus, ", ", Format::ToBin(cfg.opStatus, false, 1));
        Log("mux     : ", cfg.mux, ", ", Format::ToBin(cfg.mux, false, 3));
        Log("pga     : ", cfg.pga, ", ", Format::ToBin(cfg.pga, false, 3));
        Log("mode    : ", cfg.mode, ", ", Format::ToBin(cfg.mode, false, 1));
        Log("dr      : ", cfg.dr, ", ", Format::ToBin(cfg.dr, false, 3));
        Log("compMode: ", cfg.compMode, ", ", Format::ToBin(cfg.compMode, false, 1));
        Log("compPol : ", cfg.compPol, ", ", Format::ToBin(cfg.compPol, false, 1));
        Log("compLat : ", cfg.compLat, ", ", Format::ToBin(cfg.compLat, false, 1));
        Log("compQue : ", cfg.compQue, ", ", Format::ToBin(cfg.compQue, false, 2));
    }

    Config GetAdcConfig()
    {
        uint16_t retVal = i2c_.ReadReg16(ADS1115_DEVICE_ID, ADS1115_REG_ADDR_CONFIG);

        // convert to config
        Config cfg = {
            .opStatus = (uint8_t)BitsGet(retVal, 15, 15),
            .mux      = (uint8_t)BitsGet(retVal, 14, 12),
            .pga      = (uint8_t)BitsGet(retVal, 11,  9),
            .mode     = (uint8_t)BitsGet(retVal,  8,  8),
            .dr       = (uint8_t)BitsGet(retVal,  7,  5),
            .compMode = (uint8_t)BitsGet(retVal,  4,  4),
            .compPol  = (uint8_t)BitsGet(retVal,  3,  3),
            .compLat  = (uint8_t)BitsGet(retVal,  2,  2),
            .compQue  = (uint8_t)BitsGet(retVal,  1,  0),
        };

        return cfg;
    }

    Config ConfigureAdc()
    {
        Config cfg = {
            .opStatus = 0b1,    // start conversion
            .mux      = 0b100,  // reference adc channel 0 to gnd
            .pga      = 0b000,  // +/- 6.144 volt range
            .mode     = 0b0,    // continuous conversion
            .dr       = 0b000,  // data rate = 8 sps, slowest and highest res
            .compMode = 0b0,    // default comparator mode (not even sure)
            .compPol  = 0b0,    // default behavior of alert pin
            .compLat  = 0b0,    // default behavior of latching of alert pin
            .compQue  = 0b11,   // default disable alert pin
        };

        // convert into 16-bit reg value
        uint16_t val = 0;
        val |= BitsPut(cfg.opStatus, 15, 15);
        val |= BitsPut(cfg.mux,      14, 12);
        val |= BitsPut(cfg.pga,      11,  9);
        val |= BitsPut(cfg.mode,      8,  8);
        val |= BitsPut(cfg.dr,        7,  5);
        val |= BitsPut(cfg.compMode,  4,  4);
        val |= BitsPut(cfg.compPol,   3,  3);
        val |= BitsPut(cfg.compLat,   2,  2);
        val |= BitsPut(cfg.compQue,   1,  0);

        i2c_.WriteReg16(ADS1115_DEVICE_ID, ADS1115_REG_ADDR_CONFIG, val);

        return cfg;
    }

    double GetAdcVoltage()
    {
        double adcVoltage = 0.0;

        ConfigureAdc();

        uint16_t val = i2c_.ReadReg16(ADS1115_DEVICE_ID, ADS1115_REG_ADDR_CONVERSION);

        static const uint16_t ADC_MAX = 0x7FFF;
        double adcPct = (double)val / ADC_MAX;

        static const double VOLTAGE_RANGE = 6.144;
        adcVoltage = adcPct * VOLTAGE_RANGE;

        return adcVoltage;
    }

    static void Setup()
    {
        Shell::AddCommand("adc.1115.test", [](vector<string> argList){
            ADC_ADS1115 adc;
            adc.Test();
        }, { .argCount = 0, .help = "test adc ads1115 driver"});

        Shell::AddCommand("adc.1115.cfg.get", [](vector<string> argList){
            ADC_ADS1115 adc;
            ADC_ADS1115::Config cfg = adc.GetAdcConfig();
            adc.PrintAdcConfig("adc.1115.cfg.get", cfg);
        }, { .argCount = 0, .help = "test adc get cfg"});

        Shell::AddCommand("adc.1115.cfg.set", [](vector<string> argList){
            ADC_ADS1115 adc;
            ADC_ADS1115::Config cfg = adc.ConfigureAdc();
            adc.PrintAdcConfig("adc.1115.cfg.set", cfg);
        }, { .argCount = 0, .help = "test adc set cfg"});

        Shell::AddCommand("adc.1115.get", [](vector<string> argList){
            ADC_ADS1115 adc;
            double voltage = adc.GetAdcVoltage();
            Log("ADC Voltage: ", voltage);

            // was used in conjunction with ADB495, which is a thermocouple
            // which outputs a voltage that you can convert to a temperature.
            // lazy left this code here since I was just working on I2C really.
            double tempC = voltage / 0.005;
            double tempF = (tempC * 9 / 5) + 32;

            Log("TempC: ", tempC);
            Log("TempF: ", tempF);
        }, { .argCount = 0, .help = "adc.1115 get voltage"});
    }

private:

    I2C i2c_;
};
