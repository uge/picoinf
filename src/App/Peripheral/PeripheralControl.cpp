using namespace std;

#include "PeripheralControl.h"
#include "Shell.h"
#include "Timeline.h"

#include "hardware/structs/scb.h"
#include "hardware/clocks.h"

#include "StrictMode.h"


void PeripheralControl::DisablePeripheralList(vector<Peripheral> peripheralList)
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

void PeripheralControl::DisablePeripheral(Peripheral peripheral)
{
    DisablePeripheralList({ peripheral });
}

void PeripheralControl::EnablePeripheralList(vector<Peripheral> peripheralList)
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

void PeripheralControl::EnablePeripheral(Peripheral peripheral)
{
    EnablePeripheralList({ peripheral });
}

bool PeripheralControl::IsEnabled(Peripheral peripheral)
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

void PeripheralControl::SetupShell()
{
    Timeline::Global().Event("PeripheralControl::SetupShell");

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


