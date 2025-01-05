#include "BitField.h"

#include "StrictMode.h"



BitField::BitField()
{
    Attach(nullptr, 0);
}

BitField::BitField(uint8_t *buf, uint8_t bufSize)
{
    Attach(buf, bufSize);
}

void BitField::Attach(uint8_t *buf, uint8_t bufSize)
{
    buf_     = buf;
    bufSize_ = bufSize;
}

uint8_t BitField::SetBitAt(uint16_t bitIdx, uint8_t bitVal)
{
    uint8_t retVal = 0;
    
    uint8_t byteIdx;
    uint8_t bitInByte;
    
    if (GetIndexesIfInRange(bitIdx, byteIdx, bitInByte))
    {
        retVal = 1;
        
        buf_[byteIdx] = (uint8_t)((buf_[byteIdx] & ~(1 << bitInByte)) | (bitVal << bitInByte));
    }
    
    return retVal;
}

uint8_t BitField::GetBitAt(uint16_t bitIdx, uint8_t &bitVal) const
{
    uint8_t retVal = 0;
    
    bitVal = 0;
    
    uint8_t byteIdx;
    uint8_t bitInByte;
    
    if (GetIndexesIfInRange(bitIdx, byteIdx, bitInByte))
    {
        retVal = 1;
        
        bitVal = (buf_[byteIdx] & (1 << bitInByte)) ? 1 : 0;
    }

    return retVal;
}

uint8_t BitField::Size() const
{
    return bufSize_ * 8;
}

uint8_t BitField::GetBuf(uint8_t *&buf, uint8_t &bufSize)
{
    uint8_t retVal = 0;
    
    if (buf_)
    {
        retVal = 1;
        
        buf     = buf_;
        bufSize = bufSize_;
    }
    
    return retVal;
}

uint8_t BitField::GetIndexesIfInRange(uint16_t bitIdx, uint8_t &byteIdx, uint8_t &bitInByte) const
{
    uint8_t retVal = 0;
    
    if (buf_)
    {
        byteIdx = (uint8_t)(bitIdx / 8);
        
        if (byteIdx < bufSize_)
        {
            retVal = 1;
            
            bitInByte = (uint8_t)(7 - (bitIdx - (byteIdx * 8)));   // from rhs
        }
    }
    
    return retVal;
}


