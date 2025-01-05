#pragma once

#include <cstdint>


// RP2040 is Little Endian
// Some other things are also, want to be able to not know host endianness
inline uint16_t ltohs(uint16_t val) { return val; }
inline uint32_t ltohl(uint32_t val) { return val; }
inline uint16_t htols(uint16_t val) { return val; }
inline uint32_t htoll(uint32_t val) { return val; }

inline uint64_t Flip8(uint64_t val)
{
    uint64_t retVal =
        ((val & 0xFF'00'00'00'00'00'00'00) >> 56) |
        ((val & 0x00'FF'00'00'00'00'00'00) >> 40) |
        ((val & 0x00'00'FF'00'00'00'00'00) >> 24) |
        ((val & 0x00'00'00'FF'00'00'00'00) >>  8) |
        ((val & 0x00'00'00'00'FF'00'00'00) <<  8) |
        ((val & 0x00'00'00'00'00'FF'00'00) << 24) |
        ((val & 0x00'00'00'00'00'00'FF'00) << 40) |
        ((val & 0x00'00'00'00'00'00'00'FF) << 56);

    return retVal;
}

inline uint32_t Flip4(uint32_t val)
{
    uint32_t retVal =
        ((val & 0xFF000000) >> 24) |
        ((val & 0x00FF0000) >>  8) |
        ((val & 0x0000FF00) <<  8) |
        ((val & 0x000000FF) << 24);

    return retVal;
}

inline uint16_t Flip2(uint16_t val)
{
    uint16_t retVal = 
        ((val & 0xFF00) >> 8) |
        ((val & 0x00FF) << 8);

    return retVal;
}

// we are a little endian device, net is big endian, so flip
inline uint32_t ntohl(uint32_t val)
{
    return Flip4(val);
}

inline uint16_t ntohs(uint16_t val)
{
    return Flip2(val);
}

inline uint32_t htonl(uint32_t val)
{
    return Flip4(val);
}

inline uint16_t htons(uint16_t val)
{
    return Flip2(val);
}

inline uint64_t ToBigEndian(uint64_t val)
{
    return Flip8(val);
}

inline uint32_t ToBigEndian(uint32_t val)
{
    return htonl(val);
}

inline uint16_t ToBigEndian(uint16_t val)
{
    return htons(val);
}

inline uint64_t ToLittleEndian(uint64_t val)
{
    return val;
}

inline uint32_t ToLittleEndian(uint32_t val)
{
    return val;
}

inline uint16_t ToLittleEndian(uint16_t val)
{
    return val;
}
