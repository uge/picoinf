#pragma once

#include <stdint.h>

#include "Log.h"
#include "PAL.h"

#include "hardware/adc.h"
#include "pico/cyw43_arch.h"


class ADC
{
private:
    static const uint8_t PICO_FIRST_ADC_PIN = 26;
    static const uint8_t PICO_POWER_SAMPLE_COUNT = 5;


public:

    /////////////////////////////////////////////////////////////////
    // Use normal ADC channels as instantiated objects
    /////////////////////////////////////////////////////////////////

    ADC(uint8_t pin)
    : pin_(pin)
    {
        // Nothing to do
    }

    uint16_t GetMilliVolts()
    {
        return GetMilliVolts(pin_);
    }


public:

    /////////////////////////////////////////////////////////////////
    // Use ADC class to static read from a given pin
    /////////////////////////////////////////////////////////////////

    // raw 12-bit value
    static uint16_t Read(uint8_t pin)
    {
        adc_gpio_init(pin);

        return InputRead(GetInputFromPin(pin));
    }

    // raw converted to mv, assuming 3.3v reference
    static uint16_t GetMilliVolts(uint8_t pin)
    {
        uint16_t rawVal = Read(pin);

        // 12-bit, so out of 4096
        // referencing a 3.3v internal voltage
        uint16_t retVal = rawVal * 3300 / 4096;

        return retVal;
    }


public:

    /////////////////////////////////////////////////////////////////
    // Use ADC class to static read from special pins
    /////////////////////////////////////////////////////////////////

    // adapted from pico-examples read_vsys/power_status.c
    static uint16_t GetMilliVoltsVCC()
    {
        uint16_t retVal = 0;

        if (PAL.IsPicoW())
        {
            cyw43_thread_enter();
            // Make sure cyw43 is awake
            cyw43_arch_gpio_get(CYW43_WL_GPIO_VBUS_PIN);
        }

        retVal = GetMilliVolts(PICO_VSYS_PIN) * 3;
        
        if (PAL.IsPicoW())
        {
            cyw43_thread_exit();
        }

        return retVal;
    }

    static uint16_t ReadTemperature()
    {
        return InputRead(4);
    }


    /////////////////////////////////////////////////////////////////
    // Test
    /////////////////////////////////////////////////////////////////

    static void Test()
    {
        // The first 3 are normal ADC inputs
        for (uint8_t i = 26; i <= 28; ++i)
        {
            ADC adc(i);

            uint16_t mv = adc.GetMilliVolts();

            Log("Pin ", i, " (input ", GetInputFromPin(i), "): ", mv, " mV");
        }

        // Next is VCC
        Log("VCC: ", GetMilliVoltsVCC(), " mV");

        // Next is Temperature
    }


    /////////////////////////////////////////////////////////////////
    // Hooks
    /////////////////////////////////////////////////////////////////

    static void Init();
    static void SetupShell();


private:

    static uint8_t GetInputFromPin(uint8_t pin)
    {
        uint8_t retVal = 0;

        if (pin == 26 || pin == 27 || pin == 28 || pin == 29)
        {
            retVal = pin - PICO_FIRST_ADC_PIN;
        }

        return retVal;
    }

    // adapted from power_status.c in pico-examples read_vsys.
    // discarding samples was originally only part of reading 
    // vcc, but I'm applying it to all channels because why not.
    static uint16_t InputRead(uint8_t input)
    {
        adc_select_input(input);

        adc_fifo_setup(true, false, 0, false, false);
        adc_run(true);

        // We seem to read low values initially - this seems to fix it
        int ignore_count = PICO_POWER_SAMPLE_COUNT;
        while (!adc_fifo_is_empty() || ignore_count-- > 0) {
            adc_fifo_get_blocking();
        }

        uint32_t rawSum = 0;
        for(int i = 0; i < PICO_POWER_SAMPLE_COUNT; i++) {
            uint16_t val = adc_fifo_get_blocking();
            rawSum += val;
        }

        adc_run(false);
        adc_fifo_drain();

        uint16_t rawAvg = rawSum / PICO_POWER_SAMPLE_COUNT;

        return rawAvg;
    }


private:

    uint8_t pin_;
};