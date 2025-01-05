#pragma once

#include "hardware/clocks.h"

#include <cstdint>
#include <vector>


class PeripheralControl
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

    static void DisablePeripheralList(std::vector<Peripheral> peripheralList);
    static void DisablePeripheral(Peripheral peripheral);

    static void EnablePeripheralList(std::vector<Peripheral> peripheralList);
    static void EnablePeripheral(Peripheral peripheral);

    static bool IsEnabled(Peripheral peripheral);

    static void SetupShell();
};


