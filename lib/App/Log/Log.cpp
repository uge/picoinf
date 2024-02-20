#include "Log.h"

#include <stdio.h>
#include <string.h>

#include "PAL.h"
#include "Shell.h"
#include "UART.h"
#include "Format.h"
#include "Timeline.h"


////////////////////////////////////////////////////////////////////////////////
// Intercept libc output and direct to UART
////////////////////////////////////////////////////////////////////////////////

int _write(int file, char *ptr, int len)
{
    (void)file;

    UartSend((uint8_t *)ptr, len);

    return len;
}

int _write_r(struct _reent *r, int file, const void *ptr, int len)
{
    (void)r;

    return _write(file, (char *)ptr, len);
}

// Picolibc notes on how to do intercept more than this.
// Maybe pick this up later, I do want to be able to
// capture the output from the logging, printf, printk, etc
// https://github.com/picolibc/picolibc/blob/main/doc/os.md


////////////////////////////////////////////////////////////////////////////////
// Mode selection
////////////////////////////////////////////////////////////////////////////////

void LogModeSync()
{
    PAL.EnableForcedInIsrYes(true);
}

void LogModeAsync()
{
    PAL.EnableForcedInIsrYes(false);
}


////////////////////////////////////////////////////////////////////////////////
// Enable buffered UART
////////////////////////////////////////////////////////////////////////////////

template <typename T>
void FormatAndUartSend(const char *fmt, T val)
{
    char bufFmt[FormatBase::BUF_SIZE];

    auto [buf, bufSize] = Format::StrC(bufFmt, FormatBase::BUF_SIZE, fmt, val);
    if (bufSize > 0)
    {
        UartSend((uint8_t *)buf, bufSize);
    }
}


////////////////////////////////////////////////////////////////////////////////
// Basic
////////////////////////////////////////////////////////////////////////////////

void LogNL()
{
    UartSend((uint8_t *)"\n", 1);
}

void LogNL(uint8_t count)
{
    for (uint16_t i = 0; i < count; ++i)
    {
        LogNL();
    }
}


////////////////////////////////////////////////////////////////////////////////
// Strings
////////////////////////////////////////////////////////////////////////////////

void LogNNL(const char *str)
{
    if (str)
    {
        UartSend((uint8_t *)str, strlen(str));
    }
}

void Log(const char *str)
{
    if (str)
    {
        LogNNL(str);
    }
    LogNL();
}

void LogNNL(char *str)
{
    LogNNL((const char *)str);
}

void Log(char *str)
{
    Log((const char *)str);
}

void LogNNL(const std::string &str)
{
    LogNNL(str.c_str());
}

void Log(const std::string &str)
{
    LogNNL(str.c_str());
    LogNL();
}

void LogNNL(const std::string_view &str)
{
    UartSend((uint8_t *)str.data(), str.size());
}

void Log(const std::string_view &str)
{
    LogNNL(str);
    LogNL();
}



////////////////////////////////////////////////////////////////////////////////
// Chars
////////////////////////////////////////////////////////////////////////////////

void LogNNL(char val)
{
    UartSend((uint8_t *)&val, 1);
}

void Log(char val)
{
    LogNNL(val);
    LogNL();
}

void LogXNNL(char val, uint8_t count)
{
    for (uint8_t i = 0; i < count; ++i)
    {
        LogNNL(val);
    }
}

void LogX(char val, uint8_t count)
{
    LogXNNL(val, count);
    LogNL();
}


////////////////////////////////////////////////////////////////////////////////
// Unsigned Ints
////////////////////////////////////////////////////////////////////////////////

void LogNNL(uint64_t val)
{
    FormatAndUartSend("%lld", val);
}

void Log(uint64_t val)
{
    LogNNL(val);
    LogNL();
}

void LogNNL(unsigned int val)
{
    FormatAndUartSend("%u", val);
}

void Log(unsigned int val)
{
    LogNNL(val);
    LogNL();
}

void LogNNL(uint32_t val)
{
    FormatAndUartSend("%u", val);
}

void Log(uint32_t val)
{
    LogNNL(val);
    LogNL();
}

void LogNNL(uint16_t val)
{
    LogNNL((uint32_t)val);
}

void Log(uint16_t val)
{
    Log((uint32_t)val);
}

void LogNNL(uint8_t val)
{
    LogNNL((uint32_t)val);
}

void Log(uint8_t val)
{
    Log((uint32_t)val);
}


////////////////////////////////////////////////////////////////////////////////
// Signed Ints
////////////////////////////////////////////////////////////////////////////////

void LogNNL(int val)
{
    FormatAndUartSend("%i", val);
}

void Log(int val)
{
    LogNNL(val);
    LogNL();
}

void LogNNL(int32_t val)
{
    FormatAndUartSend("%i", val);
}

void Log(int32_t val)
{
    LogNNL(val);
    LogNL();
}

void LogNNL(int16_t val)
{
    LogNNL((int32_t)val);
}

void Log(int16_t val)
{
    Log((int32_t)val);
}

void LogNNL(int8_t val)
{
    LogNNL((int32_t)val);
}

void Log(int8_t val)
{
    Log((int32_t)val);
}


////////////////////////////////////////////////////////////////////////////////
// Floating Point
////////////////////////////////////////////////////////////////////////////////

void LogNNL(double val)
{
    FormatAndUartSend("%0.3f", val);
}

void Log(double val)
{
    LogNNL(val);
    LogNL();
}


////////////////////////////////////////////////////////////////////////////////
// Bool
////////////////////////////////////////////////////////////////////////////////

void LogNNL(bool val)
{
    FormatAndUartSend("%s", val ? "true" : "false");
}   

void Log(bool val)
{
    LogNNL(val);
    LogNL();
}


////////////////////////////////////////////////////////////////////////////////
// Hex
////////////////////////////////////////////////////////////////////////////////

void LogHexNNL(const uint8_t *buf, uint8_t bufLen, bool withSpaces)
{
    if (buf && bufLen)
    {
        const char *sep = "";
        for (int i = 0; i < bufLen; ++i)
        {
            if (withSpaces && i)
            {
                sep = " ";
            }

            UartSend((uint8_t *)sep, strlen(sep));
            FormatAndUartSend("%02X", buf[i]);
        }
    }
}

void LogHex(const uint8_t *buf, uint8_t bufLen, bool withSpaces)
{
    LogHexNNL(buf, bufLen);
    LogNL();
}


////////////////////////////////////////////////////////////////////////////////
// Templates
////////////////////////////////////////////////////////////////////////////////

void Log()
{
    // Nothing to do, simply here to make the templating work
}

void LogNNL()
{
    // Nothing to do, simply here to make the templating work
}


////////////////////////////////////////////////////////////////////////////////
// LogBlob
////////////////////////////////////////////////////////////////////////////////

// only print up to 8 bytes
uint8_t LogBlobRow(uint16_t  byteCount,
                   uint8_t  *buf,
                   uint16_t  bufSize,
                   uint8_t   showBin,
                   uint8_t   showHex)
{
    char bufSprintf[9];

    sprintf(bufSprintf, "%08X", byteCount);
    
    // Print byte start
    LogNNL("0x", bufSprintf, ": ");
    
    // Calculate how many real vs pad bytes to output
    uint8_t realBytes = min(8, (int)bufSize);
    uint8_t padBytes = 8 - realBytes;
    
    // Print hex
    if (showHex)
    {
        for (uint8_t i = 0; i < realBytes; ++i)
        {
            uint8_t b = buf[i];
            
            sprintf(bufSprintf, "%02X", b);
            LogNNL(bufSprintf);
            LogNNL(' ');
        }
        
        for (uint8_t i = 0; i < padBytes; ++i)
        {
            LogXNNL(' ', 3);
        }
        
        LogNNL("| ");
    }
    
    // Print binary
    if (showBin)
    {
        for (uint8_t i = 0; i < realBytes; ++i)
        {
            uint8_t b = buf[i];
            
            for (uint8_t j = 0; j < 8; ++j)
            {
                LogNNL((uint8_t)((b & 0x80) ? 1 : 0));
                
                b <<= 1;
            }
            
            LogNNL(' ');
        }
        
        for (uint8_t i = 0; i < padBytes; ++i)
        {
            LogXNNL(' ', 9);
        }
        
        LogNNL("| ");
    }
    
    // Print visible
    for (uint8_t i = 0; i < realBytes; ++i)
    {
        char b = buf[i];
        
        if (isprint(b))
        {
            LogNNL(b);
        }
        else
        {
            LogNNL('.');
        }
    }

    LogNL();

    return realBytes;
}

void LogBlob(uint8_t  *buf,
             uint16_t  bufSize,
             uint8_t   showBin,
             uint8_t   showHex)
{
    uint16_t bufSizeRemaining = bufSize;
    uint16_t byteCount        = 0;
    
    while (bufSizeRemaining)
    {
        uint8_t realBytes = LogBlobRow(byteCount, buf, bufSize, showBin, showHex);

        // work through the buffer
        buf += realBytes;
        bufSize -= realBytes;
        
        // Account for used data
        byteCount += realBytes;
        bufSizeRemaining -= realBytes;
    }
}

void LogBlob(const vector<uint8_t> &byteList, uint8_t showBin, uint8_t showHex)
{
    LogBlob(byteList.data(), byteList.size(), showBin, showHex);
}




////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////

void LogInit()
{
    Timeline::Global().Event("LogInit");

    LogModeSync();
}

void LogSetupShell()
{
    Timeline::Global().Event("LogSetupShell");

    Shell::AddCommand("log.test", [](vector<string>){
        Log("log.test success");
    }, { .help = "" });

    Shell::AddCommand("log.testnow", [](vector<string>){
        LogModeSync();
        Log("log.testnow success");
        LogModeAsync();
    }, { .help = "" });
}



