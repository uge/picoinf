#include "I2C.h"
#include "Log.h"
#include "Timeline.h"
#include "Utl.h"

#include "hardware/i2c.h"

#include "StrictMode.h"


// link about I2C
// https://developerhelp.microchip.com/xwiki/bin/view/applications/i2c/


// Useful list of known I2C addresses
// https://learn.adafruit.com/i2c-addresses?view=all
// https://github.com/adafruit/I2C_Addresses


// Repeated Start
// - not something I support across multiple Read/Write operations, but could be added if necessary
// - I think basically it would be
//   - add StartTransaction()/EndTranscation()
//   - StartTransaction() would set a member variable which would be checked in the Write/Read functions
//     to determine if the bus should be held
//   - EndTransaction() would clear that member variable and also issue a stop somehow
//     - possibly do a read with a stop just to end the transaction without complicating the read/write functions
// 
// link to some info on repeated start
// https://learn.adafruit.com/working-with-i2c-devices/repeated-start



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


I2C::I2C(uint8_t addr, Instance instance)
: addr_(addr)
, i2c_(instance == Instance::I2C0 ? i2c0 : i2c1)
{
    // nothing to do
}

bool I2C::IsAlive()
{
    uint8_t byte;
    int ret = i2c_read_blocking_until(i2c_, addr_, &byte, 1, false, make_timeout_time_ms(TIMEOUT_MS));

    AnalyzeRetVal(ret);

    return ret >= 0;
}

uint8_t I2C::ReadReg8(uint8_t reg)
{
    uint8_t retVal;
    
    AnalyzeRetVal(i2c_write_blocking_until(i2c_, addr_, &reg, 1, true, make_timeout_time_ms(TIMEOUT_MS)));
    AnalyzeRetVal(i2c_read_blocking_until(i2c_, addr_, &retVal, 1, false, make_timeout_time_ms(TIMEOUT_MS)));

    return retVal;
}

uint16_t I2C::ReadReg16(uint8_t reg)
{
    uint16_t retVal;

    // I2C is MSB, so create our own MSB buffer
    uint16_t buf = 0;

    AnalyzeRetVal(i2c_write_blocking_until(i2c_, addr_, &reg, 1, true, make_timeout_time_ms(TIMEOUT_MS)));
    AnalyzeRetVal(i2c_read_blocking_until(i2c_, addr_, (uint8_t *)&buf, 2, false, make_timeout_time_ms(TIMEOUT_MS)));

    // put in host order
    retVal = ntohs(buf);

    return retVal;
}

void I2C::WriteReg8(uint8_t reg, uint8_t val)
{
    uint8_t buf[] = {
        reg,
        val,
    };

    AnalyzeRetVal(i2c_write_blocking_until(i2c_, addr_, (uint8_t *)&buf, sizeof(buf), false, make_timeout_time_ms(TIMEOUT_MS)));
}

void I2C::WriteReg16(uint8_t reg, uint16_t val)
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

    AnalyzeRetVal(i2c_write_blocking_until(i2c_, addr_, (uint8_t *)&buf, sizeof(buf), false, make_timeout_time_ms(TIMEOUT_MS)));
}

// No endianness conversions done here, caller has to swap around bytes
// of registers if they need to be converted.  Data sent as-is.
void I2C::WriteDirect(uint8_t reg, uint8_t *buf, uint8_t bufSize)
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

    AnalyzeRetVal(i2c_write_blocking_until(i2c_, addr_, (uint8_t *)&bufLocal, sizeof(bufLocal), false, make_timeout_time_ms(TIMEOUT_MS)));
}

void I2C::AnalyzeRetVal(int retVal)
{
    Stats &stats = stats_[i2c_ == i2c0 ? 0 : 1];

    if (retVal == PICO_ERROR_GENERIC)
    {
        ++stats.PICO_ERROR_GENERIC;
    }
    else if (retVal == PICO_ERROR_TIMEOUT)
    {
        ++stats.PICO_ERROR_TIMEOUT;
    }
    else if (retVal < 0)
    {
        ++stats.PICO_ERROR_OTHER;
    }
    else if (retVal >= 0)
    {
        ++stats.PICO_OK;
    }
}



////////////////////////////////////////////////////////////////////////////////
// Statics
////////////////////////////////////////////////////////////////////////////////

bool I2C::IsAlive(uint8_t addr, Instance instance)
{
    I2C i2c(addr, instance);

    return i2c.IsAlive();
}

vector<uint8_t> I2C::Scan(Instance instance)
{
    vector<uint8_t> retVal;

    for (uint8_t i = 0; i < 0b01111111; ++i)
    {
        if (IsAlive(i, instance))
        {
            retVal.push_back(i);
        }
    }

    return retVal;
}


////////////////////////////////////////////////////////////////////////////////
// Initialization
//
// Bus Speeds:
// Standard Mode  :  100 Kbit
// Fast Mode      :  400 Kbit
// Fast Mode Plus : 1000 Kbit (1.0 Mbit)
// High Speed Mode: 3400 Kbit (3.4 Mbit)
//
// Pico can do 1Mbit, but we choose to do 400 Kbit for stability after seeing
// some farther-away sensors fail at 1Mbit. Works fine at this speed.
//
////////////////////////////////////////////////////////////////////////////////

void I2C::Init0()
{
    Timeline::Global().Event("I2C::Init0");

    i2c_init(i2c0, 400 * 1000);
    gpio_set_function(4, GPIO_FUNC_I2C);
    gpio_set_function(5, GPIO_FUNC_I2C);
    gpio_pull_up(4);
    gpio_pull_up(5);
}

void I2C::Init1()
{
    Timeline::Global().Event("I2C::Init1");

    i2c_init(i2c1, 400 * 1000);
    gpio_set_function(14, GPIO_FUNC_I2C);
    gpio_set_function(15, GPIO_FUNC_I2C);
    gpio_pull_up(14);
    gpio_pull_up(15);
}

static void ScanPretty(I2C::Instance instance)
{
    // show column headers
    Log("   |  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
    Log("---|------------------------------------------------");

    // Generate
    for (uint8_t row = 0x00; row <= 0x70; row += 0x10)
    {
        LogNNL(Format::ToHex(row, false), " |");

        for (uint8_t col = 0x0; col <= 0xF; ++col)
        {
            uint8_t addr = row + col;

            I2C i2c(addr, instance);

            LogNNL(" ");
            if (i2c.IsAlive())
            {
                LogNNL(Format::ToHex(addr, false));
            }
            else
            {
                LogNNL("--");
            }
        }

        LogNL();
    }
}

void I2C::SetupShell0()
{
    Timeline::Global().Event("I2C::SetupShell0");

    Shell::AddCommand("i2c0.scan", [&](vector<string> argList){
        LogNL();
        ScanPretty(Instance::I2C0);
        vector<uint8_t> addrList = I2C::Scan(Instance::I2C0);
        LogNL();
        Log("Found: ", addrList.size(), ": ", ContainerToString(addrList));
    }, { .argCount = 0, .help = "I2C scan all addresses"});

    Shell::AddCommand("i2c0.addr", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)Format::FromHex(hexAddr);

        Log("Addr ", hexAddr, "(", addr, "): ", I2C::IsAlive(addr, Instance::I2C0) ? "alive" : "not alive");
    }, { .argCount = 1, .help = "I2C check hexAddr alive"});

    Shell::AddCommand("i2c0.read8", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)Format::FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)Format::FromHex(hexReg);

        I2C i2c(addr, Instance::I2C0);
        uint8_t val = i2c.ReadReg8(reg);

        Log("Reading ", hexAddr, "(", addr, ") ", hexReg, "(", reg, ")");
        Log(Format::ToHex(val), "(", val, ")(", Format::ToBin(val), ")");
    }, { .argCount = 2, .help = "I2C read <hexAddr> <hexReg>"});

    Shell::AddCommand("i2c0.read16", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)Format::FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)Format::FromHex(hexReg);

        I2C i2c(addr, Instance::I2C0);
        uint16_t val = i2c.ReadReg16(reg);

        Log("Reading ", hexAddr, "(", addr, ") ", hexReg, "(", reg, ")");
        Log(Format::ToHex(val), "(", val, ")(", Format::ToBin(val), ")");
    }, { .argCount = 2, .help = "I2C read <hexAddr> <hexReg>"});

    Shell::AddCommand("i2c0.write8", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)Format::FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)Format::FromHex(hexReg);
        string hexVal = argList[2];
        uint8_t val = (uint8_t)Format::FromHex(hexVal);

        I2C i2c(addr, Instance::I2C0);
        i2c.WriteReg8(reg, val);

        Log(Format::ToHex(val), "(", val, ")(", Format::ToBin(val), ")");
    }, { .argCount = 3, .help = "I2C write <hexAddr> <hexReg> <hexVal>"});

    Shell::AddCommand("i2c0.write16", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)Format::FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)Format::FromHex(hexReg);
        string hexVal = argList[2];
        uint16_t val = (uint8_t)Format::FromHex(hexVal);

        I2C i2c(addr, Instance::I2C0);
        i2c.WriteReg16(reg, val);

        Log(Format::ToHex(val), "(", val, ")(", Format::ToBin(val), ")");
    }, { .argCount = 3, .help = "I2C write <hexAddr> <hexReg> <hexVal>"});

    Shell::AddCommand("i2c0.stats", [&](vector<string> argList){
        stats_[0].Print();
    }, { .argCount = 0, .help = ""});
}

void I2C::SetupShell1()
{
    Timeline::Global().Event("I2C::SetupShell1");

    Shell::AddCommand("i2c1.scan", [&](vector<string> argList){
        LogNL();
        ScanPretty(Instance::I2C1);
        vector<uint8_t> addrList = I2C::Scan(Instance::I2C1);
        LogNL();
        Log("Found: ", addrList.size(), ": ", ContainerToString(addrList));
    }, { .argCount = 0, .help = "I2C scan all addresses"});

    Shell::AddCommand("i2c1.addr", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)Format::FromHex(hexAddr);

        Log("Addr ", hexAddr, "(", addr, "): ", I2C::IsAlive(addr, Instance::I2C1) ? "alive" : "not alive");
    }, { .argCount = 1, .help = "I2C check hexAddr alive"});

    Shell::AddCommand("i2c1.read8", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)Format::FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)Format::FromHex(hexReg);

        I2C i2c(addr, Instance::I2C1);
        uint8_t val = i2c.ReadReg8(reg);

        Log("Reading ", hexAddr, "(", addr, ") ", hexReg, "(", reg, ")");
        Log(Format::ToHex(val), "(", val, ")(", Format::ToBin(val), ")");
    }, { .argCount = 2, .help = "I2C read <hexAddr> <hexReg>"});

    Shell::AddCommand("i2c1.read16", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)Format::FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)Format::FromHex(hexReg);

        I2C i2c(addr, Instance::I2C1);
        uint16_t val = i2c.ReadReg16(reg);

        Log("Reading ", hexAddr, "(", addr, ") ", hexReg, "(", reg, ")");
        Log(Format::ToHex(val), "(", val, ")(", Format::ToBin(val), ")");
    }, { .argCount = 2, .help = "I2C read <hexAddr> <hexReg>"});

    Shell::AddCommand("i2c1.write8", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)Format::FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)Format::FromHex(hexReg);
        string hexVal = argList[2];
        uint8_t val = (uint8_t)Format::FromHex(hexVal);

        I2C i2c(addr, Instance::I2C1);
        i2c.WriteReg8(reg, val);

        Log(Format::ToHex(val), "(", val, ")(", Format::ToBin(val), ")");
    }, { .argCount = 3, .help = "I2C write <hexAddr> <hexReg> <hexVal>"});

    Shell::AddCommand("i2c1.write16", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)Format::FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)Format::FromHex(hexReg);
        string hexVal = argList[2];
        uint16_t val = (uint8_t)Format::FromHex(hexVal);

        I2C i2c(addr, Instance::I2C1);
        i2c.WriteReg16(reg, val);

        Log(Format::ToHex(val), "(", val, ")(", Format::ToBin(val), ")");
    }, { .argCount = 3, .help = "I2C write <hexAddr> <hexReg> <hexVal>"});

    Shell::AddCommand("i2c1.stats", [&](vector<string> argList){
        stats_[1].Print();
    }, { .argCount = 0, .help = ""});
}


void I2C::Stats::Print()
{
    Log("PICO_OK           : ", Commas(PICO_OK));
    Log("PICO_ERROR_GENERIC: ", Commas(PICO_ERROR_GENERIC));
    Log("PICO_ERROR_TIMEOUT: ", Commas(PICO_ERROR_TIMEOUT));
    Log("PICO_ERROR_OTHER  : ", Commas(PICO_ERROR_OTHER));
}

