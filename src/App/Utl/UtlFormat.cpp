#include "UtlFormat.h"
#include "UtlString.h"

#include <algorithm>
#include <cstdio>
using namespace std;

#include "StrictMode.h"


string ToHex(const uint8_t *buf, uint8_t bufLen, bool addPrefix)
{
    const char *hexList = "0123456789ABCDEF";

    string retVal;

    if (buf && bufLen)
    {
        if (addPrefix)
        {
            retVal = "0x";
        }

        for (int i = 0; i < bufLen; ++i)
        {
            char h1 = hexList[(buf[i] & 0xF0) >> 4];
            char h2 = hexList[(buf[i] & 0x0F)];

            retVal.push_back(h1);
            retVal.push_back(h2);
        }
    }

    return retVal;
}

string ToHex(uint64_t val, bool addPrefix)
{
    uint64_t valBigEndian = ToBigEndian(val);

    return ToHex((uint8_t *)&valBigEndian, 8, addPrefix);
}

string ToHex(uint32_t val, bool addPrefix)
{
    uint32_t valBigEndian = ToBigEndian(val);

    return ToHex((uint8_t *)&valBigEndian, 4, addPrefix);
}

string ToHex(uint16_t val, bool addPrefix)
{
    uint16_t valBigEndian = ToBigEndian(val);

    return ToHex((uint8_t *)&valBigEndian, 2, addPrefix);
}

string ToHex(uint8_t val, bool addPrefix)
{
    return ToHex(&val, 1, addPrefix);
}

string ToBin(uint8_t *buf, uint8_t  bufLen, bool addPrefix)
{
    const char *binList = "01";

    string retVal;

    if (buf && bufLen)
    {
        if (addPrefix)
        {
            retVal = "0x";
        }

        string sepByte = "";
        string sepNibble = "";

        for (int i = 0; i < bufLen; ++i)
        {
            retVal += sepByte;

            uint8_t byte = buf[i];

            for (int j = 0; j < 4; ++j)
            {
                char bit = binList[byte & 0x01];
                byte >>= 1;

                retVal.push_back(bit);
            }

            retVal += sepNibble;

            for (int j = 0; j < 4; ++j)
            {
                char bit = binList[byte & 0x01];
                byte >>= 1;

                retVal.push_back(bit);
            }
            
            sepByte = "'";
        }
    }

    return retVal;
}

string ToBin(uint32_t val, bool addPrefix)
{
    uint32_t valBigEndian = htonl(val);

    return ToBin((uint8_t *)&valBigEndian, 4, addPrefix);
}

string ToBin(uint16_t val, bool addPrefix)
{
    uint16_t valBigEndian = htons(val);

    return ToBin((uint8_t *)&valBigEndian, 2, addPrefix);
}

std::string ToBin(uint8_t val, bool addPrefix, uint8_t places)
{
    string retVal;

    char buf[9];

    uint8_t mask = 0b10000000;

    for (int i = 0; i < 8; ++i)
    {
        buf[i] = (val & mask) ? '1' : '0';

        mask >>= 1;
    }

    buf[8] = '\0';

    if (addPrefix)
    {
        retVal += "0b";
    }

    if (places >= 8)
    {
        retVal += buf;
    }
    else
    {
        retVal += &buf[8 - places];
    }

    return retVal;
}


// supports 0x prefix, and all caps or not
const uint64_t FromHex(std::string val)
{
    uint64_t retVal = 0;
    const char *str = val.c_str();

    sscanf(str, "%llx", &retVal);

    return retVal;
}











string Commas(string num)
{
    string retVal;

    vector<string> partList = Split(num, ".");
    string numWhole = partList[0];

    reverse(numWhole.begin(), numWhole.end());

    int count = 0;
    for (auto c : numWhole)
    {
        ++count;

        if (count == 4)
        {
            retVal.push_back(',');
            count = 1;
        }

        retVal.push_back(c);
    }

    reverse(retVal.begin(), retVal.end());

    if (partList.size() > 1)
    {
        partList.erase(partList.begin());

        retVal += "." + Join(partList, ".");
    }

    return retVal;
}




static string num(55, '\0');
static string retVal(55, '\0');

string &CommasStatic(const char *numStr)
{
    num = numStr;
    retVal.clear();

    reverse(num.begin(), num.end());

    int count = 0;
    for (auto c : num)
    {
        ++count;

        if (count == 4)
        {
            retVal.push_back(',');
            count = 1;
        }

        retVal.push_back(c);
    }

    reverse(retVal.begin(), retVal.end());

    return retVal;
}

string &CommasStatic(uint32_t num)
{
    return CommasStatic(ToString((uint64_t)num));
}

string &CommasStatic(uint64_t num)
{
    return CommasStatic(ToString(num));
}





























