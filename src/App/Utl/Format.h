#pragma once

#include "Log.h"

#include <cstdint>
#include <string>
#include <utility>


// uint64_t max = 18446744073709551615 (20 chars) + 1 null
// let's just be safe and go big though
class FormatIsrBase
{
public:
    inline static const uint8_t BUF_SIZE = 64;
protected:
    inline static char BUF[BUF_SIZE];
};

class FormatBase
{
public:
    inline static const uint8_t BUF_SIZE = 64;
protected:
    inline static char BUF[BUF_SIZE];
};

template <typename FormatTemplateBase>
class FormatTemplate
: public FormatTemplateBase
{
public:

    ///////////////////////////////////////////////////////////////////////////
    // Base functionality which doesn't do strings or dynamic allocation.
    // Good for low-level non-application-facing code.
    // ISRs ok.
    ///////////////////////////////////////////////////////////////////////////


    template <typename T>
    static std::pair<const char *, int> StrC(char *buf, uint8_t bufSizeIn, const char *fmt, T val)
    {
        int bufSize = snprintf((char *) buf, bufSizeIn, fmt, val);

        return { buf, bufSize }; 
    }

    template <typename T>
    static std::pair<const char *, int> StrC(const char *fmt, T val)
    {
        return StrC(FormatTemplateBase::BUF, FormatTemplateBase::BUF_SIZE, fmt, val);
    }

    ///////////////////////////////////////////////////////////////////////////
    // Common use-case, fat strings.
    // Not good for ISRs
    ///////////////////////////////////////////////////////////////////////////
    template <typename T>
    static std::string Str(std::string fmt, T val)
    {
        auto [buf, bufSize] = StrC(fmt.c_str(), val);

        return buf;
    }

    static std::string ToHex(uint8_t val, bool prependIdentifier = true)
    {
        auto buf = Str("%02X", val);

        if (prependIdentifier)
        {
            buf = std::string{"0x"} + buf;
        }

        return buf;
    }

    static std::string ToHex(uint16_t val, bool prependIdentifier = true)
    {
        auto buf = Str("%04X", val);

        if (prependIdentifier)
        {
            buf = std::string{"0x"} + buf;
        }

        return buf;
    }

    static std::string ToHex(uint32_t val, bool prependIdentifier = true)
    {
        auto buf = Str("%08X", val);

        if (prependIdentifier)
        {
            buf = std::string{"0x"} + buf;
        }

        return buf;
    }

    static std::string ToHex(uint64_t val, bool prependIdentifier = true)
    {
        uint32_t u321 = (val >> 32) & 0xFFFFFFFF;
        uint32_t u322 = val & 0xFFFFFFFF;

        auto buf = Str("%08X", u321);
        buf += Str("%08X", u322);

        if (prependIdentifier)
        {
            buf = std::string{"0x"} + buf;
        }

        return buf;
    }

    // supports 0x prefix, and all caps or not
    static const uint64_t FromHex(std::string val)
    {
        uint64_t retVal = 0;
        const char *str = val.c_str();

        sscanf(str, "%llx", &retVal);

        return retVal;
    }

    static std::string ToBin(uint8_t val, bool prependIdentifier = true, uint8_t places = 8)
    {
        std::string retVal;

        uint8_t mask = 0b10000000;

        for (int i = 0; i < 8; ++i)
        {
            FormatTemplateBase::BUF[i] = (val & mask) ? '1' : '0';

            mask >>= 1;
        }

        FormatTemplateBase::BUF[8] = '\0';

        if (prependIdentifier)
        {
            retVal += "0b";
        }

        if (places >= 8)
        {
            retVal += FormatTemplateBase::BUF;
        }
        else
        {
            retVal += &FormatTemplateBase::BUF[8 - places];
        }

        return retVal;
    }

    static std::string ToBin(uint16_t val, bool prependIdentifier = true)
    {
        std::string retVal;

        // MSB
        uint8_t b1 = (uint8_t)((val >> 8) & 0xFF);
        uint8_t b2 = (uint8_t)(val & 0xFF);

        retVal += ToBin(b1, prependIdentifier);
        retVal += " ";
        retVal += ToBin(b2, prependIdentifier);

        return retVal;
    }

    static std::string ToBin(uint32_t val, bool prependIdentifier = true)
    {
        std::string retVal;

        // MSB
        uint16_t u161 = (uint16_t)((val >> 16) & 0xFFFF);
        uint16_t u162 = (uint16_t)(val & 0xFFFF);

        retVal += ToBin(u161, prependIdentifier);
        retVal += " ";
        retVal += ToBin(u162, prependIdentifier);

        return retVal;
    }

    static std::string ToBin(uint64_t val, bool prependIdentifier = true)
    {
        std::string retVal;

        // MSB
        uint32_t u321 = (uint32_t)((val >> 32) & 0xFFFFFFFF);
        uint32_t u322 = (uint32_t)(val & 0xFFFFFFFF);

        retVal += ToBin(u321, prependIdentifier);
        retVal += " ";
        retVal += ToBin(u322, prependIdentifier);

        return retVal;
    }

    template <typename T>
    static void PrintAll(std::string str, T val)
    {
        Log(str, ": ", val, ", ", ToHex(val), ", ", ToBin(val));
        std::string toHex = ToHex(val);
        T valParsed = (T)FromHex(toHex);

        if (valParsed == val)
        {
            Log("Hex parse OK");
        }
        else
        {
            Log("Hex parse FAIL");
            Log("ToHex: ", toHex);
            Log("FromHex: ", valParsed);
        }
        LogNL();
    }

    static void Test()
    {
        LogNL();
        Log("Format::Test");
        LogNL();

        ////////////////////
        // uint8_t
        ////////////////////

        uint8_t val8 = 0b0000'0001; // 1 ; 0x01
        PrintAll("val8", val8);

        ////////////////////
        // uint16_t
        ////////////////////

        uint16_t val16 = (uint16_t)0b0000'0001'0010'0011;   // 291 ; 0x123
        PrintAll("val16", val16);

        ////////////////////
        // uint32_t
        ////////////////////

        uint32_t val32 = (uint32_t)0b0000'0001'0010'0011'0100'0101'0110'0111;   // 19,088,743 ; 0x01234567
        PrintAll("val32", val32);

        ////////////////////
        // uint64_t
        ////////////////////

        uint64_t val64 = (uint64_t)0b0000'0001'0010'0011'0100'0101'0110'0111'1000'1001'1010'1011'1100'1101'1110'1111;    // 81,985,529,216,486,895 ; 0x0123456789ABCDEF
        PrintAll("val64", val64);
    }

private:
};

using FormatISR = FormatTemplate<FormatIsrBase>;
using Format    = FormatTemplate<FormatBase>;
