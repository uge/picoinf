#pragma once

#include <functional>
#include <string>
#include <utility>
using namespace std;


extern void UartInit();

// Allow code to decide which UART it wants to send data to
enum class UART : uint8_t
{
    UART_0,
    UART_1,
    UART_USB,
};

extern void UartPush(UART uart);
extern void UartPop();
extern UART UartCurrent();

class UartTarget
{
public:
    UartTarget(UART uart)
    {
        UartPush(uart);
    }

    ~UartTarget()
    {
        UartPop();
    }
};

// Output - Synchronous
extern void UartOut(char c);
extern void UartOut(const char *str);

// Output - Async via thread
extern void UartSend(const uint8_t *buf, uint16_t bufLen);
extern void UartSend(const vector<uint8_t> &byteList);

// Input handling for raw data streams
// Low frills (no auto-uart re-direct of output), cb executes in smaller thread stack
extern pair<bool, uint8_t> UartAddDataStreamCallback(UART uart, function<void(const vector<uint8_t> &data)> cbFn);
extern bool                UartSetDataStreamCallback(UART uart, function<void(const vector<uint8_t> &data)> cbFn, uint8_t id = 0);
extern bool                UartRemoveDataStreamCallback(UART uart, uint8_t id = 0);

// Input handling for ASCII-only lines
// high frills (auto-redirect output), cb executes in main evm thread
extern pair<bool, uint8_t> UartAddLineStreamCallback(UART uart, function<void(const string &line)> cbFn, bool hideBlankLines = true);
extern bool                UartSetLineStreamCallback(UART uart, function<void(const string &line)> cbFn, uint8_t id = 0, bool hideBlankLines = true);
extern bool                UartRemoveLineStreamCallback(UART uart, uint8_t id = 0);





// Turn on or off
extern void UartClearRxBuffer(UART uart);
extern void UartClearTxBuffer(UART uart);

extern uint32_t UartGetBaud(UART uart);

