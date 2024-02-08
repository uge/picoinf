#pragma once

#include <stdint.h>

#include <array>
#include <string>
#include <vector>
using namespace std;


////////////////////////////////////////////////////////////////////////////////
// Mode selection
////////////////////////////////////////////////////////////////////////////////

extern void LogModeSync();
extern void LogModeAsync();


////////////////////////////////////////////////////////////////////////////////
// Basic
////////////////////////////////////////////////////////////////////////////////

extern void LogNL();
extern void LogNL(uint8_t count);


////////////////////////////////////////////////////////////////////////////////
// Strings
////////////////////////////////////////////////////////////////////////////////

extern void LogNNL(const char *str);
extern void Log(const char *str);
extern void LogNNL(char *str);
extern void Log(char *str);
extern void LogNNL(const std::string &str);
extern void Log(const std::string &str);
extern void LogNNL(const std::string_view &str);
extern void Log(const std::string_view &str);


////////////////////////////////////////////////////////////////////////////////
// Chars
////////////////////////////////////////////////////////////////////////////////

extern void LogNNL(char val);
extern void Log(char val);
extern void LogXNNL(char val, uint8_t count);
extern void LogX(char val, uint8_t count);


////////////////////////////////////////////////////////////////////////////////
// Unsigned Ints
////////////////////////////////////////////////////////////////////////////////

extern void LogNNL(uint64_t val);
extern void Log(uint64_t val);
extern void LogNNL(uint32_t val);
extern void Log(uint32_t val);
extern void LogNNL(uint16_t val);
extern void Log(uint16_t val);
extern void LogNNL(uint8_t val);
extern void Log(uint8_t val);


////////////////////////////////////////////////////////////////////////////////
// Signed Ints
////////////////////////////////////////////////////////////////////////////////

extern void LogNNL(int32_t val);
extern void Log(int32_t val);
extern void LogNNL(int16_t val);
extern void Log(int16_t val);
extern void LogNNL(int8_t val);
extern void Log(int8_t val);


////////////////////////////////////////////////////////////////////////////////
// Floating Point
////////////////////////////////////////////////////////////////////////////////

extern void LogNNL(double val);
extern void Log(double val);


////////////////////////////////////////////////////////////////////////////////
// Bool
////////////////////////////////////////////////////////////////////////////////

extern void LogNNL(bool val);
extern void Log(bool val);


////////////////////////////////////////////////////////////////////////////////
// Hex
////////////////////////////////////////////////////////////////////////////////

extern void LogHexNNL(const uint8_t *buf, uint8_t bufLen, bool withSpaces = true);
extern void LogHex(const uint8_t *buf, uint8_t bufLen, bool withSpaces = true);


////////////////////////////////////////////////////////////////////////////////
// Containers
////////////////////////////////////////////////////////////////////////////////

template <typename T>
void LogNNL(const vector<T> &valList)
{
    LogNNL("[");
    const char *sep = "";
    for (const auto val : valList)
    {
        LogNNL(sep);
        LogNNL(val);

        sep = ", ";
    }
    LogNNL("]");
}

template <typename T>
void Log(const vector<T> &valList)
{
    LogNNL(valList);
    LogNL();
}

template <typename T, size_t S>
void LogNNL(const array<T, S> &valList)
{
    LogNNL("[");
    const char *sep = "";
    for (const auto val : valList)
    {
        LogNNL(sep);
        LogNNL(val);

        sep = ", ";
    }
    LogNNL("]");
}

template <typename T, size_t S>
void Log(const array<T, S> &valList)
{
    LogNNL(valList);
    LogNL();
}


////////////////////////////////////////////////////////////////////////////////
// Templates
////////////////////////////////////////////////////////////////////////////////

extern void Log();
extern void LogNNL();

template <typename T>
void LogNNL(T *val)
{
    LogNNL((uint32_t)val);
}

template <typename T>
void Log(T *val)
{
    Log((uint32_t)val);
    LogNL();
}

template <typename T1, typename T2, typename ...Ts>
void LogNNL(T1 &&val1, T2 &&val2, Ts &&...args)
{
    LogNNL(val1);
    LogNNL(val2);
    LogNNL(args...);
}

template <typename T1, typename T2, typename ...Ts>
void Log(T1 &&val1, T2 &&val2, Ts &&...args)
{
    LogNNL(val1, val2, args...);
    LogNL();
}

template <typename T>
void LogXNNL(T &&val, uint8_t count)
{
    for (uint8_t i = 0; i < count; ++i)
    {
        LogNNL(val);
    }
}

template <typename T>
void LogX(T &&val, uint8_t count)
{
    LogXNNL(val, count);
    LogNL();
}


////////////////////////////////////////////////////////////////////////////////
// LogBlob
////////////////////////////////////////////////////////////////////////////////

extern void LogBlob(uint8_t *buf, uint16_t bufSize, uint8_t showBin = 0, uint8_t showHex = 1);

extern void LogBlob(const vector<uint8_t> &byteList, uint8_t showBin = 0, uint8_t showHex = 1);

template <typename T, typename U>
void LogBlob(T *buf, U bufSize, uint8_t showBin = 0, uint8_t showHex = 1)
{
    LogBlob((uint8_t *)buf, (uint16_t)bufSize, showBin, showHex);
}



