#pragma once

#include "Arduino.h"

// https://github.com/arduino/ArduinoCore-avr/blob/master/libraries/Wire/src/Wire.h
// https://github.com/arduino/ArduinoCore-avr/blob/master/libraries/Wire/src/Wire.cpp


class TwoWire
{
public:
    void begin()
    {

    }

    void end()
    {

    }

    void beginTransmission(uint8_t addr)
    {

    }

    uint8_t endTransmission()
    {
        uint8_t retVal = 0;

        return retVal;
    }

    uint8_t endTransmission(uint8_t stop)
    {
        uint8_t retVal = 0;

        return retVal;
    }

    int read()
    {
        int retVal = 0;

        return retVal;
    }

    size_t write(const uint8_t *prefix_buffer, size_t prefix_len)
    {
        size_t retVal = 0;

        return retVal;
    }

    size_t requestFrom(uint8_t addr, uint8_t len, uint8_t stop)
    {
        size_t retVal = 0;

        return retVal;
    }

    void setClock(uint32_t desiredclk)
    {

    }

};

extern TwoWire Wire;