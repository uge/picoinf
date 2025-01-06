#pragma once

#include "hardware/i2c.h"

#include <cstdint>
#include <vector>


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
    uint8_t  ReadReg8(uint8_t reg, bool stop = true);
    uint16_t ReadReg16(uint8_t reg, bool stop = true);
    uint32_t ReadReg(uint8_t reg, uint8_t *buf, uint32_t bufSize, bool stop = true);
    uint32_t ReadRaw(uint8_t *buf, uint32_t bufSize, bool stop = true);
    uint32_t WriteReg8(uint8_t reg, uint8_t val, bool stop = true);
    uint32_t WriteReg16(uint8_t reg, uint16_t val, bool stop = true);
    uint32_t WriteReg(uint8_t reg, uint8_t *buf, uint32_t bufSize, bool stop = true);
    uint32_t WriteRaw(uint8_t *buf, uint32_t bufSize, bool stop = true);

private:

    void AnalyzeRetVal(int retVal);
    int  GetRetValLast();


private:

    uint8_t     addr_       = 0;
    i2c_inst_t *i2c_        = nullptr;
    int         retValLast_ = 0;


public:

    static bool IsAlive(uint8_t addr, Instance instance = Instance::I2C0);
    static std::vector<uint8_t> Scan(Instance instance = Instance::I2C0);

    static void Init0();
    static void Init1();
    static void SetupShell0();
    static void SetupShell1();

private:

    static const uint32_t TIMEOUT_MS = 1;

    struct Stats
    {
        uint64_t PICO_OK;
        uint64_t PICO_ERROR_GENERIC;
        uint64_t PICO_ERROR_TIMEOUT;
        uint64_t PICO_ERROR_OTHER;

        void Print();
    };

    inline static Stats stats_[2];
};





