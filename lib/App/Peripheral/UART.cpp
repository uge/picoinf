#include "Evm.h"
#include "IDMaker.h"
#include "KMessagePassing.h"
#include "KTask.h"
#include "PAL.h"
#include "Shell.h"
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
// UART Output state
////////////////////////////////////////////////////////////////////////////////

static const uint16_t UART_OUTPUT_PIPE_SIZE = 1000;
static KMessagePipe<char, UART_OUTPUT_PIPE_SIZE> UART_0_OUTPUT_PIPE;
static KMessagePipe<char, UART_OUTPUT_PIPE_SIZE> UART_1_OUTPUT_PIPE;


////////////////////////////////////////////////////////////////////////////////
// UART Input state
////////////////////////////////////////////////////////////////////////////////

class DataStreamDistributor
{
public:
    DataStreamDistributor(UART uart)
    : uart_(uart)
    {
        // Reserve id of 0 for special purposes
        idMaker_.GetNextId();   // 0
    }

    pair<bool, uint8_t> AddDataStreamCallback(function<void(const vector<uint8_t> &data)> cbFn)
    {
        auto [ok, id] = idMaker_.GetNextId();

        if (ok)
        {
            id__cbFn_[id] = cbFn;
        }

        return { ok, id };
    }

    bool SetDataStreamCallback(uint8_t id, function<void(const vector<uint8_t> &data)> cbFn)
    {
        bool retVal = false;

        if (id__cbFn_.contains(id) || id == 0)
        {
            id__cbFn_[id] = cbFn;

            retVal = true;
        }

        return retVal;
    }

    bool RemoveDataStreamCallback(uint8_t id)
    {
        idMaker_.ReturnId(id);

        return id__cbFn_.erase(id);
    }

    void Distribute(const vector<uint8_t> &data)
    {
        // distribute
        for (auto &[id, cbFn] : id__cbFn_)
        {
            // default to writing back to the uart that sent the data
            // UartTarget target(uart_);    // ehh, let's keep this functionality very simple

            // fire callback
            cbFn(data);
        }
    }

    uint8_t GetCallbackCount()
    {
        return id__cbFn_.size();
    }

private:

    UART uart_;
    IDMaker<32> idMaker_;
    unordered_map<uint8_t, function<void(const vector<uint8_t> &data)>> id__cbFn_;
};

class LineStreamDistributor
{
public:
    LineStreamDistributor(UART uart, uint16_t maxLineLen, const char *name)
    : uart_(uart)
    , maxLineLen_(maxLineLen)
    , name_(name)
    {
        // Reserve id of 0 for special purposes
        idMaker_.GetNextId();   // 0
    }

    pair<bool, uint8_t> AddLineStreamCallback(function<void(const string &line)> cbFn, bool hideBlankLines = true)
    {
        auto [ok, id] = idMaker_.GetNextId();

        if (ok)
        {
            id__data_[id] = Data{cbFn, hideBlankLines};
        }

        return { ok, id };
    }

    bool SetLineStreamCallback(uint8_t id, function<void(const string &line)> cbFn, bool hideBlankLines = true)
    {
        bool retVal = false;

        if (id__data_.contains(id) || id == 0)
        {
            id__data_[id] = Data{cbFn, hideBlankLines};

            retVal = true;
        }

        return retVal;
    }

    bool RemoveLineStreamCallback(uint8_t id)
    {
        idMaker_.ReturnId(id);
        
        return id__data_.erase(id);
    }

    void AddData(const vector<uint8_t> &data)
    {
        if (id__data_.empty()) { return; }

        for (int i = 0; i < (int)data.size(); ++i)
        {
            char c = data[i];

            if (c == '\n' || c == '\r' || inputStream_.size() == maxLineLen_)
            {
                // remember if max line len hit
                bool wasMaxLine = inputStream_.size() == maxLineLen_;

                // make a copy (move) of the input stream on the way in
                Evm::QueueLowPriorityWork(name_, [this, inputStream = move(inputStream_)](){
                    // distribute

                    // handle case where the callback leads to the caller
                    // de-registering itself and invalidating the id and callback fn
                    // itself.
                    //
                    // this was happening with gps getting a lock, bubbling the lock
                    // event, which then says ok no need to listen anymore, which
                    // found its way back to deregistering, all while the initial
                    // callback was still executing
                    auto tmp = id__data_;
                    for (auto &[id, data] : tmp)
                    {
                        if (inputStream.size() || data.hideBlankLines == false)
                        {
                            // default to writing back to the uart that sent the data
                            UartTarget target(uart_);

                            // fire callback
                            data.cbFn(inputStream);
                        }
                    }
                });

                // clear line cache
                inputStream_.clear();

                // wind forward to next newline if needed
                if (wasMaxLine)
                {
                    for (; i < (int)data.size(); ++i)
                    {
                        if (data[i] == '\n')
                        {
                            break;
                        }
                    }
                }
            }
            else if ((isprint(c) || c == ' ' || c == '\t'))
            {
                inputStream_.push_back(c);
            }
        }
    }

    uint8_t GetCallbackCount()
    {
        return id__data_.size();
    }

    uint32_t Size() const
    {
        return inputStream_.size();
    }

    uint32_t Clear()
    {
        uint32_t size = inputStream_.size();

        inputStream_.clear();

        return size;
    }

    uint32_t ClearInFlight()
    {
        return Evm::ClearLowPriorityWorkByLabel(name_);
    }

private:
    UART uart_;
    uint16_t maxLineLen_;
    const char *name_;
    IDMaker<32> idMaker_;
    struct Data
    {
        function<void(const string &)> cbFn = [](const string &){};
        bool hideBlankLines = true;
    };
    unordered_map<uint8_t, Data> id__data_;
    string inputStream_;
};


static const uint16_t UART_INPUT_PIPE_SIZE = 1002;
static const uint16_t UART_INPUT_MAX_LINE_LEN = 1000;

static DataStreamDistributor UART_0_INPUT_DATA_STREAM_DISTRIBUTOR(UART::UART_0);
static vector<uint8_t> UART_0_INPUT_VEC;
static LineStreamDistributor UART_0_INPUT_LINE_STREAM_DISTRIBUTOR(UART::UART_0, UART_INPUT_MAX_LINE_LEN, "UART_0_LINE_STREAM_DISTRIBUTOR");
static KSemaphore UART_0_INPUT_SEM;

static DataStreamDistributor UART_1_INPUT_DATA_STREAM_DISTRIBUTOR(UART::UART_1);
static vector<uint8_t> UART_1_INPUT_VEC;
static LineStreamDistributor UART_1_INPUT_LINE_STREAM_DISTRIBUTOR(UART::UART_1, UART_INPUT_MAX_LINE_LEN, "UART_1_LINE_STREAM_DISTRIBUTOR");
static KSemaphore UART_1_INPUT_SEM;

static DataStreamDistributor UART_USB_INPUT_DATA_STREAM_DISTRIBUTOR(UART::UART_USB);
static LineStreamDistributor UART_USB_INPUT_LINE_STREAM_DISTRIBUTOR(UART::UART_USB, UART_INPUT_MAX_LINE_LEN, "UART_USB_LINE_STREAM_DISTRIBUTOR");

static const uint8_t READ_BUF_SIZE = 50;
static uint8_t READ_BUF[READ_BUF_SIZE];


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
static void UartInterruptHandlerUartX(uart_inst_t               *uart,
                                      vector<uint8_t>           &vec,
                                      DataStreamDistributor     &dist,
                                      KMessagePipe<char, 1000U> &pipe,
                                      KSemaphore                &sem)
{
    // do some writing
    while (pipe.Count() && uart_is_writable(uart))
    {
        char ch;
        if (pipe.Get(ch))
        {
            uart_putc_raw(uart, ch);
        }
    }

    // clear "tell me when to feed the next bpyte" interrupt
    if (pipe.Count() == 0)
    {
        uart_get_hw(uart)->icr = UART_UARTICR_TXIC_BITS ;
    }

    // do some reading
    bool dataReady = false;
    while (uart_is_readable(uart))
    {
        uint8_t ch = uart_getc(uart);

        // is there someone who wants it and a place to put it?
        if (dist.GetCallbackCount() == 0)
        {
            vec.clear();
        }
        else if (vec.size() < UART_INPUT_PIPE_SIZE)
        {
            vec.push_back(ch);

            dataReady = true;
        }
    }

    if (dataReady)
    {
        sem.Give();
    }
}

static void UartInterruptHandlerUart0()
{
    UartInterruptHandlerUartX(uart0,
                              UART_0_INPUT_VEC,
                              UART_0_INPUT_DATA_STREAM_DISTRIBUTOR,
                              UART_0_OUTPUT_PIPE,
                              UART_0_INPUT_SEM);
}

static void UartInterruptHandlerUart1()
{
    UartInterruptHandlerUartX(uart1,
                              UART_1_INPUT_VEC,
                              UART_1_INPUT_DATA_STREAM_DISTRIBUTOR,
                              UART_1_OUTPUT_PIPE,
                              UART_1_INPUT_SEM);
}


////////////////////////////////////////////////////////////////////////////////
// Threads dedicated to dealing with uart at low priority
////////////////////////////////////////////////////////////////////////////////

static void ThreadFnUartXInput(vector<uint8_t>       &vec,
                               DataStreamDistributor &dist,
                               KSemaphore            &sem)
{
    while (true)
    {
        if (sem.Take())
        {
            // lock out interrupts so we can safely manipulate the captured data vector
            IrqLock lock;
            PAL.SchedulerLock();

            // pass all the data to the line stream
            dist.Distribute(vec);

            // erase all data
            vec.clear();
            PAL.SchedulerUnlock();
        }
    }
}

static void ThreadFnUART0Input()
{
    ThreadFnUartXInput(UART_0_INPUT_VEC,
                       UART_0_INPUT_DATA_STREAM_DISTRIBUTOR,
                       UART_0_INPUT_SEM);
}

static void ThreadFnUART1Input()
{
    ThreadFnUartXInput(UART_1_INPUT_VEC,
                       UART_1_INPUT_DATA_STREAM_DISTRIBUTOR,
                       UART_1_INPUT_SEM);
}

static const uint32_t STACK_SIZE = 1024;
static const uint8_t  PRIORITY   = 10;
static KTask<STACK_SIZE> uart0Input("UART0_INPUT", ThreadFnUART0Input, PRIORITY);
static KTask<STACK_SIZE> uart1Input("UART1_INPUT", ThreadFnUART1Input, PRIORITY);


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

static void UartSetDefault(UART uart)
{
    uartStack[0] = uart;
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
                PAL.SchedulerLock();

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

                PAL.SchedulerUnlock();
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
// UART Clear Buffer Interface
////////////////////////////////////////////////////////////////////////////////

void UartClearRxBuffer(UART uart)
{
    // clear the queues seen below
    if (uart == UART::UART_1)
    {
        UART_1_INPUT_VEC.clear();
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

static void UartInitDevice(uart_inst_t *uart, uint8_t pinRx, uint8_t pinTx)
{
    // https://lucidar.me/en/serialib/most-used-baud-rates-table/

    // static const uint32_t BAUD_RATE = 9600;
    // static const uint32_t BAUD_RATE = 57600;
    static const uint32_t BAUD_RATE = 76800;

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(pinTx, GPIO_FUNC_UART);
    gpio_set_function(pinRx, GPIO_FUNC_UART);

    // do init
    uart_init(uart, BAUD_RATE);

    // Turn off FIFO's - we want to do this character by character
    // uart_set_fifo_enabled(uart, false);
}

static void UartInitDeviceInterrupts(uart_inst_t *uart, irq_handler_t handler)
{
    // Set up a RX interrupt
    // We need to set up the handler first
    // Select correct interrupt for the UART we are using
    int UART_IRQ = uart == uart0 ? UART0_IRQ : UART1_IRQ;

    // And set up and enable the interrupt handlers
    // irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
    irq_set_exclusive_handler(UART_IRQ, handler);
    irq_set_enabled(UART_IRQ, true);

    // Now enable the UART to send interrupts - RX only
    // uart_set_irq_enables(uart, true, false);
    uart_set_irq_enables(uart, true, true);
}

#include "Timeline.h"
void UartInit()
{
    Timeline::Global().Event("UartInit");

    // Set up stdio
    stdio_init_all();

    // pre-allocate buffer so no need to do so under ISR
    UART_0_INPUT_VEC.reserve(UART_INPUT_PIPE_SIZE);
    UART_1_INPUT_VEC.reserve(UART_INPUT_PIPE_SIZE);

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

    // set up raw IRQ handling
    UartInitDevice(uart0, 1, 0);
    UartInitDevice(uart1, 9, 8);
    UartInitDeviceInterrupts(uart0, UartInterruptHandlerUart0);
    UartInitDeviceInterrupts(uart1, UartInterruptHandlerUart1);

    // trigger interrupts to kick off any already-queued data
    irq_set_pending(UART0_IRQ);
    irq_set_pending(UART1_IRQ);

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
        Log("- irq queued    : ", UART_0_INPUT_VEC.size());
        Log("- raw listeners : ", UART_0_INPUT_DATA_STREAM_DISTRIBUTOR.GetCallbackCount() - 1);
        Log("- line listeners: ", UART_0_INPUT_LINE_STREAM_DISTRIBUTOR.GetCallbackCount());
        Log("  - queued      : ", UART_0_INPUT_LINE_STREAM_DISTRIBUTOR.Size());
        LogNL();
        Log("UART_1");
        Log("- irq queued    : ", UART_1_INPUT_VEC.size());
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

    // Shell::AddCommand("uart.uart1", [](vector<string> argList){
    //     if (argList[0] == "on") { UartEnable(UART::UART_1);  }
    //     else                    { UartDisable(UART::UART_1); }
    // }, { .argCount = 1, .help = "UART1 <on/off>"});

    Shell::AddCommand("uart.uart1.clear.rx", [](vector<string> argList){
        PAL.DelayBusy(2000);
        UartClearRxBuffer(UART::UART_1);
    }, { .argCount = 0, .help = "Clear UART1 RX buffer"});

    Shell::AddCommand("uart.uart1.clear.tx", [](vector<string> argList){
        UartClearTxBuffer(UART::UART_1);
    }, { .argCount = 0, .help = "Clear UART1 TX buffer"});
}

