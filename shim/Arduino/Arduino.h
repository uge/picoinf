#pragma once

#include <string.h>

#include <cmath>

#include "Log.h"
#include "PAL.h"

// https://github.com/arduino/ArduinoCore-avr/blob/master/cores/arduino/Arduino.h


#define MSBFIRST 0
#define LSBFIRST 1

#define LOW  0
#define HIGH 1

static const uint8_t INPUT = 1;
static const uint8_t OUTPUT = 0;

static const uint8_t HEX = 16;


typedef int BitOrder;

// An unbelievable and gross hack.
//
// The ultimate problem is that a lot of picoinf code has been written with
// 'using namespace std' in a lot of header files.
//
// All the header includes ultimately make it up to the main application and
// are comingled there.
//
// Now introducing the Arduino libraries into the mix (not this file, but the
// actual libraries from Adafruit, etc), there is a clash.
//
// The clash is that std::byte is not a typedef or a #define, but instead an
// actual typed enum (char), so it has a type, and can't be assigned to by
// anything other than that type.
//
// The Arduino-based libs are assigning things like unnamed/untyped enums to
// values which were defined as 'byte val' or whatever. This breaks compilation.
//
// If I introduce another typedef, then there is a clash.
//
// So the gross hack is to:
// - know the specific sequence of includes that lead to the main application
// - cut off the clash at the earliest point possible
//   - that is in the sensor library
// - the cut is done by:
//   - defining (temporarily) the word 'byte' to be a valid c++ type (uint8_t)
//   - letting arduino-based libs get included
//   - undefining the word byte before any std::byte ever gets involved
//
// The right way to fix this would be to:
// - stop 'using namespace std' in headers
//   - I guess I could still do using std::vector, etc, and just list them out instead of
//     needing to completely re-write the headers to be typed
// - then build layers of libraries and private includes/etc
//   - not even thought-through enough to know for sure this would work
//
// Do this for now and see how it goes.
//
#define byte uint8_t
#define PI 3.1415926535897932384626433832795


inline int digitalPinToPort(int8_t)
{
    return 0;
}

inline uint32_t *portOutputRegister(int)
{
    static uint32_t reg;

    return &reg;
}

inline uint32_t *portInputRegister(int)
{
    static uint32_t reg;

    return &reg;
}

inline uint32_t digitalPinToBitMask(int8_t)
{
    return 0;
}

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}

template <typename T1, typename T2>
inline int bitRead(T1 value, T2 bit)
{
    return (((value) >> (bit)) & 0x01);
}

template <typename T1, typename T2>
inline int bitSet(T1 value, T2 bit)
{
    return ((value) |= (1UL << (bit)));
}

template <typename T1, typename T2>
inline int bitClear(T1 value, T2 bit)
{
    return ((value) &= ~(1UL << (bit)));
}

template <typename T1, typename T2>
inline int bitToggle(T1 value, T2 bit)
{
    return ((value) ^= (1UL << (bit)));
}

template <typename T1, typename T2, typename T3>
inline int bitWrite(T1 value, T2 bit, T3 bitvalue)
{
    return ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit));
}


inline const char *F(const char *t)
{
    return t;
}

inline void delay(uint32_t ms)
{
    PAL.Delay(ms);
}

inline void delayMicroseconds(uint32_t us)
{
    PAL.DelayUs(us);
}

inline uint64_t millis()
{
    return PAL.Millis();
}

inline void yield() {}

template <typename T1, typename T2, typename T3>
T1 constrain(T1 amt, T2 low, T3 high)
{
    T1 lowVal = (T1)low;
    T1 highVal = (T1)high;

    if (amt < lowVal)
    {
        return lowVal;
    }
    else
    {
        if (amt > highVal)
        {
            return highVal;
        }
        else
        {
            return amt;
        }
    }
}


class Stream
{
public:
    template <typename T>
    static void print(const T &val)
    {
        LogNNL(val);
    }

    template <typename T>
    static void print(const T &val, int)
    {
        LogNNL(val);
    }

    template <typename T>
    static void println(const T &val)
    {
        Log(val);
    }

    static void println()
    {
        LogNL();
    }
};

class SerialClass
: public Stream
{
public:
};

extern SerialClass Serial;

