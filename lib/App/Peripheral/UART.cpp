#include "Evm.h"
#include "DataStreamDistributor.h"
#include "IDMaker.h"
#include "KMessagePassing.h"
#include "KTask.h"
#include "LineStreamDistributor.h"
#include "PAL.h"
#include "Shell.h"
#include "Timeline.h"
#include "UART.h"
#include "USB.h"

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"

#include <string.h>

#include <algorithm>
#include <vector>
using namespace std;


////////////////////////////////////////////////////////////////////////////////
// UART Init state
////////////////////////////////////////////////////////////////////////////////

static USB_CDC *cdc0 = USB::GetCdcInstance(0);


////////////////////////////////////////////////////////////////////////////////
// UART Input / Output state
////////////////////////////////////////////////////////////////////////////////

static const uint16_t UART_OUTPUT_PIPE_SIZE   = 1000;
static const uint16_t UART_INPUT_PIPE_SIZE    = 1002;
static const uint16_t UART_INPUT_MAX_LINE_LEN = 1000;

static KMessagePipe<char, UART_OUTPUT_PIPE_SIZE> UART_0_OUTPUT_PIPE;
static KMessagePipe<char, UART_INPUT_PIPE_SIZE>  UART_0_INPUT_PIPE;
static DataStreamDistributor UART_0_INPUT_DATA_STREAM_DISTRIBUTOR(UART::UART_0);
static LineStreamDistributor UART_0_INPUT_LINE_STREAM_DISTRIBUTOR(UART::UART_0, UART_INPUT_MAX_LINE_LEN, "UART_0_LINE_STREAM_DISTRIBUTOR");

static KMessagePipe<char, UART_OUTPUT_PIPE_SIZE> UART_1_OUTPUT_PIPE;
static KMessagePipe<char, UART_INPUT_PIPE_SIZE>  UART_1_INPUT_PIPE;
static DataStreamDistributor UART_1_INPUT_DATA_STREAM_DISTRIBUTOR(UART::UART_1);
static LineStreamDistributor UART_1_INPUT_LINE_STREAM_DISTRIBUTOR(UART::UART_1, UART_INPUT_MAX_LINE_LEN, "UART_1_LINE_STREAM_DISTRIBUTOR");

static DataStreamDistributor UART_USB_INPUT_DATA_STREAM_DISTRIBUTOR(UART::UART_USB);
static LineStreamDistributor UART_USB_INPUT_LINE_STREAM_DISTRIBUTOR(UART::UART_USB, UART_INPUT_MAX_LINE_LEN, "UART_USB_LINE_STREAM_DISTRIBUTOR");


////////////////////////////////////////////////////////////////////////////////
// UART Output
////////////////////////////////////////////////////////////////////////////////

static KMessagePipe<char, UART_OUTPUT_PIPE_SIZE> &UartGetPipe()
{
    KMessagePipe<char, UART_OUTPUT_PIPE_SIZE> *ptr = &UART_0_OUTPUT_PIPE;

    UART uart = UartCurrent();

    if      (uart == UART::UART_1)   { ptr = &UART_1_OUTPUT_PIPE; }
    
    return *ptr;
}


////////////////////////////////////////////////////////////////////////////////
// UART Input Handling
////////////////////////////////////////////////////////////////////////////////

// https://stackoverflow.com/questions/76367736/uart-tx-produce-endless-interrupts-how-to-acknowlage-the-interrupt
// https://forums.raspberrypi.com/viewtopic.php?t=343110
static void UartInterruptHandlerUartX(const char                                *workLabel,
                                      uart_inst_t                               *uart,
                                      KMessagePipe<char, UART_OUTPUT_PIPE_SIZE> &pipeOut,
                                      KMessagePipe<char, UART_INPUT_PIPE_SIZE>  &pipeIn,
                                      DataStreamDistributor                     &distIn)
{
    // do some writing
    while (pipeOut.Count() && uart_is_writable(uart))
    {
        char ch;
        if (pipeOut.Get(ch))
        {
            uart_putc_raw(uart, ch);
        }
    }

    // clear "tell me when to feed the next byte" interrupt
    if (pipeOut.Count() == 0)
    {
        uart_get_hw(uart)->icr = UART_UARTICR_TXIC_BITS ;
    }

    // do some reading
    bool dataReady = false;
    while (uart_is_readable(uart))
    {
        uint8_t ch = uart_getc(uart);

        // is there someone who wants it and a place to put it?
        if (distIn.GetCallbackCount() == 0)
        {
            pipeIn.Flush();
        }
        else
        {
            pipeIn.Put(ch, 0);

            dataReady = true;
        }
    }

    if (dataReady)
    {
        Evm::QueueLowPriorityWork(workLabel, [&]{
            if (pipeIn.Count())
            {
                vector<uint8_t> byteList;

                char ch;
                bool ok = pipeIn.Get(ch, 0);
                while (ok)
                {
                    byteList.push_back(ch);

                    ok = pipeIn.Get(ch, 0);
                }

                if (byteList.size())
                {
                    distIn.Distribute(byteList);
                }
            }
        });
    }
}

static void UartInterruptHandlerUart0()
{
    UartInterruptHandlerUartX("UART::uart0",
                              uart0,
                              UART_0_OUTPUT_PIPE,
                              UART_0_INPUT_PIPE,
                              UART_0_INPUT_DATA_STREAM_DISTRIBUTOR);
}

static void UartInterruptHandlerUart1()
{
    UartInterruptHandlerUartX("UART::uart1",
                              uart1,
                              UART_1_OUTPUT_PIPE,
                              UART_1_INPUT_PIPE,
                              UART_1_INPUT_DATA_STREAM_DISTRIBUTOR);
}


////////////////////////////////////////////////////////////////////////////////
// Handlers for USB
////////////////////////////////////////////////////////////////////////////////

static void UsbOnRx(vector<uint8_t> &byteList)
{
    UART_USB_INPUT_DATA_STREAM_DISTRIBUTOR.Distribute(byteList);
}

static void UsbSend(const uint8_t *buf, uint16_t bufLen)
{
    if (cdc0->GetDtr())
    {
        cdc0->Send(buf, bufLen);
    }
    else
    {
        cdc0->Clear();
    }
}


////////////////////////////////////////////////////////////////////////////////
// UART External API
////////////////////////////////////////////////////////////////////////////////

static vector<UART> uartStack = { UART::UART_0 };

void UartPush(UART uart)
{
    uartStack.push_back(uart);
}

void UartPop()
{
    if (uartStack.size() != 1)
    {
        uartStack.pop_back();
    }
}

UART UartCurrent()
{
    return uartStack.back();
}

void UartOut(char c)
{
    UART uart = UartCurrent();

    if (uart == UART::UART_0)
    {
        uart_putc_raw(uart0, c);
    }
    else if (uart == UART::UART_1)
    {
        uart_putc_raw(uart1, c);
    }
    else if (uart == UART::UART_USB)
    {
        UsbSend((uint8_t *)&c, 1);
    }
}

void UartOut(const char *str)
{
    for (size_t i = 0; i < strlen(str); ++i)
    {
        UartOut(str[i]);
    }
}

void UartSend(const uint8_t *buf, uint16_t bufLen)
{
    if (PAL.InIsr())
    {
        for (int i = 0; i < bufLen; ++i)
        {
            UartOut(buf[i]);
        }
    }
    else
    {
        UART uart = UartCurrent();

        if (uart == UART::UART_0 || uart == UART::UART_1)
        {
            if (bufLen <= UART_OUTPUT_PIPE_SIZE)
            {
                // Make sure the output isn't disabled before sending
                bool sendOk = true;

                auto *pipe = &UartGetPipe();
                // if (pipe == &UART_1_OUTPUT_PIPE)
                // {
                //     sendOk = UartIsEnabled(UART::UART_1);
                // }

                if (sendOk)
                {
                    // Should be ok without an IRQ lock given guarantees around the pipe
                    pipe->Put((char *)buf, bufLen);
                    irq_set_pending(UartCurrent() == UART::UART_0 ? UART0_IRQ : UART1_IRQ);
                }
            }
            else
            {
                // I guess it's not going then
            }
        }
        else if (uart == UART::UART_USB)
        {
            UsbSend(buf, bufLen);
        }
    }
}

void UartSend(const vector<uint8_t> &byteList)
{
    UartSend(byteList.data(), byteList.size());
}


////////////////////////////////////////////////////////////////////////////////
// UART DataStream Interface
////////////////////////////////////////////////////////////////////////////////

pair<bool, uint8_t> UartAddDataStreamCallback(UART uart, function<void(const vector<uint8_t> &data)> cbFn)
{
    bool retValOk = false;
    uint8_t retValId = 0;

    DataStreamDistributor *dist = nullptr;
    if (uart == UART::UART_0)
    {
        dist = &UART_0_INPUT_DATA_STREAM_DISTRIBUTOR;
    }
    else if (uart == UART::UART_1)
    {
        dist = &UART_1_INPUT_DATA_STREAM_DISTRIBUTOR;
    }
    else if (uart == UART::UART_USB)
    {
        dist = &UART_USB_INPUT_DATA_STREAM_DISTRIBUTOR;
    }

    if (dist)
    {
        auto [ok, id] = dist->AddDataStreamCallback(cbFn);

        retValOk = ok;
        retValId = id;
    }

    return { retValOk, retValId };
}

bool UartSetDataStreamCallback(UART uart, function<void(const vector<uint8_t> &data)> cbFn, uint8_t id)
{
    bool retVal = false;

    DataStreamDistributor *dist = nullptr;
    if (uart == UART::UART_0)
    {
        dist = &UART_0_INPUT_DATA_STREAM_DISTRIBUTOR;
    }
    else if (uart == UART::UART_1)
    {
        dist = &UART_1_INPUT_DATA_STREAM_DISTRIBUTOR;
    }
    else if (uart == UART::UART_USB)
    {
        dist = &UART_USB_INPUT_DATA_STREAM_DISTRIBUTOR;
    }

    if (dist)
    {
        retVal = dist->SetDataStreamCallback(id, cbFn);
    }

    return retVal;
}

bool UartRemoveDataStreamCallback(UART uart, uint8_t id)
{
    bool retVal = false;

    DataStreamDistributor *dist = nullptr;
    if (uart == UART::UART_0)
    {
        dist = &UART_0_INPUT_DATA_STREAM_DISTRIBUTOR;
    }
    else if (uart == UART::UART_1)
    {
        dist = &UART_1_INPUT_DATA_STREAM_DISTRIBUTOR;
    }
    else if (uart == UART::UART_USB)
    {
        dist = &UART_USB_INPUT_DATA_STREAM_DISTRIBUTOR;
    }

    if (dist)
    {
        retVal = dist->RemoveDataStreamCallback(id);
    }

    return retVal;
}


////////////////////////////////////////////////////////////////////////////////
// UART LineStream Interface
////////////////////////////////////////////////////////////////////////////////

pair<bool, uint8_t> UartAddLineStreamCallback(UART uart, function<void(const string &line)> cbFn, bool hideBlankLines)
{
    bool retValOk = false;
    uint8_t retValId = 0;

    LineStreamDistributor *dist = nullptr;
    if (uart == UART::UART_0)
    {
        dist = &UART_0_INPUT_LINE_STREAM_DISTRIBUTOR;
    }
    else if (uart == UART::UART_1)
    {
        dist = &UART_1_INPUT_LINE_STREAM_DISTRIBUTOR;
    }
    else if (uart == UART::UART_USB)
    {
        dist = &UART_USB_INPUT_LINE_STREAM_DISTRIBUTOR;
    }

    if (dist)
    {
        auto [ok, id] = dist->AddLineStreamCallback(cbFn, hideBlankLines);

        retValOk = ok;
        retValId = id;
    }

    return { retValOk, retValId };
}

bool UartSetLineStreamCallback(UART uart, function<void(const string &line)> cbFn, uint8_t id, bool hideBlankLines)
{
    bool retVal = false;

    LineStreamDistributor *dist = nullptr;
    if (uart == UART::UART_0)
    {
        dist = &UART_0_INPUT_LINE_STREAM_DISTRIBUTOR;
    }
    else if (uart == UART::UART_1)
    {
        dist = &UART_1_INPUT_LINE_STREAM_DISTRIBUTOR;
    }
    else if (uart == UART::UART_USB)
    {
        dist = &UART_USB_INPUT_LINE_STREAM_DISTRIBUTOR;
    }

    if (dist)
    {
        retVal = dist->SetLineStreamCallback(id, cbFn, hideBlankLines);
    }

    return retVal;
}

bool UartRemoveLineStreamCallback(UART uart, uint8_t id)
{
    bool retVal = false;

    LineStreamDistributor *dist = nullptr;
    if (uart == UART::UART_0)
    {
        dist = &UART_0_INPUT_LINE_STREAM_DISTRIBUTOR;
    }
    else if (uart == UART::UART_1)
    {
        dist = &UART_1_INPUT_LINE_STREAM_DISTRIBUTOR;
    }
    else if (uart == UART::UART_USB)
    {
        dist = &UART_USB_INPUT_LINE_STREAM_DISTRIBUTOR;
    }

    if (dist)
    {
        retVal = dist->RemoveLineStreamCallback(id);
    }

    return retVal;
}


////////////////////////////////////////////////////////////////////////////////
// UART Enable / Disable
////////////////////////////////////////////////////////////////////////////////

static void UartInitDevice(uart_inst_t *uart, uint8_t pinRx, uint8_t pinTx, uint32_t baudRate)
{
    // https://lucidar.me/en/serialib/most-used-baud-rates-table/

    // static const uint32_t BAUD_RATE = 9600;
    // static const uint32_t BAUD_RATE = 57600;
    // static const uint32_t BAUD_RATE = 76800;

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(pinTx, GPIO_FUNC_UART);
    gpio_set_function(pinRx, GPIO_FUNC_UART);

    // do init
    uart_init(uart, baudRate);

    // Turn off FIFO's - we want to do this character by character
    // uart_set_fifo_enabled(uart, false);
}

static void UartDeInitDevice(uart_inst_t *uart, uint8_t pinRx, uint8_t pinTx)
{
    uart_deinit(uart);

    gpio_set_function(pinRx, GPIO_FUNC_NULL);
    gpio_set_function(pinTx, GPIO_FUNC_NULL);
}

static void UartInitDeviceInterrupts(uart_inst_t *uart, irq_handler_t handler)
{
    // Set up a RX interrupt
    // We need to set up the handler first
    // Select correct interrupt for the UART we are using
    int UART_IRQ = uart == uart0 ? UART0_IRQ : UART1_IRQ;

    // And set up and enable the interrupt handlers
    if (irq_get_exclusive_handler(UART_IRQ) == nullptr)
    {
        irq_set_exclusive_handler(UART_IRQ, handler);
    }
    irq_set_enabled(UART_IRQ, true);

    // Now enable the UART to send interrupts - RX and TX
    uart_set_irq_enables(uart, true, true);
}

static void UartDeInitDeviceInterrupts(uart_inst_t *uart)
{
    int UART_IRQ = uart == uart0 ? UART0_IRQ : UART1_IRQ;

    irq_set_enabled(UART_IRQ, false);
}


static bool uart1Enabled_ = false;

void UartEnable(UART uart)
{
    IrqLock lock;

    // set up raw IRQ handling
    // then trigger interrupts to kick off any already-queued data

    if (uart == UART::UART_0)
    {
        UartInitDevice(uart0, 1, 0, 76800);
        UartInitDeviceInterrupts(uart0, UartInterruptHandlerUart0);
        irq_set_pending(UART0_IRQ);
    }
    else if (uart == UART::UART_1)
    {
        UartInitDevice(uart1, 9, 8, 9600);
        UartInitDeviceInterrupts(uart1, UartInterruptHandlerUart1);
        irq_set_pending(UART1_IRQ);

        uart1Enabled_ = true;
    }
}

void UartDisable(UART uart)
{
    IrqLock lock;

    UartClearTxBuffer(uart);
    UartClearRxBuffer(uart);

    if (uart == UART::UART_1)
    {
        UartDeInitDevice(uart1, 9, 8);
        UartDeInitDeviceInterrupts(uart1);

        uart1Enabled_ = false;
    }
}

bool UartIsEnabled(UART uart)
{
    bool retVal = true;

    if (uart == UART::UART_1)
    {
        retVal = uart1Enabled_;
    }

    return retVal;
}


////////////////////////////////////////////////////////////////////////////////
// UART Clear Buffer Interface
////////////////////////////////////////////////////////////////////////////////

void UartClearRxBuffer(UART uart)
{
    // clear the queues seen below
    if (uart == UART::UART_1)
    {
        UART_1_INPUT_PIPE.Flush();
        UART_1_INPUT_LINE_STREAM_DISTRIBUTOR.Clear();
        UART_1_INPUT_LINE_STREAM_DISTRIBUTOR.ClearInFlight();
    }
}

void UartClearTxBuffer(UART uart)
{
    if (uart == UART::UART_1)
    {
        UART_1_OUTPUT_PIPE.Flush();
    }
}


////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////

void UartInit()
{
    Timeline::Global().Event("UartInit");

    // Set up stdio
    stdio_init_all();

    // register the line distributors with the binary distributors -- daisy chain
    UART_0_INPUT_DATA_STREAM_DISTRIBUTOR.AddDataStreamCallback([](const vector<uint8_t> &data){
        UART_0_INPUT_LINE_STREAM_DISTRIBUTOR.AddData(data);
    });
    UART_1_INPUT_DATA_STREAM_DISTRIBUTOR.AddDataStreamCallback([](const vector<uint8_t> &data){
        UART_1_INPUT_LINE_STREAM_DISTRIBUTOR.AddData(data);
    });
    UART_USB_INPUT_DATA_STREAM_DISTRIBUTOR.AddDataStreamCallback([](const vector<uint8_t> &data){
        UART_USB_INPUT_LINE_STREAM_DISTRIBUTOR.AddData(data);
    });

    // Enable
    UartEnable(UART::UART_0);
    UartEnable(UART::UART_1);

    // Set up USB serial interface
    cdc0->SetCallbackOnRx(UsbOnRx);
}

void UartSetupShell()
{
    Timeline::Global().Event("UartSetupShell");
    
    // Setup shell
    Shell::AddCommand("uart.log", [](vector<string> argList){
        string uartStr = argList[0];
        UART uart = UART::UART_0;

        if      (uartStr == "1")   { uart = UART::UART_1; }
        else if (uartStr == "usb") { uart = UART::UART_USB; }

        string msg = argList[1];

        UartTarget target(uart);
        Log(msg);
    }, { .argCount = 2, .help = "UART log <uart=1> <msg>"});

    Shell::AddCommand("uart.stats", [](vector<string> argList){
        Log("UART_0");
        Log("- irq queued    : ", UART_0_INPUT_PIPE.Count());
        Log("- raw listeners : ", UART_0_INPUT_DATA_STREAM_DISTRIBUTOR.GetCallbackCount() - 1);
        Log("- line listeners: ", UART_0_INPUT_LINE_STREAM_DISTRIBUTOR.GetCallbackCount());
        Log("  - queued      : ", UART_0_INPUT_LINE_STREAM_DISTRIBUTOR.Size());
        LogNL();
        Log("UART_1");
        Log("- irq queued    : ", UART_1_INPUT_PIPE.Count());
        Log("- raw listeners : ", UART_1_INPUT_DATA_STREAM_DISTRIBUTOR.GetCallbackCount() - 1);
        Log("- line listeners: ", UART_1_INPUT_LINE_STREAM_DISTRIBUTOR.GetCallbackCount());
        Log("  - queued      : ", UART_1_INPUT_LINE_STREAM_DISTRIBUTOR.Size());
        LogNL();
        Log("UART_USB");
        Log("- irq queued    : -");
        Log("- raw listeners : ", UART_USB_INPUT_DATA_STREAM_DISTRIBUTOR.GetCallbackCount() - 1);
        Log("- line listeners: ", UART_USB_INPUT_LINE_STREAM_DISTRIBUTOR.GetCallbackCount());
        Log("  - queued      : ", UART_USB_INPUT_LINE_STREAM_DISTRIBUTOR.Size());
    }, { .argCount = 0, .help = "UART stats"});

    Shell::AddCommand("uart.uart1", [](vector<string> argList){
        if (argList[0] == "on") { UartEnable(UART::UART_1);  }
        else                    { UartDisable(UART::UART_1); }
    }, { .argCount = 1, .help = "UART1 <on/off>"});

    Shell::AddCommand("uart.uart1.clear.rx", [](vector<string> argList){
        PAL.DelayBusy(2000);
        UartClearRxBuffer(UART::UART_1);
    }, { .argCount = 0, .help = "Clear UART1 RX buffer"});

    Shell::AddCommand("uart.uart1.clear.tx", [](vector<string> argList){
        UartClearTxBuffer(UART::UART_1);
    }, { .argCount = 0, .help = "Clear UART1 TX buffer"});
}

