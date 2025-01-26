#pragma once

#include "I2C.h"

#include <cstdint>
#include <vector>


class TwoWire
{
public:
    TwoWire(uint8_t addr, I2C::Instance instance = I2C::Instance::I2C0);

    void begin();
    void end();

    void    beginTransmission(uint8_t addr);
    size_t  write(uint8_t data);
    size_t  write(const uint8_t *buf, size_t bufSize);
    uint8_t endTransmission();
    uint8_t endTransmission(uint8_t stop);

    size_t requestFrom(uint8_t addr, uint8_t len);
    size_t requestFrom(uint8_t addr, uint8_t len, uint8_t stop);
    int    available();
    int    read();

    void setClock(uint32_t);

    // convenience for my own interfacing
    I2C &GetI2C();


private:

    I2C i2c_;

    bool inTransmission_ = false;
    uint8_t addrTransmission_ = 0x00;
    std::vector<uint8_t> queueWrite_;

    std::vector<uint8_t> queueRead_;
};

extern TwoWire Wire;