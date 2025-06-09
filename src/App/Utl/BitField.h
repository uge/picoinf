#pragma once

#include <cstdint>


class BitField
{
public:
    BitField();
    BitField(uint8_t *buf, uint8_t bufSize);
    void Attach(uint8_t *buf, uint8_t bufSize);
    uint8_t SetBitAt(uint16_t bitIdx, uint8_t bitVal);
    
    template <typename T>
    uint8_t SetBitRangeAt(uint16_t bitIdx, T bitList, uint8_t bitCount)
    {
        uint8_t retVal = 0;
        
        uint8_t bitOffset = 0;
        for (int8_t bitInByte = bitCount - 1; bitInByte >= 0; --bitInByte)
        {
            uint8_t bitVal = (bitList & ((T)1 << bitInByte)) ? 1 : 0;
            
            retVal = SetBitAt(bitIdx + bitOffset, bitVal);
            
            ++bitOffset;
        }
        
        return retVal;
    }

    uint8_t GetBitAt(uint16_t bitIdx, uint8_t &bitVal) const;
    
    template <typename T>
    uint8_t GetBitRangeAt(uint16_t bitIdx, T &bitList, uint8_t bitCount) const
    {
        uint8_t retVal = 0;
        
        bitList = 0;
        
        uint8_t bitOffset = 0;
        for (int8_t bitInByte = bitCount - 1; bitInByte >= 0; --bitInByte)
        {
            uint8_t bitVal;
            retVal = GetBitAt(bitIdx + bitOffset, bitVal);
            
            if (retVal)
            {
                bitList = (bitList & ~(1 << bitInByte)) | ((T)bitVal << bitInByte);
            }
            
            ++bitOffset;
        }
        
        return retVal;
    }
    
    uint8_t Size() const;
    uint8_t GetBuf(uint8_t *&buf, uint8_t &bufSize);
    

private:

    uint8_t GetIndexesIfInRange(uint16_t bitIdx, uint8_t &byteIdx, uint8_t &bitInByte) const;

    uint8_t *buf_;
    uint8_t  bufSize_;
};


template <uint8_t BIT_COUNT>
class BitFieldOwned
: public BitField
{
    static const uint8_t BUF_SIZE = (BIT_COUNT / 8) + ((BIT_COUNT % 8) ? 1 : 0);
public:

    BitFieldOwned()
    : BitField(buf_, BUF_SIZE)
    {
        // Nothing to do
    }

private:
    uint8_t buf_[BUF_SIZE] = { 0 };
};


















