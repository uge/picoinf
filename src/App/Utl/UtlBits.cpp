#include "UtlBits.h"

using namespace std;

#include "StrictMode.h"


// meant to extract a range of bits from a bitfield and shift them to
// all the way to the right.
// high and low bit are inclusive
uint16_t BitsGet(uint16_t val, uint8_t highBit, uint8_t lowBit)
{
    uint16_t retVal = val;

    // Log(ToBin(val), " [", highBit, ", ", lowBit, "]");

    if (highBit >= lowBit && highBit <= 15)
    {
        uint8_t bitCount = (uint8_t)(highBit - lowBit + 1);
        uint8_t bitShiftCount = lowBit;

        // Log("bitcount: ", bitCount, ", shiftBy: ", bitShiftCount);

        // create the mask
        uint16_t mask = 0;
        for (int i = 0; i < bitCount; ++i)
        {
            mask <<= 1;
            mask |= 1;
        }
        mask = mask << bitShiftCount;

        // Log("Mask: ", ToBin(mask));
        // Log("Val : ", ToBin(val));

        // apply mask
        retVal = mask & val;
        // Log("Res : ", ToBin(retVal));

        // shift back
        retVal = retVal >> bitShiftCount;
        // Log("Res2: ", ToBin(retVal));
    }

    return retVal;
}

uint16_t BitsPut(uint16_t val, uint8_t highBit, uint8_t lowBit)
{
    uint16_t retVal = val;

    if (highBit >= lowBit && highBit <= 15)
    {
        uint8_t bitCount = (uint8_t)(highBit - lowBit + 1);
        uint8_t bitShiftCount = lowBit;

        // Log("bitcount: ", bitCount, ", shiftBy: ", bitShiftCount);

        // create the mask
        uint16_t mask = 0;
        for (int i = 0; i < bitCount; ++i)
        {
            mask <<= 1;
            mask |= 1;
        }

        // Log("Mask: ", ToBin(mask));
        // Log("Val : ", ToBin(val));

        // apply mask
        retVal = mask & val;
        // Log("Res : ", ToBin(retVal));

        // shift back
        retVal = retVal << bitShiftCount;
        // Log("Res2: ", ToBin(retVal));
    }

    return retVal;
}
