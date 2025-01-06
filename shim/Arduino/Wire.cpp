#include "Wire.h"
#include "Log.h"
#include "Utl.h"

#include <vector>
using std::vector;

#include "StrictMode.h"


TwoWire Wire(0);



// https://github.com/arduino/ArduinoCore-avr/blob/master/libraries/Wire/src/Wire.h
// https://github.com/arduino/ArduinoCore-avr/blob/master/libraries/Wire/src/Wire.cpp


TwoWire::TwoWire(uint8_t addr, I2C::Instance instance)
: i2c_(addr, instance)
{
    // nothing to do
    // Log("[TwoWire::TwoWire() this(", (uint32_t)this, ")->addr_ = ", addr, "]");
}






void TwoWire::begin()
{
    // nothing to do
    // other variants of begin set the target i2c address
}

void TwoWire::end()
{
    // nothing to do
    // real end() disables I2C
}







void TwoWire::beginTransmission(uint8_t addr)
{
    // begin queuing outbound data
    inTransmission_ = true;
    addrTransmission_ = addr;
    queueWrite_.clear();


    queueWrite_.push_back(addr);
}

size_t TwoWire::write(uint8_t data)
{
    return write(&data, 1);
}

size_t TwoWire::write(const uint8_t *buf, size_t bufSize)
{
    // I need to queue this possibly
    if (buf)
    {
        if (inTransmission_)
        {
            for (size_t i = 0; i < bufSize; ++i)
            {
                queueWrite_.push_back(buf[i]);
            }

            // Log("Queuing ", bufSize, " bytes, now: ", ContainerToString(queueWrite_));
        }
        else
        {
            // I have never seen this actually invoked. Hopefully works.
            i2c_.WriteRaw((uint8_t *)buf, (uint32_t)bufSize);
        }
    }

    // returns the number of bytes accepted
    return (size_t)bufSize;
}

uint8_t TwoWire::endTransmission()
{
    return endTransmission(true);
}

uint8_t TwoWire::endTransmission(uint8_t stop)
{
    // this would flush the queued data to the register address which was specified
    // at beginTransmission

    inTransmission_ = false;
    i2c_.WriteRaw((uint8_t *)queueWrite_.data(), (uint32_t)queueWrite_.size(), stop);

    // Log("endTx(", addrTransmission_, ", ", stop, "): ", ContainerToString(queueWrite_));

    // https://github.com/arduino/ArduinoCore-avr/blob/master/libraries/Wire/src/Wire.cpp#L217
    // https://github.com/arduino/ArduinoCore-avr/blob/c8c514c9a19602542bc32c7033f48fecbbda4401/libraries/Wire/src/utility/twi.c#L246
    // the return value is 0 on success
    return 0;
}









size_t TwoWire::requestFrom(uint8_t addr, uint8_t len)
{
    return requestFrom(addr, len, true);
}

size_t TwoWire::requestFrom(uint8_t addr, uint8_t len, uint8_t stop)
{
    // real does a transactional write to device indicating read target register
    // then does a blocking read for those bytes
    // - and queues up those bytes to be served up later as requested by read()

    queueRead_.clear();
    queueRead_.resize(len);

    // Log("RequestFrom(", addr, ", ", len, ", ", stop, ")");

    size_t retVal = i2c_.ReadRaw((uint8_t *)queueRead_.data(), len, stop);

    // returns the number of bytes actually read
    return retVal;
}

int TwoWire::available()
{
    return (int)queueRead_.size();
}

int TwoWire::read()
{
    int retVal = 0;

    // pops the data that was acquired by requestFrom, calling code has to know
    // that quantity so it can call read() that many times

    // LogNNL("read(", ContainerToString(queueRead_), ") = ");

    if (queueRead_.size())
    {
        retVal = queueRead_[0];

        queueRead_.erase(queueRead_.begin());
    }

    // Log(retVal);

    return retVal;
}













void TwoWire::setClock(uint32_t)
{
    // nothing to do

    // real would set the baud
}

