#include "RP2040.h"

#include "hardware/timer.h"
#include "pico/bootrom.h"

#include "ADCInternal.h"
#include "Pin.h"



uint64_t RP2040::TimeUs64()
{
    return time_us_64();
}

void RP2040::EnsureAdcChannelIsPinInput(uint8_t channel)
{
    if (channel == 0)
    {
        Pin::Configure(0, 26, Pin::Type::INPUT);
    }
    else if (channel == 1)
    {
        Pin::Configure(0, 27, Pin::Type::INPUT);
    }
    else if (channel == 2)
    {
        Pin::Configure(0, 28, Pin::Type::INPUT);
    }
    else if (channel == 3)
    {
        Pin::Configure(0, 29, Pin::Type::INPUT);
    }

    PAL.DelayBusyUs(500);   // determined needed at at ADC 12MHz
}

double RP2040::MeasureVCC()
{
    return ADCInternal::GetChannel(3) * 3.0 / 1000.0;
}

void RP2040::DebugPrint()
{
    Clock::PrintAll();
}


#include "uart_rpi_pico_copy.h"
void RP2040::InitUart1()
{
    uart_rpi_init(UartDeviceMap(UART::UART_1));

    // When device was running at 48MHz and taking in GPS via
    // UART1 while also talking USB serial, the GPS stream would
    // be missing characters.
    // Changing UART1 to use FIFO processing eliminated that issue.
    // Given the GPS is a non-interactive continuous stream of
    // data, and  this seems
    hw_set_bits(&uart1_hw->lcr_h, UART_UARTLCR_H_FEN_BITS);
}

void RP2040::Init()
{
    Timeline::Global().Event("RP2040::Init");

    RP2040::Clock::Init();
}

void RP2040::SetupShell()
{
    Timeline::Global().Event("RP2040::SetupShell");

    RP2040::Clock::SetupShell();
    RP2040::Peripheral::SetupShell();

    Shell::AddCommand("rp2040.bootloader", [](vector<string> argList) {
        reset_usb_boot(0, 0);
    }, { .argCount = 0, .help = "" });
}



static int RP2040Init()
{
    RP2040::SetupShell();

    return 1;
}

static int RP2040SetupShell()
{
    RP2040::SetupShell();

    return 1;
}


#include <zephyr/init.h>
SYS_INIT(RP2040Init, APPLICATION, 50);
SYS_INIT(RP2040SetupShell, APPLICATION, 80);
