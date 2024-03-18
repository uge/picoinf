#include "MYRP2040.h"

#include "hardware/timer.h"
#include "pico/bootrom.h"

#include "ADCInternal.h"
#include "Pin.h"



void RP2040::DebugPrint()
{
    Clock::PrintAll();
}


void RP2040::InitUart1()
{
    // TODO -- uncomment and implement
    // uart_rpi_init(UartDeviceMap(UART::UART_1));

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

