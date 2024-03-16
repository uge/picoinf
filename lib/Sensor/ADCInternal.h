#pragma once

#include <stdint.h>


class ADC
{
private:
    static const uint8_t PICO_FIRST_ADC_PIN = 26;
    static const uint8_t PICO_POWER_SAMPLE_COUNT = 5;


public:

    /////////////////////////////////////////////////////////////////
    // Use normal ADC channels as instantiated objects
    /////////////////////////////////////////////////////////////////

    ADC(uint8_t pin);

    uint16_t GetMilliVolts();


public:

    /////////////////////////////////////////////////////////////////
    // Use ADC class to static read from a given pin
    /////////////////////////////////////////////////////////////////

    // raw 12-bit value
    static uint16_t Read(uint8_t pin);

    // raw converted to mv, assuming 3.3v reference
    static uint16_t GetMilliVolts(uint8_t pin);


public:

    /////////////////////////////////////////////////////////////////
    // Use ADC class to static read from special pins
    /////////////////////////////////////////////////////////////////

    // adapted from pico-examples read_vsys/power_status.c
    static uint16_t GetMilliVoltsVCC();

    static uint16_t ReadTemperature();


    /////////////////////////////////////////////////////////////////
    // Test
    /////////////////////////////////////////////////////////////////

    static void Test();


    /////////////////////////////////////////////////////////////////
    // Hooks
    /////////////////////////////////////////////////////////////////

    static void Init();
    static void SetupShell();


private:

    /////////////////////////////////////////////////////////////////
    // Implementation
    /////////////////////////////////////////////////////////////////

    static uint8_t GetInputFromPin(uint8_t pin);

    // adapted from power_status.c in pico-examples read_vsys.
    // discarding samples was originally only part of reading 
    // vcc, but I'm applying it to all channels because why not.
    static uint16_t InputRead(uint8_t input);


private:

    uint8_t pin_;
};