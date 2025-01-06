#include "I2C.h"
#include "Log.h"
#include "Shell.h"
#include "Timeline.h"
#include "Utl.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"

#include <cstring>
#include <string>
using namespace std;

#include "StrictMode.h"


// I2C comms 
// https://www.ti.com/lit/an/slva704/slva704.pdf?ts=1736148035401
// https://developerhelp.microchip.com/xwiki/bin/view/applications/i2c/
// https://learn.sparkfun.com/tutorials/i2c/all
// https://www.circuitbasics.com/basics-of-the-i2c-communication-protocol/
// 
// Notes:
// - It's not that hard
// - I'm describing convention below, maybe some devices do goofy stuff
// - Writing data (in pure I2C terms)
//   - The first byte written to the I2C bus is the address of the device you're referring to
//   - Along with that byte (a single bit) indicates whether the operation you're about to
//     perform is a Read or Write operation
//   - For a Write, the master then clocks out bits for as many bytes as desired
//     - slave can NAK to stop the transfer early
// - Reading data (in pure I2C terms)
//   - Begins (as above) with writing to the I2C bus the address of the slave device and indicating a
//     Read operation.
//   - Master then clocks in bits
//   - Master will ACK each byte except the final byte which it will NAK
//
// - Writing (in practice)
//   - You write the device address and "write" bit
//   - You send the register address you intend to start writing at
//   - You clock out bytes, and the receiver auto-increments the destination it applies them to
//     offset from the original register
// - Reading (in practice)
//   - You write the device address and "write" bit
//   - You send the register you intend to start reading from
//   - (optionally you do not indicate a stop here, to hold the device in control)
//   - You do a (re)start, and address the same device, this time with a "read" bit
//   - You clock in bytes, and the slave sends you bytes starting at the register you indicated
//     and auto-increments the origin offset from that register
//
// Note, when the master addresses the slave, the slave must ACK/NAK, this is how the master knows it has
// a valid endpoint it is communicating with.
//
// The start of a new sequence, Read or Write, is explained more in the Start / Repeated start notes.
//
// Library will do the part about indicating whether the first written byte (address) carries
// the read or write bit.
//



// Library design notes
// 
// The lowest-level of each Read and Write function below is "raw" in the sense that the caller must have set up
// the conditions where this operation makes sense.
//
// This library is known to be used by the Arduino-shim Wire interface as well.
// That library is used by pure chaos which basically hand-crafts the bytes to the device.
// The raw nature of the interface here is required to support that, and so is used by the internal
// functions as well to keep it all consistent and tested.







// Useful list of known I2C addresses
// https://learn.adafruit.com/i2c-addresses?view=all
// https://github.com/adafruit/I2C_Addresses


// Repeated Start info
// https://learn.adafruit.com/working-with-i2c-devices/repeated-start
//
// Basically:
// - Some devices might not like it if you "give up" the bus between read/write transactions
// - So you can hold it between by not issuing a "stop" (like, stop transaction)
// - Instead, you just don't, and issue a new "start" (a repeated start) to go on with the next
// - Make sure you do actually "stop" on the last


// Notes about Arduino's beginTransmission concept
// - it's not related to repeated starts
// - it means "queue up outbound data until I say go"
// - it solves the issue of needing to write "all at once" on the I2C bus
//   - more like, APIs commonly want you to send data like this:
//     - specify target register
//     - specify a series of bytes to send (the rcv side increments the target destination for each)
//     - api issues a start/repeatedStart
//     - api fires all the bytes down the hole
//     - api may or may not (you decide) issue a "stop"
//     - BUT, how do you "write more" now? your next call to write will issue a start/repeatedStart
//       - could you target the next register in the series to continue? maybe (never tried, probably)
//   - arduino doesn't try to get fancy, it just queues the data and sends when gather complete
//   - looks like in newer versions of pico-sdk you can "burst" write to accomplish avoiding
//     needing to queue (which makes sense, the lower-level I2C HW probably doesn't care what
//     you do, plus it wouldn't expect you have all the data at once anyway)
// - looking at the Adafruit I2CDevice, it seems to do begin/end for every write
//   - like, why?
//   - it also uses back-to-back start/endTransmission to check health of the endpoint
//     - looks like this will ultimately
//       - set up a write to the endpoint (which will ack or nack)
//       - then send no bytes
//       - initial error value is "no error," then no bytes sent, so no error returned when
//         ACK on write target from dst.
//     - so, really it's just sensing the endpoint is there
//     - this is different than what I did, which is read from 0x00, which I wonder if that
//       can fail in some scenarios? Can change if needed later.
// https://github.com/arduino/ArduinoCore-avr/blob/master/libraries/Wire/src/Wire.cpp#L217
// - the endTransmission appears to
//   - send (blocking) all the queued data
//   - it does send a stop




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
    // ideally we would simply address the endpoint and look for ack/nak.
    // this doesn't work in practice with this hw because the underlying hw
    // (according to rpi-pico code) will not allow a zero-byte transfer.
    // instead we read register 0 and upon success call the endpoint alive.

    uint8_t byte;
    int ret = i2c_read_blocking_until(i2c_, addr_, &byte, 1, false, make_timeout_time_ms(TIMEOUT_MS));
    AnalyzeRetVal(ret);

    return ret >= 0;
}

uint8_t I2C::ReadReg8(uint8_t reg, bool stop)
{
    uint8_t retVal = 0;
    
    ReadReg(reg, &retVal, 1, stop);

    return retVal;
}

uint16_t I2C::ReadReg16(uint8_t reg, bool stop)
{
    uint16_t retVal = 0;

    // I2C is MSB, so create our own MSB buffer
    uint16_t buf = 0;
    ReadReg(reg, (uint8_t *)&buf, 2, stop);

    // put in host order
    retVal = ntohs(buf);

    return retVal;
}

// No endianness conversions done here, caller has to swap around bytes
// of registers if they need to be converted.  Data sent as-is.
uint32_t I2C::ReadReg(uint8_t reg, uint8_t *buf, uint32_t bufSize, bool stop)
{
    uint32_t retVal = 0;

    if (buf && bufSize)
    {
        int retWrite = i2c_write_blocking_until(i2c_, addr_, &reg, 1, true, make_timeout_time_ms(TIMEOUT_MS));
        AnalyzeRetVal(retWrite);

        if (retWrite >= 0)
        {
            retVal = ReadRaw(buf, bufSize, stop);
        }
    }

    return retVal;
}

uint32_t I2C::ReadRaw(uint8_t *buf, uint32_t bufSize, bool stop)
{
    uint32_t retVal = 0;

    if (buf && bufSize)
    {
        int retRead = i2c_read_blocking_until(i2c_, addr_, buf, (size_t)bufSize, !stop, make_timeout_time_ms(TIMEOUT_MS));
        AnalyzeRetVal(retRead);

        if (retRead >= 0)
        {
            retVal = (uint32_t)retRead;
        }
    }

    return retVal;
}

uint32_t I2C::WriteReg8(uint8_t reg, uint8_t val, bool stop)
{
    return WriteReg(reg, &val, 1, stop);
}

uint32_t I2C::WriteReg16(uint8_t reg, uint16_t val, bool stop)
{
    // I2C is MSB.
    uint16_t buf = htons(val);

    return WriteReg(reg, (uint8_t *)&buf, 2, stop);
}

// No endianness conversions done here, caller has to swap around bytes
// of registers if they need to be converted.  Data sent as-is.
uint32_t I2C::WriteReg(uint8_t reg, uint8_t *buf, uint32_t bufSize, bool stop)
{
    // We have bufSize bytes to send, and we want to target register reg
    //
    // The first byte sent (after the addr) is the register to target,
    // then the actual bytes for that register and beyond.
    //
    // We will pack our own bufSize+1-byte buffer.
    // I2C is MSB, but we don't care, transfer as-is.
    uint8_t bufLocal[bufSize + 2];

    // drop in the device address
    bufLocal[0] = addr_;
    // drop in the target register
    bufLocal[1] = reg;

    // copy in all the data that was given
    memcpy(&bufLocal[2], buf, bufSize);

    return WriteRaw(bufLocal, sizeof(bufLocal), stop);
}

uint32_t I2C::WriteRaw(uint8_t *buf, uint32_t bufSize, bool stop)
{
    uint32_t retVal = 0;

    if (buf && bufSize >= 1)
    {
        int ret = i2c_write_blocking_until(i2c_, buf[0], (uint8_t *)&buf[1], bufSize - 1, !stop, make_timeout_time_ms(TIMEOUT_MS));
        AnalyzeRetVal(ret);

        if (ret >= 0)
        {
            retVal = (uint32_t)ret;
        }
    }

    return retVal;
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

    retValLast_ = retVal;
}

int I2C::GetRetValLast()
{
    return retValLast_;
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
        LogNNL(ToHex(row, false), " |");

        for (uint8_t col = 0x0; col <= 0xF; ++col)
        {
            uint8_t addr = row + col;

            I2C i2c(addr, instance);

            LogNNL(" ");
            if (i2c.IsAlive())
            {
                LogNNL(ToHex(addr, false));
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
        uint8_t addr = (uint8_t)FromHex(hexAddr);

        Log("Addr ", hexAddr, "(", addr, "): ", I2C::IsAlive(addr, Instance::I2C0) ? "alive" : "not alive");
    }, { .argCount = 1, .help = "I2C check hexAddr alive"});

    Shell::AddCommand("i2c0.read8", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)FromHex(hexReg);

        I2C i2c(addr, Instance::I2C0);
        uint8_t val = i2c.ReadReg8(reg);

        Log("Reading ", hexAddr, "(", addr, ") ", hexReg, "(", reg, ")");
        Log(ToHex(val), "(", val, ")(", ToBin(val), ")");
    }, { .argCount = 2, .help = "I2C read <hexAddr> <hexReg>"});

    Shell::AddCommand("i2c0.read16", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)FromHex(hexReg);

        I2C i2c(addr, Instance::I2C0);
        uint16_t val = i2c.ReadReg16(reg);

        Log("Reading ", hexAddr, "(", addr, ") ", hexReg, "(", reg, ")");
        Log(ToHex(val), "(", val, ")(", ToBin(val), ")");
    }, { .argCount = 2, .help = "I2C read <hexAddr> <hexReg>"});

    Shell::AddCommand("i2c0.write8", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)FromHex(hexReg);
        string hexVal = argList[2];
        uint8_t val = (uint8_t)FromHex(hexVal);

        I2C i2c(addr, Instance::I2C0);
        i2c.WriteReg8(reg, val);

        Log(ToHex(val), "(", val, ")(", ToBin(val), ")");
    }, { .argCount = 3, .help = "I2C write <hexAddr> <hexReg> <hexVal>"});

    Shell::AddCommand("i2c0.write16", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)FromHex(hexReg);
        string hexVal = argList[2];
        uint16_t val = (uint8_t)FromHex(hexVal);

        I2C i2c(addr, Instance::I2C0);
        i2c.WriteReg16(reg, val);

        Log(ToHex(val), "(", val, ")(", ToBin(val), ")");
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
        uint8_t addr = (uint8_t)FromHex(hexAddr);

        Log("Addr ", hexAddr, "(", addr, "): ", I2C::IsAlive(addr, Instance::I2C1) ? "alive" : "not alive");
    }, { .argCount = 1, .help = "I2C check hexAddr alive"});

    Shell::AddCommand("i2c1.read8", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)FromHex(hexReg);

        I2C i2c(addr, Instance::I2C1);
        uint8_t val = i2c.ReadReg8(reg);

        Log("Reading ", hexAddr, "(", addr, ") ", hexReg, "(", reg, ")");
        Log(ToHex(val), "(", val, ")(", ToBin(val), ")");
    }, { .argCount = 2, .help = "I2C read <hexAddr> <hexReg>"});

    Shell::AddCommand("i2c1.read16", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)FromHex(hexReg);

        I2C i2c(addr, Instance::I2C1);
        uint16_t val = i2c.ReadReg16(reg);

        Log("Reading ", hexAddr, "(", addr, ") ", hexReg, "(", reg, ")");
        Log(ToHex(val), "(", val, ")(", ToBin(val), ")");
    }, { .argCount = 2, .help = "I2C read <hexAddr> <hexReg>"});

    Shell::AddCommand("i2c1.write8", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)FromHex(hexReg);
        string hexVal = argList[2];
        uint8_t val = (uint8_t)FromHex(hexVal);

        I2C i2c(addr, Instance::I2C1);
        i2c.WriteReg8(reg, val);

        Log(ToHex(val), "(", val, ")(", ToBin(val), ")");
    }, { .argCount = 3, .help = "I2C write <hexAddr> <hexReg> <hexVal>"});

    Shell::AddCommand("i2c1.write16", [&](vector<string> argList){
        string hexAddr = argList[0];
        uint8_t addr = (uint8_t)FromHex(hexAddr);
        string hexReg = argList[1];
        uint8_t reg = (uint8_t)FromHex(hexReg);
        string hexVal = argList[2];
        uint16_t val = (uint8_t)FromHex(hexVal);

        I2C i2c(addr, Instance::I2C1);
        i2c.WriteReg16(reg, val);

        Log(ToHex(val), "(", val, ")(", ToBin(val), ")");
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

