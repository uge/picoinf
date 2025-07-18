#pragma once

#include <functional>
#include <string>
#include <utility>


extern void UartInit();
extern void UartSetupShell();

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
extern void UartSend(const std::vector<uint8_t> &byteList);

// Input handling for raw data streams
// Low frills (no auto-uart re-direct of output), cb executes in smaller thread stack
extern std::pair<bool, uint8_t> UartAddDataStreamCallback(UART uart, std::function<void(const std::vector<uint8_t> &data)> cbFn);
extern bool                     UartSetDataStreamCallback(UART uart, std::function<void(const std::vector<uint8_t> &data)> cbFn, uint8_t id = 0);
extern bool                     UartRemoveDataStreamCallback(UART uart, uint8_t id = 0);

// Input handling for ASCII-only lines
// high frills (auto-redirect output), cb executes in main evm thread
extern std::pair<bool, uint8_t> UartAddLineStreamCallback(UART uart, std::function<void(const std::string &line)> cbFn, bool hideBlankLines = true);
extern bool                     UartSetLineStreamCallback(UART uart, std::function<void(const std::string &line)> cbFn, uint8_t id = 0, bool hideBlankLines = true);
extern bool                     UartRemoveLineStreamCallback(UART uart, uint8_t id = 0);



// Enable/Disable
extern void UartEnable(UART uart);
extern void UartDisable(UART uart);
extern bool UartIsEnabled(UART uart);

// Turn on or off
extern void UartClearRxBuffer(UART uart);
extern void UartClearTxBuffer(UART uart);

