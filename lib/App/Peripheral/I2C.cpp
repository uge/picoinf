#include "I2C.h"

#include <zephyr/net/net_ip.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

#include "Log.h"
#include "Utl.h"


// https://docs.zephyrproject.org/latest/build/dts/api/bindings/i2c/nordic,nrf-twi.html
// https://docs.zephyrproject.org/latest/hardware/peripherals/i2c.html
// https://docs.zephyrproject.org/2.7.0/reference/devicetree/bindings/i2c/nordic%2Cnrf-twi.html
// https://www.beyondlogic.org/devicetree-overlays-on-zephyr-rtos-adding-i2c-or-spi/

// https://infocenter.nordicsemi.com/index.jsp?topic=%2Fps_nrf52840%2Ftwi.html
// https://infocenter.nordicsemi.com/index.jsp?topic=%2Fps_nrf52840%2Ftwi.html&cp=5_0_0_5_28_7_0&anchor=register.TASKS_STARTRX



// the first param of each is the dev ptr.
// all are synchronous.
//
// int i2c_write(const uint8_t *buf, uint32_t num_bytes, uint16_t addr)
// - Write a set amount of data to an I2C device
//
// int i2c_read(uint8_t *buf, uint32_t num_bytes, uint16_t addr)
// - Read a set amount of data from an I2C device.
//
// int i2c_write_read(uint16_t addr, const void *write_buf, size_t num_write, void *read_buf, size_t num_read)
// - Write then read data from an I2C device.
// - This supports the common operation “this is what I want”, “now give it to me”
// - transaction pair through a combined write-then-read bus transaction
//
// int i2c_reg_read_byte(uint16_t dev_addr, uint8_t reg_addr, uint8_t *value)
// - Read internal register of an I2C device.
//
// int i2c_reg_write_byte(uint16_t dev_addr, uint8_t reg_addr, uint8_t value)
// - Write internal register of an I2C device.
// - NOTE: This function internally combines the register and value into a single bus transaction.
//
// int i2c_reg_update_byte(uint8_t dev_addr, uint8_t reg_addr, uint8_t mask, uint8_t value)¶
// - Update internal register of an I2C device.
// - NOTE: If the calculated new register value matches the value that was read this function will not generate a write operation
//



I2C::I2C()
{
    dev_ = DEVICE_DT_GET(DT_NODELABEL(i2c0));
}

void I2C::RecoverBus()
{
    i2c_recover_bus(dev_);
}

bool I2C::CheckAddr(uint8_t addr)
{
    uint8_t byte;

    RecoverBus();
    int ret = i2c_read(dev_, &byte, 1, addr);
    SetOk(ret);

    return !ret;
}

vector<uint8_t> I2C::Scan()
{
    vector<uint8_t> retVal;

    for (uint8_t i = 0; i < 0b01111111; ++i)
    {
        if (CheckAddr(i))
        {
            retVal.push_back(i);
        }
    }

    return retVal;
}

uint8_t I2C::ReadReg8(uint8_t addr, uint8_t reg)
{
    uint8_t retVal;
    
    RecoverBus();
    int ret = i2c_write_read(dev_, addr, &reg, 1, &retVal, 1);
    SetOk(ret);

    return retVal;
}

uint16_t I2C::ReadReg16(uint8_t addr, uint8_t reg)
{
    uint16_t retVal;

    // I2C is MSB, so create our own MSB buffer
    uint16_t buf;

    RecoverBus();
    int ret = i2c_write_read(dev_, addr, &reg, 1, (uint8_t *)&buf, sizeof(buf));
    SetOk(ret);

    // put in host order
    retVal = ntohs(buf);

    return retVal;
}

void I2C::WriteReg8(uint8_t addr, uint8_t reg, uint8_t val)
{
    RecoverBus();
    int ret = i2c_reg_write_byte(dev_, addr, reg, val);
    SetOk(ret);
}

void I2C::WriteReg16(uint8_t addr, uint8_t reg, uint16_t val)
{
    // We have 2 bytes to send, and we want to target register reg
    //
    // The first byte sent (after the addr) is the register to target,
    // then the actual bytes for that register.
    //
    // We will pack our own 3-byte buffer.
    // I2C is MSB.

    uint8_t buf[3] = {
        reg,
        (uint8_t)((val >> 8) & 0xFF),
        (uint8_t)(val & 0xFF),
    };

    RecoverBus();
    int ret = i2c_write(dev_, buf, 3, addr);
    SetOk(ret);
}

// No endianness conversions done here, caller has to swap around bytes
// of registers if they need to be converted.  Data sent as-is.
void I2C::WriteDirect(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t bufSize)
{
    // We have bufSize bytes to send, and we want to target register reg
    //
    // The first byte sent (after the addr) is the register to target,
    // then the actual bytes for that register and beyond.
    //
    // We will pack our own bufSize+1-byte buffer.
    // I2C is MSB, but we don't care, transfer as-is.
    uint8_t localBuf[bufSize + 1];

    // drop in the target register
    localBuf[0] = reg;

    // copy in all the data that was given
    memcpy(&localBuf[1], buf, bufSize);

    RecoverBus();
    int ret = i2c_write(dev_, localBuf, bufSize + 1, addr);
    SetOk(ret);
}

bool I2C::Ok()
{
    return ok_;
}

void I2C::ResetOk()
{
    ok_ = true;
}

void I2C::SetOk(int ret)
{
    if (ret < 0)
    {
        ok_ = ret && ok_;
    }
}


////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////

int I2CSetupShell()
{
    Timeline::Global().Event("I2CSetupShell");

    static I2C i2c;

    Shell::AddCommand("i2c.scan", [&](vector<string> argList){
        vector<uint8_t> addrList = i2c.Scan();
        Log("Count found: ", addrList.size());
        LogNNL("List:");
        for (auto addr : addrList)
        {
            LogNNL(" ", Format::ToHex(addr), "(", addr, ")");
        }
        LogNL();

        i2c.ResetOk();
    }, { .argCount = 0, .help = "I2C scan all addresses"});

    Shell::AddCommand("i2c.addr", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = Format::FromHex(hexAddr);

        Log("Addr ", hexAddr, "(", addr, "): ", i2c.CheckAddr(addr) ? "alive" : "not alive");

        i2c.ResetOk();
    }, { .argCount = 1, .help = "I2C check hexAddr alive"});

    Shell::AddCommand("i2c.read8", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = Format::FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = Format::FromHex(hexReg);

        uint8_t val = i2c.ReadReg8(addr, reg);

        Log("Reading ", hexAddr, "(", addr, ") ", hexReg, "(", reg, ")");
        Log("Val (", i2c.Ok() ? "ok" : "bad", "): ", Format::ToHex(val), "(", val, ")(", Format::ToBin(val), ")");

        i2c.ResetOk();
    }, { .argCount = 2, .help = "I2C read <hexAddr> <hexReg>"});

    Shell::AddCommand("i2c.read16", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = Format::FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = Format::FromHex(hexReg);

        uint16_t val = i2c.ReadReg16(addr, reg);

        Log("Reading ", hexAddr, "(", addr, ") ", hexReg, "(", reg, ")");
        Log("Val (", i2c.Ok() ? "ok" : "bad", "): ", Format::ToHex(val), "(", val, ")(", Format::ToBin(val), ")");

        i2c.ResetOk();
    }, { .argCount = 2, .help = "I2C read <hexAddr> <hexReg>"});

    Shell::AddCommand("i2c.write8", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = Format::FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = Format::FromHex(hexReg);
        string hexVal = argList[2];
        uint8_t val = Format::FromHex(hexVal);

        i2c.WriteReg8(addr, reg, val);

        Log("Val (", i2c.Ok() ? "ok" : "bad", "): ", Format::ToHex(val), "(", val, ")(", Format::ToBin(val), ")");

        i2c.ResetOk();
    }, { .argCount = 3, .help = "I2C write <hexAddr> <hexReg> <hexVal>"});

    Shell::AddCommand("i2c.write16", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = Format::FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = Format::FromHex(hexReg);
        string hexVal = argList[2];
        uint16_t val = Format::FromHex(hexVal);

        i2c.WriteReg16(addr, reg, val);

        Log("Val (", i2c.Ok() ? "ok" : "bad", "): ", Format::ToHex(val), "(", val, ")(", Format::ToBin(val), ")");

        i2c.ResetOk();
    }, { .argCount = 3, .help = "I2C write <hexAddr> <hexReg> <hexVal>"});

    return 1;
}

#include <zephyr/init.h>
SYS_INIT(I2CSetupShell, APPLICATION, 80);



