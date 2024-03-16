#include "I2C.h"
#include "Log.h"
#include "Timeline.h"
#include "Utl.h"

#include "hardware/i2c.h"

#include "StrictMode.h"


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


bool I2C::CheckAddr(uint8_t addr)
{
    uint8_t byte;

    int ret = i2c_read_blocking(i2c0, addr, &byte, 1, false);

    return ret >= 0;
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
    
    i2c_write_blocking(i2c0, addr, &reg, 1, true);
    i2c_read_blocking(i2c0, addr, &retVal, 1, false);

    return retVal;
}

uint16_t I2C::ReadReg16(uint8_t addr, uint8_t reg)
{
    uint16_t retVal;

    // I2C is MSB, so create our own MSB buffer
    uint16_t buf = 0;

    i2c_write_blocking(i2c0, addr, &reg, 1, true);
    i2c_read_blocking(i2c0, addr, (uint8_t *)&buf, 2, false);

    // put in host order
    retVal = ntohs(buf);

    return retVal;
}

void I2C::WriteReg8(uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t buf[] = {
        reg,
        val,
    };

    i2c_write_blocking(i2c0, addr, (uint8_t *)&buf, sizeof(buf), false);
}

void I2C::WriteReg16(uint8_t addr, uint8_t reg, uint16_t val)
{
    // We have 2 bytes to send, and we want to target register reg
    //
    // The first byte sent (after the addr) is the register to target,
    // then the actual bytes for that register.
    //
    // We will pack our own buffer.
    // I2C is MSB.

    uint8_t buf[] = {
        reg,
        (uint8_t)((val >> 8) & 0xFF),
        (uint8_t)(val & 0xFF),
    };

    i2c_write_blocking(i2c0, addr, (uint8_t *)&buf, sizeof(buf), false);
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
    uint8_t bufLocal[bufSize + 1];

    // drop in the target register
    bufLocal[0] = reg;

    // copy in all the data that was given
    memcpy(&bufLocal[1], buf, bufSize);

    i2c_write_blocking(i2c0, addr, (uint8_t *)&bufLocal, sizeof(bufLocal), false);
}


////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////

void I2C::Init()
{
    Timeline::Global().Event("I2C::Init");

    i2c_init(i2c0, 1000 * 1000);
    gpio_set_function(4, GPIO_FUNC_I2C);
    gpio_set_function(5, GPIO_FUNC_I2C);
    gpio_pull_up(4);
    gpio_pull_up(5);
}

void I2C::SetupShell()
{
    Timeline::Global().Event("I2C::SetupShell");

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
    }, { .argCount = 0, .help = "I2C scan all addresses"});

    Shell::AddCommand("i2c.addr", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)Format::FromHex(hexAddr);

        Log("Addr ", hexAddr, "(", addr, "): ", i2c.CheckAddr(addr) ? "alive" : "not alive");
    }, { .argCount = 1, .help = "I2C check hexAddr alive"});

    Shell::AddCommand("i2c.read8", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)Format::FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)Format::FromHex(hexReg);

        uint8_t val = i2c.ReadReg8(addr, reg);

        Log("Reading ", hexAddr, "(", addr, ") ", hexReg, "(", reg, ")");
        Log(Format::ToHex(val), "(", val, ")(", Format::ToBin(val), ")");
    }, { .argCount = 2, .help = "I2C read <hexAddr> <hexReg>"});

    Shell::AddCommand("i2c.read16", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)Format::FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)Format::FromHex(hexReg);

        uint16_t val = i2c.ReadReg16(addr, reg);

        Log("Reading ", hexAddr, "(", addr, ") ", hexReg, "(", reg, ")");
        Log(Format::ToHex(val), "(", val, ")(", Format::ToBin(val), ")");
    }, { .argCount = 2, .help = "I2C read <hexAddr> <hexReg>"});

    Shell::AddCommand("i2c.write8", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)Format::FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)Format::FromHex(hexReg);
        string hexVal = argList[2];
        uint8_t val = (uint8_t)Format::FromHex(hexVal);

        i2c.WriteReg8(addr, reg, val);

        Log(Format::ToHex(val), "(", val, ")(", Format::ToBin(val), ")");
    }, { .argCount = 3, .help = "I2C write <hexAddr> <hexReg> <hexVal>"});

    Shell::AddCommand("i2c.write16", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)Format::FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)Format::FromHex(hexReg);
        string hexVal = argList[2];
        uint16_t val = (uint8_t)Format::FromHex(hexVal);

        i2c.WriteReg16(addr, reg, val);

        Log(Format::ToHex(val), "(", val, ")(", Format::ToBin(val), ")");
    }, { .argCount = 3, .help = "I2C write <hexAddr> <hexReg> <hexVal>"});
}




