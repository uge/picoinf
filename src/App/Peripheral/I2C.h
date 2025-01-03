#pragma once

#include <cstdint>
#include <vector>

#include "hardware/i2c.h"


class I2C
{
public:

    enum class Instance : uint8_t
    {
        I2C0 = 0,
        I2C1 = 1,
    };

    I2C(uint8_t addr, Instance instance = Instance::I2C0);
    bool     IsAlive();
    uint8_t  ReadReg8(uint8_t reg);
    uint16_t ReadReg16(uint8_t reg);
    void     WriteReg8(uint8_t reg, uint8_t val);
    void     WriteReg16(uint8_t reg, uint16_t val);
    void     WriteDirect(uint8_t reg, uint8_t *buf, uint8_t bufSize);

    static bool IsAlive(uint8_t addr, Instance instance = Instance::I2C0);
    static std::vector<uint8_t> Scan(Instance instance = Instance::I2C0);

    static void Init0();
    static void Init1();
    static void SetupShell0();
    static void SetupShell1();

private:

    uint8_t     addr_ = 0;
    i2c_inst_t *i2c_  = nullptr;
};





