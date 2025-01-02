#pragma once

#include <stdint.h>

#include <vector>


class I2C
{
public:
    static bool CheckAddr(uint8_t addr);
    static std::vector<uint8_t> Scan();
    static uint8_t ReadReg8(uint8_t addr, uint8_t reg);
    static uint16_t ReadReg16(uint8_t addr, uint8_t reg);
    static void WriteReg8(uint8_t addr, uint8_t reg, uint8_t val);
    static void WriteReg16(uint8_t addr, uint8_t reg, uint16_t val);
    static void WriteDirect(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t bufSize);

    static void Init();
    static void SetupShell();

private:
};





