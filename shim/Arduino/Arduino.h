#pragma once

#include <string.h>

#include <cmath>

#include "PAL.h"


#define MSBFIRST 0
#define LSBFIRST 1

typedef int BitOrder;

typedef uint8_t byte;


inline const char *F(const char *t)
{
    return t;
}

inline void delay(uint32_t ms)
{
    PAL.Delay(ms);
}

inline uint64_t millis()
{
    return PAL.Millis();
}

class SerialClass
{
public:
    template <typename T>
    void print(const T &)
    {
    }

    template <typename T>
    void println(const T &)
    {
    }

    void println()
    {
    }
};

extern SerialClass Serial;

