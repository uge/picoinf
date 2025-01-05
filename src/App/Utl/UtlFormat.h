#pragma once

#include "UtlEndian.h"

#include <cstdint>
#include <cstdio>
#include <string>




extern std::string Commas(std::string num);
template <typename T>
inline std::string Commas(T num)
{
    return Commas(std::to_string(num));
}

extern std::string &CommasStatic(const char *numStr);
extern std::string &CommasStatic(uint32_t num);
extern std::string &CommasStatic(uint64_t num);





extern std::string ToHex(const uint8_t *buf, uint8_t bufLen, bool addPrefix = true);
extern std::string ToHex(uint64_t val, bool addPrefix = true);
extern std::string ToHex(uint32_t val, bool addPrefix = true);
extern std::string ToHex(uint16_t val, bool addPrefix = true);
extern std::string ToHex(uint8_t val, bool addPrefix = true);

// supports 0x prefix, and all caps or not
extern const uint64_t FromHex(std::string val);

extern std::string ToBin(uint8_t *buf, uint8_t  bufLen, bool addPrefix = true);
extern std::string ToBin(uint32_t val, bool addPrefix = true);
extern std::string ToBin(uint16_t val, bool addPrefix = true);
extern std::string ToBin(uint8_t val, bool addPrefix = true, uint8_t places = 8);







template <typename T>
std::pair<const char *, int> FormatStrC(char *buf, uint8_t bufSizeIn, const char *fmt, T val)
{
    int bufSize = snprintf((char *) buf, bufSizeIn, fmt, val);

    return { buf, bufSize }; 
}

template <typename T>
std::pair<const char *, int> FormatStrC(const char *fmt, T val)
{
    static const uint8_t BUF_SIZE = 64;
    static char BUF[BUF_SIZE];

    return FormatStrC(BUF, BUF_SIZE, fmt, val);
}

template <typename T>
std::string FormatStr(std::string fmt, T val)
{
    auto [buf, bufSize] = FormatStrC(fmt.c_str(), val);

    return buf;
}


