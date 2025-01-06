#pragma once

#include "Arduino.h"

// https://github.com/arduino/ArduinoCore-avr/blob/master/libraries/SPI/src/SPI.h
// https://github.com/arduino/ArduinoCore-avr/blob/master/libraries/SPI/src/SPI.cpp


#define SPI_MODE0 0x00
#define SPI_MODE1 0x04
#define SPI_MODE2 0x08
#define SPI_MODE3 0x0C


class SPISettings
{
public:
    SPISettings()
    {
    }

    SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode)
    {
    }

};

class SPIClass
{
public:
    static void begin() {}
    static void transfer(void *buf, size_t count) {}
    static void beginTransaction(SPISettings) {}
    static void endTransaction() {}
};

extern SPIClass SPI;