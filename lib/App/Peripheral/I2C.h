#pragma once

#include <stdint.h>

#include <vector>

#include <zephyr/device.h>


class I2C
{
public:
    I2C();
    void RecoverBus();
    bool CheckAddr(uint8_t addr);
    std::vector<uint8_t> Scan();
    uint8_t ReadReg8(uint8_t addr, uint8_t reg);
    uint16_t ReadReg16(uint8_t addr, uint8_t reg);
    void WriteReg8(uint8_t addr, uint8_t reg, uint8_t val);
    void WriteReg16(uint8_t addr, uint8_t reg, uint16_t val);
    void WriteDirect(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t bufSize);
    bool Ok();
    void ResetOk();

private:

    void SetOk(int ret);

    const device *dev_ = nullptr;
    bool ok_ = true;
};





