#pragma once

#include <cstdint>
#include <vector>
using namespace std;

#include "hardware/structs/scb.h"
#include "hardware/clocks.h"

class RP2040_Peripheral
{
public:

    struct Peripheral
    {
        uint8_t regNum;
        uint32_t bits;
    };

    inline static const Peripheral SPI1    = { 0, CLOCKS_ENABLED0_CLK_SYS_SPI1_BITS | CLOCKS_ENABLED0_CLK_PERI_SPI1_BITS };
    inline static const Peripheral SPI0    = { 0, CLOCKS_ENABLED0_CLK_SYS_SPI0_BITS | CLOCKS_ENABLED0_CLK_PERI_SPI0_BITS };
    inline static const Peripheral RTC     = { 0, CLOCKS_ENABLED0_CLK_SYS_RTC_BITS | CLOCKS_ENABLED0_CLK_RTC_RTC_BITS };
    inline static const Peripheral PWM     = { 0, CLOCKS_ENABLED0_CLK_SYS_PWM_BITS };
    inline static const Peripheral SYS_PLL = { 0, CLOCKS_ENABLED0_CLK_SYS_PLL_SYS_BITS };
    inline static const Peripheral PIO1    = { 0, CLOCKS_ENABLED0_CLK_SYS_PIO1_BITS };
    inline static const Peripheral PIO0    = { 0, CLOCKS_ENABLED0_CLK_SYS_PIO0_BITS };
    inline static const Peripheral I2C1    = { 0, CLOCKS_ENABLED0_CLK_SYS_I2C1_BITS };
    inline static const Peripheral ADC     = { 0, CLOCKS_ENABLED0_CLK_SYS_ADC_BITS | CLOCKS_ENABLED0_CLK_ADC_ADC_BITS };
    inline static const Peripheral UART1   = { 1, CLOCKS_ENABLED1_CLK_SYS_UART1_BITS | CLOCKS_ENABLED1_CLK_PERI_UART1_BITS };

    static void DisablePeripheralList(vector<Peripheral> peripheralList)
    {
        uint32_t offBits0 = 0;
        uint32_t offBits1 = 0;

        for (auto peripheral : peripheralList)
        {
            if (peripheral.regNum == 0)
            {
                offBits0 |= peripheral.bits;
            }
            else
            {
                offBits1 |= peripheral.bits;
            }
        }

        clocks_hw->wake_en0 &= ~offBits0;
        clocks_hw->wake_en1 &= ~offBits1;
    }

    static void DisablePeripheral(Peripheral peripheral)
    {
        DisablePeripheralList({ peripheral });
    }

    static void EnablePeripheralList(vector<Peripheral> peripheralList)
    {
        uint32_t onBits0 = 0;
        uint32_t onBits1 = 0;

        for (auto peripheral : peripheralList)
        {
            if (peripheral.regNum == 0)
            {
                onBits0 |= peripheral.bits;
            }
            else
            {
                onBits1 |= peripheral.bits;
            }
        }

        clocks_hw->wake_en0 |= onBits0;
        clocks_hw->wake_en1 |= onBits1;
    }

    static void EnablePeripheral(Peripheral peripheral)
    {
        EnablePeripheralList({ peripheral });
    }

    static bool IsEnabled(Peripheral peripheral)
    {
        uint32_t regBits = 0;

        if (peripheral.regNum == 0)
        {
            regBits = clocks_hw->wake_en0;
        }
        else
        {
            regBits = clocks_hw->wake_en1;
        }

        return (regBits & peripheral.bits) == peripheral.bits;
    }

    static void SetupShell()
    {

        Shell::AddCommand("perip.wake.off", [](vector<string> argList) {
            // take out unused components

            uint32_t offBits0 =
                CLOCKS_ENABLED0_CLK_SYS_SPI1_BITS | CLOCKS_ENABLED0_CLK_PERI_SPI1_BITS |
                CLOCKS_ENABLED0_CLK_SYS_SPI0_BITS | CLOCKS_ENABLED0_CLK_PERI_SPI0_BITS |
                CLOCKS_ENABLED0_CLK_SYS_ROSC_BITS |
                CLOCKS_ENABLED0_CLK_SYS_PWM_BITS |
                CLOCKS_ENABLED0_CLK_SYS_PIO1_BITS |
                CLOCKS_ENABLED0_CLK_SYS_PIO0_BITS |
                CLOCKS_ENABLED0_CLK_SYS_I2C1_BITS
            ;

            uint32_t offBits1 = 0;

            clocks_hw->wake_en0 &= ~offBits0;
            clocks_hw->wake_en1 &= ~offBits1;
        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("perip.wake.on", [](vector<string> argList) {
            uint32_t onBits0 =
                CLOCKS_ENABLED0_CLK_SYS_SPI1_BITS | CLOCKS_ENABLED0_CLK_PERI_SPI1_BITS |
                CLOCKS_ENABLED0_CLK_SYS_SPI0_BITS | CLOCKS_ENABLED0_CLK_PERI_SPI0_BITS |
                CLOCKS_ENABLED0_CLK_SYS_ROSC_BITS |
                CLOCKS_ENABLED0_CLK_SYS_PWM_BITS |
                CLOCKS_ENABLED0_CLK_SYS_PIO1_BITS |
                CLOCKS_ENABLED0_CLK_SYS_PIO0_BITS |
                CLOCKS_ENABLED0_CLK_SYS_I2C1_BITS
            ;

            uint32_t onBits1 = 0;

            clocks_hw->wake_en0 |= onBits0;
            clocks_hw->wake_en1 |= onBits1;
        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("perip.wake.off.adc", [](vector<string> argList) {
            DisablePeripheral(ADC);
        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("perip.enabled.adc", [](vector<string> argList) {
            bool enabled = IsEnabled(ADC);
            Log("ADC is", enabled ? "" : " NOT", " enabled");
        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("perip.wake.on.adc", [](vector<string> argList) {
            EnablePeripheral(ADC);
        }, { .argCount = 0, .help = "" });

        // hangs the system on irq callback if irqs aren't disabled first.
        // probably handleable, but I just disabled irqs instead.
        // no power savings to be had from disabling this also.
        Shell::AddCommand("perip.wake.off.uart1", [](vector<string> argList) {
            DisablePeripheral(UART1);
        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("perip.enabled.uart1", [](vector<string> argList) {
            bool enabled = IsEnabled(UART1);
            Log("UART1 is", enabled ? "" : " NOT", " enabled");
        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("perip.wake.on.uart1", [](vector<string> argList) {
            EnablePeripheral(UART1);
        }, { .argCount = 0, .help = "" });

    }
};


