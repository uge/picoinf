#include "KMessagePassing.h"
#include "KTask.h"
#include "PAL.h"
// #include "Shell.h"
#include "UART.h"
#include "IDMaker.h"

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



////////////////////////////////////////////////////////////////////////////////
// UART Output state
////////////////////////////////////////////////////////////////////////////////

static const uint16_t UART_OUTPUT_PIPE_SIZE = 1000;
static KMessagePipe<char, UART_OUTPUT_PIPE_SIZE> UART_0_OUTPUT_PIPE;
static KMessagePipe<char, UART_OUTPUT_PIPE_SIZE> UART_1_OUTPUT_PIPE;
static KMessagePipe<char, UART_OUTPUT_PIPE_SIZE> UART_USB_OUTPUT_PIPE;


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

            if (c == '\n' || inputStream_.size() == maxLineLen_)
            {
                // remember if max line len hit
                bool wasMaxLine = inputStream_.size() == maxLineLen_;

                // TODO
                #if 0
                // make a copy (move) of the input stream on the way in
                Evm::QueueLowPriorityWork(name_, [this, inputStream = move(inputStream_)](){
                #else
                auto inputStream = move(inputStream_);
                #endif
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
                #if 0
                });
                #endif


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

    uint32_t Clear()
    {
        uint32_t size = inputStream_.size();

        inputStream_.clear();

        return size;
    }

    uint32_t ClearInFlight()
    {
        // TODO
        // return Evm::ClearLowPriorityWorkByLabel(name_);
        return 1;
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
static vector<uint8_t> UART_USB_INPUT_VEC;
static LineStreamDistributor UART_USB_INPUT_LINE_STREAM_DISTRIBUTOR(UART::UART_USB, UART_INPUT_MAX_LINE_LEN, "UART_USB_LINE_STREAM_DISTRIBUTOR");
static KSemaphore UART_USB_INPUT_SEM;

static const uint8_t READ_BUF_SIZE = 50;
static uint8_t READ_BUF[READ_BUF_SIZE];


////////////////////////////////////////////////////////////////////////////////
// UART Output
////////////////////////////////////////////////////////////////////////////////

vector<UART> uartStack = { UART::UART_0 };

static uart_inst_t *UartGetActiveInstance()
{
    uart_inst_t *activeInstance = uart0;

    UART uart = uartStack.back();

    if      (uart == UART::UART_1)   { activeInstance = uart1; }
    // else if (uart == UART::UART_USB) { activeInstance = DEV_UART_USB; }
    
    return activeInstance;
}

static KMessagePipe<char, UART_OUTPUT_PIPE_SIZE> &UartGetPipe()
{
    KMessagePipe<char, UART_OUTPUT_PIPE_SIZE> *ptr = &UART_0_OUTPUT_PIPE;

    UART uart = uartStack.back();

    if      (uart == UART::UART_1)   { ptr = &UART_1_OUTPUT_PIPE; }
    else if (uart == UART::UART_USB) { ptr = &UART_USB_OUTPUT_PIPE; }
    
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

// TODO
#include "Pin.h"
static void UartInterruptHandlerUart0()
{
    // TODO
    static Pin p(14);
    p.DigitalToggle();
    UartInterruptHandlerUartX(uart0,
                              UART_0_INPUT_VEC,
                              UART_0_INPUT_DATA_STREAM_DISTRIBUTOR,
                              UART_0_OUTPUT_PIPE,
                              UART_0_INPUT_SEM);
    p.DigitalToggle();
}

static void UartInterruptHandlerUart1()
{
    // TODO
    static Pin p(15);
    p.DigitalToggle();
    UartInterruptHandlerUartX(uart1,
                              UART_1_INPUT_VEC,
                              UART_1_INPUT_DATA_STREAM_DISTRIBUTOR,
                              UART_1_OUTPUT_PIPE,
                              UART_1_INPUT_SEM);
    p.DigitalToggle();
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
static const uint8_t  PRIORITY   = 1;
static KTask<STACK_SIZE> uart0Input("UART0_INPUT", ThreadFnUART0Input, PRIORITY);
static KTask<STACK_SIZE> uart1Input("UART1_INPUT", ThreadFnUART1Input, PRIORITY);


#if 0
//
// UART USB
//
static void ThreadFnUARTUSBOutput()
{
    bool skippedLast = false;
    while (true)
    {
        char c;
        if (UART_USB_OUTPUT_PIPE.Get(c, K_FOREVER))
        {
            // consume the bytes, but only attempt to output if USB serial connected
            uint32_t dtr;
            uart_line_ctrl_get(DEV_UART_USB, UART_LINE_CTRL_DTR, &dtr);
            if (dtr)
            {
                // sad kludge to protect against:
                // - sending a stream of data to an endpoint which disconnects midway through
                // - unknowingly queuing a few additional bytes in the USB output buffer
                // - those bytes lingering and later getting sent on a new connection, corrupting the stream
                //
                // this approach hopes:
                // - the other side is line-oriented and simply discards the malformed message
                // - too few bytes get in the buffer to form a complete message
                if (skippedLast)
                {
                    skippedLast = false;

                    for (int i = 0; i < 6; ++i)
                    {
                        uart_poll_out(DEV_UART_USB, '\n');
                    }
                }
                uart_poll_out(DEV_UART_USB, c);
            }
            else
            {
                skippedLast = true;
            }
        }
    }
}
K_THREAD_DEFINE(ThreadUARTUSBOutput, STACKSIZE, ThreadFnUARTUSBOutput, NULL, NULL, NULL, PRIORITY, 0, 0);

static void ThreadFnUARTUSBInput()
{
    while (true)
    {
        if (UART_USB_INPUT_SEM.Take(K_FOREVER))
        {
            // lock out interrupts so we can safely manipulate the captured data vector
            IrqLock lock;
            PAL.SchedulerLock();

            // pass all the data to the line stream
            UART_USB_INPUT_DATA_STREAM_DISTRIBUTOR.Distribute(UART_USB_INPUT_VEC);

            // erase all data
            UART_USB_INPUT_VEC.clear();
            PAL.SchedulerUnlock();

            // reset the semaphore, all data is drained by now
            UART_USB_INPUT_SEM.Reset();
        }
    }
}
K_THREAD_DEFINE(ThreadUARTUSBInput, STACKSIZE, ThreadFnUARTUSBInput, NULL, NULL, NULL, PRIORITY, 0, 0);
#endif

////////////////////////////////////////////////////////////////////////////////
// UART External API
////////////////////////////////////////////////////////////////////////////////

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
    uart_inst_t *activeInstance = UartGetActiveInstance();

    if (activeInstance == uart0 || activeInstance == uart1)
    {
        uart_putc_raw(activeInstance, c);
    }
    #if 0
    else
    {
        // protect against synchronous logging clogging up the
        // usb serial output

        uint32_t dtr;
        uart_line_ctrl_get(activeInstance, UART_LINE_CTRL_DTR, &dtr);
        if (dtr)
        {
            uart_poll_out(activeInstance, c);
        }
    }
    #endif
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
                irq_set_pending(UartGetActiveInstance() == uart0 ? UART0_IRQ : UART1_IRQ);
            }

            PAL.SchedulerUnlock();
        }
        else
        {
            // I guess it's not going then
        }
    }
}

void UartSend(const vector<uint8_t> &byteList)
{
    UartSend(byteList.data(), byteList.size());
}

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
    static const uint32_t BAUD_RATE = 9600;

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(pinTx, GPIO_FUNC_UART);
    gpio_set_function(pinRx, GPIO_FUNC_UART);

    // do init
    uart_init(uart, BAUD_RATE);

    // Turn off FIFO's - we want to do this character by character
    uart_set_fifo_enabled(uart, false);
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

void UartInit()
{
    // TODO
    // Timeline::Global().Event("UartInit");

    // Set up stdio
    stdio_init_all();

    // pre-allocate buffer so no need to do so under ISR
    UART_0_INPUT_VEC.reserve(UART_INPUT_PIPE_SIZE);
    UART_1_INPUT_VEC.reserve(UART_INPUT_PIPE_SIZE);
    UART_USB_INPUT_VEC.reserve(UART_INPUT_PIPE_SIZE);

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

    // Set up USB serial interface
    // const device *chosenShell = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_shell_uart));
    // if (chosenShell == DEV_UART_USB)
    // {
    //     UartSetDefault(UART::UART_USB);
    // }
    // else
    // {
    //     uart_irq_callback_set(DEV_UART_USB, UartInputIrqHandler);
    //     uart_irq_rx_enable(DEV_UART_USB);
    // }

    // trigger interrupts to kick off any already-queued data
    irq_set_pending(UART0_IRQ);
    irq_set_pending(UART1_IRQ);
}

// TODO
#if 0
int UartSetupShell()
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
        Log("- input queued    : N/A");
        Log("  - raw listeners : N/A");
        Log("  - line listeners: N/A");

        Log("UART_1");
        Log("- input queued    : ", UART_1_INPUT_VEC.size());
        Log("  - raw listeners : ", UART_1_INPUT_DATA_STREAM_DISTRIBUTOR.GetCallbackCount() - 1);
        Log("  - line listeners: ", UART_1_INPUT_LINE_STREAM_DISTRIBUTOR.GetCallbackCount());

        Log("UART_USB");
        Log("- input queued    : ", UART_USB_INPUT_VEC.size());
        Log("  - raw listeners : ", UART_USB_INPUT_DATA_STREAM_DISTRIBUTOR.GetCallbackCount() - 1);
        Log("  - line listeners: ", UART_USB_INPUT_LINE_STREAM_DISTRIBUTOR.GetCallbackCount());

        LogNL();
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

    return 1;
}
#endif

