#pragma once

#include "Log.h"
#include "PAL.h"
#include "Pin.h"
#include "Shell.h"
#include "HeapAllocators.h"
#include "Format.h"

#include "pico/rand.h"

#include <string.h>

#include <algorithm>
#include <vector>
#include <string>
#include <list>
#include <cmath>
using namespace std;


// thanks to (but lightly modified from)
// https://stackoverflow.com/questions/216823/how-to-trim-a-stdstring

// trim from end of string (right)
inline std::string& rtrim(std::string& s, string t = " \t\n\r\f\v")
{
    s.erase(s.find_last_not_of(t.c_str()) + 1);
    return s;
}

// trim from beginning of string (left)
inline std::string& ltrim(std::string& s, string t = " \t\n\r\f\v")
{
    s.erase(0, s.find_first_not_of(t.c_str()));
    return s;
}

// trim from both ends of string (right then left)
inline std::string& trim(std::string& s, string t = " \t\n\r\f\v")
{
    return ltrim(rtrim(s, t), t);
}

inline string& toupper(string &s)
{
    for (char &c : s)
    {
        c = toupper(c);
    }

    return s;
}

inline bool IsPrefix(const string &prefix, const string &target)
{
    bool retVal = false;
    
    if (prefix.length() <= target.length())
    {
        retVal = !strncmp(prefix.c_str(), target.c_str(), prefix.length());
    }

    return retVal;
}


// str is any string, optionally with delimiters
// delim can be a string of any size
// trimmed means whitespace stripped from each side of each element
// allowEmpty is tough
// - if you split on whitespace, you don't want empty substrings
// - if you split on comma, you do want empty (individual cells)
// - I'm tuning the defaults to be for whitespace
// 
//
// thanks to (but lightly modified from)
// https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c
inline
vector<string> Split(string str,
                     string delim = string{" "},
                     bool trimmed = true,
                     bool allowEmpty = false)
{
    vector<string> retVal;

    auto process = [&](string token){
        if (trimmed)
        {
            token = trim(token);
        }

        if (!token.empty() || allowEmpty)
        {
            retVal.push_back(token);
        }
    };

    size_t pos = 0;
    string token;
    while ((pos = str.find(delim)) != string::npos) {
        token = str.substr(0, pos);

        process(token);

        str.erase(0, pos + delim.length());
    }

    process(str);

    return retVal;
}


// assumes list is sorted smallest/largest
template <typename T>
int GetIdxAtPct(vector<T> valList, double pct)
{
    int retVal = 0;

    if (!valList.empty())
    {
        T &smallest = valList[0];
        T &largest  = valList[valList.size() - 1];
        T range = largest - smallest;

        // Log("smallest: ", smallest);
        // Log("largest: ", largest);
        // Log("range: ", range);

        double pctClosest = (double)(valList[0] - smallest) / (double)range;
        int idxClosest = 0;

        for (int i = 0; i < (int)valList.size(); ++i)
        {
            T &val = valList[i];
            // Log("Checking val ", val);

            double pctThis = (double)(val - smallest) / (double)range * 100.0;

            double diffClosest = fabs(pct - pctClosest);
            double diffThis    = fabs(pct - pctThis);

            // Log("pctThis     ", pctThis);
            // Log("diffClosest ", diffClosest);
            // Log("diffThis    ", diffThis);

            if (diffThis <= diffClosest)
            {
                // Log("this is the new best");
                pctClosest = pctThis;
                idxClosest = i;
            }
            else
            {
                // getting further away from the answer
                break;
            }

            // LogNL();
        }

        retVal = idxClosest;
    }

    return retVal;
}

inline char *ToString(uint32_t num)
{
    // strlen(4294967295) == 10, so we need 11 for the null
    static const uint8_t BUF_SIZE = 11;
    static char buf[BUF_SIZE];

    snprintf(buf, BUF_SIZE, "%u", num);

    return buf;
}

inline string ToHex(uint8_t *buf, uint8_t bufLen, bool addPrefix = true)
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

inline uint16_t Flip2(uint16_t val)
{
    uint16_t retVal = 
        ((val & 0xFF00) >> 8) |
        ((val & 0x00FF) << 8);

    return retVal;
}

// we are a little endian device, net is big endian, so flip
inline uint16_t ntohs(uint16_t val)
{
    return Flip2(val);
}

inline string ToHex(uint16_t val, bool addPrefix = true)
{
    uint16_t valBigEndian = Flip2(val);

    return ToHex((uint8_t *)&valBigEndian, 2);
}

inline string Commas(string num)
{
    string retVal;

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

template <typename T>
inline string Commas(T num)
{
    return Commas(to_string(num));
}

extern string &CommasStatic(const char *numStr);

inline string &CommasStatic(uint32_t num)
{
    return CommasStatic(ToString(num));
}


template <uint8_t CAPACITY>
class StackString
{
public:
    StackString()
    {
        Clear();
    }

    StackString &operator=(const char *str)
    {
        Clear();
        operator+=(str);
        return *this;
    }

    StackString &operator+=(const char *str)
    {
        size_t len = strlen(str);
        size_t spaceRemaining = CAPACITY - size_;

        if (len && spaceRemaining)
        {
            size_t bytesToCopy = min(len, spaceRemaining);
            memcpy(&buf_[size_], str, bytesToCopy);

            size_ += bytesToCopy;

            buf_[size_] = '\0';
        }

        return *this;
    }

    void Clear()
    {
        size_ = 0;
        memset(buf_, 0, CAPACITY + 1);
    }

    char *Data()
    {
        return buf_;
    }

    char *c_str()
    {
        return Data();
    }

private:
    uint8_t size_ = 0;
    char buf_[CAPACITY + 1];
};


// purposefully does not do operations which may allocate memory,
// thus this function is safe to call from ISRs
inline char *TimestampFromUs(uint64_t usTime)
{
    uint64_t time = usTime;

    // A 64-bit int of microseconds is, let's say, 5000 seconds, which
    // is 307,445,734,562 minutes, so 5,124,095,576 hours
    // So the max format will be:
    // HHHHHHHHHH:MM:SS.uuuuuu = 23
    // The compiler is worried that theoretical values could exceed
    // the buffer, so I'll just make it large enough for any possible
    // value fed in.
    static const uint8_t BUF_SIZE = 35 + 1;
    static char buf[BUF_SIZE];

    // extract microseconds
    uint32_t us = time % 1'000'000;
    time /= 1'000'000;

    // extract hours
    uint64_t hours = time / (60 * 60);
    time -= (hours * 60 * 60);

    // extract minutes
    uint8_t mins = time / 60;
    time -= mins * 60;

    // extract seconds
    uint8_t secs = time;

    // combine
    snprintf(buf, BUF_SIZE, "%lld:%02u:%02u.%06u", hours, mins, secs, us);

    return buf;
}

inline string TS()
{
    string retVal;

    retVal += "[";
    retVal += TimestampFromUs(PAL.Micros());
    retVal += "] ";

    return retVal;
}

// return the time without hours, with ms-resolution
inline string MsToMinutesStr(uint64_t ms)
{
    string ts = string{TimestampFromUs(ms  * 1'000)};

    auto pos = ts.find(':');

    if (pos != string::npos)
    {
        ts = ts.substr((uint8_t)pos + 1, 9);
    }

    return ts;
}


class StrUtl
{
public:
    static string PadLeft(uint64_t val, char padChar, uint8_t fieldWidthTotal)
    {
        return PadLeft(to_string(val), padChar, fieldWidthTotal);
    }

    static string PadLeft(string valStr, char padChar, uint8_t fieldWidthTotal)
    {
        int lenDiff = fieldWidthTotal - valStr.length();
        if (lenDiff > 0)
        {
            string valStrTmp;

            while (lenDiff)
            {
                valStrTmp += padChar;
                --lenDiff;
            }
            valStrTmp += valStr;

            valStr = valStrTmp;
        }

        return valStr;
    }

};


    template <typename T, uint8_t COUNT>
    class ObjectPool
    {
    public:
        ObjectPool()
        : objPtrList_(COUNT)
        {
            for (auto &obj : objList_)
            {
                objPtrList_.push_back(&obj);
            }
        }

        T *Borrow()
        {
            T *retVal = nullptr;

            if (objPtrList_.size())
            {
                retVal = objPtrList_[objPtrList_.size() - 1];
                objPtrList_.pop_back();
            }

            return retVal;
        }

        void Return(T *ptr)
        {
            if (ptr)
            {
                objPtrList_.push_back(ptr);
            }
        }

    private:
        array<T, COUNT> objList_;
        vector<T *> objPtrList_;
    };




// https://docs.zephyrproject.org/3.0.0/reference/random/index.html
// range is inclusive
// mod operator is undefined on negatives, so have to treat specially
// [-10, -5]
// 10 - 5 + 1 = 6
// rand % 6 = [0-5]
// -5 - [0-5] = [-10--5]
// yup, works

// [-5, 5]
// 5 - -5 + 1 = 11
// rand % 11 = [0-10]
// -5 + [0-10] = [-5-5]
// yup works

// [-5, 0]
// 0 - -5 + 1 = 6
// rand % 6 = [0-5]
// -5 + [0-5] = [-5-0]
// yup, works

// [5, 10]
// 10 - 5 + 1 = 6
// rand % 6 = [0-5]
// 5 + [0-5] = [5-10]
// yup, works
inline uint32_t RandInRange(int32_t rangeLow, int32_t rangeHigh)
{
    int32_t retVal = 0;

    if (rangeLow <= rangeHigh)
    {
        if (rangeLow < 0 && rangeHigh < 0)
        {
            retVal = rangeLow - (get_rand_32() % (-rangeLow - -rangeHigh + 1));
        }
        else // rangeLow can be neg/0/pos, rangeHigh >= 0
        {
            retVal = rangeLow + (get_rand_32() % (rangeHigh - rangeLow + 1));
        }
    }

    return retVal;
}


// uint16_t Swap2(uint16_t val)
// {
//     uint16_t retVal;

//     uint8_t *from = (uint8_t *)&val;
//     uint8_t *to   = (uint8_t *)&retVal;

//     to[0] = from[1];
//     to[1] = from[0];

//     return retVal;
// }

// uint32_t Swap4(uint32_t val)
// {
//     uint32_t retVal;

//     uint8_t *from = (uint8_t *)&val;
//     uint8_t *to   = (uint8_t *)&retVal;

//     to[0] = from[3];
//     to[1] = from[2];
//     to[2] = from[1];
//     to[3] = from[0];

//     return retVal;
// }

// nRF52840 is Little Endian
// Some other things are also, want to be able to not know host endianness
inline uint16_t ltohs(uint16_t val) { return val; }
inline uint32_t ltohl(uint32_t val) { return val; }
inline uint16_t htols(uint16_t val) { return val; }
inline uint32_t htoll(uint32_t val) { return val; }




// meant to extract a range of bits from a bitfield and shift them to
// all the way to the right.
// high and low bit are inclusive
constexpr uint16_t BitsGet(uint16_t val, uint8_t highBit, uint8_t lowBit)
{
    uint16_t retVal = val;

    // Log(Format::ToBin(val), " [", highBit, ", ", lowBit, "]");

    if (highBit >= lowBit && highBit <= 15)
    {
        uint8_t bitCount = highBit - lowBit + 1;
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

        // Log("Mask: ", Format::ToBin(mask));
        // Log("Val : ", Format::ToBin(val));

        // apply mask
        retVal = mask & val;
        // Log("Res : ", Format::ToBin(retVal));

        // shift back
        retVal = retVal >> bitShiftCount;
        // Log("Res2: ", Format::ToBin(retVal));
    }

    return retVal;
}

constexpr uint16_t BitsPut(uint16_t val, uint8_t highBit, uint8_t lowBit)
{
    uint16_t retVal = val;

    if (highBit >= lowBit && highBit <= 15)
    {
        uint8_t bitCount = highBit - lowBit + 1;
        uint8_t bitShiftCount = lowBit;

        // Log("bitcount: ", bitCount, ", shiftBy: ", bitShiftCount);

        // create the mask
        uint16_t mask = 0;
        for (int i = 0; i < bitCount; ++i)
        {
            mask <<= 1;
            mask |= 1;
        }

        // Log("Mask: ", Format::ToBin(mask));
        // Log("Val : ", Format::ToBin(val));

        // apply mask
        retVal = mask & val;
        // Log("Res : ", Format::ToBin(retVal));

        // shift back
        retVal = retVal << bitShiftCount;
        // Log("Res2: ", Format::ToBin(retVal));
    }

    return retVal;
}

#include "IDMaker.h"

template <typename T>
class Average
{
    static const uint16_t DEFAULT_MAX_SIZE = 10;
public:
    Average(uint16_t maxSize = DEFAULT_MAX_SIZE)
    : maxSize_(maxSize)
    {
        valList_.reserve(maxSize_);
    }

    T AddSample(T val)
    {
        if (valList_.size() < maxSize_)
        {
            valList_.push_back(val);

            sum_ += val;
        }
        else
        {
            sum_ -= valList_[idxNext_];

            valList_[idxNext_] = val;

            sum_ += val;

            idxNext_ = (idxNext_ + 1) % maxSize_;
        }

        average_ = round((double)sum_ / valList_.size());

        return average_;
    }

    T GetAverage()
    {
        return average_;
    }


private:
    vector<T> valList_;
    uint16_t maxSize_ = DEFAULT_MAX_SIZE;
    T sum_ = 0;
    uint16_t idxNext_ = 0;
    T average_ = 0;
};


// positive values mean rotate right
// negative values mean rotate left
// zero results in no rotation
template <typename T>
vector<T> &Rotate(vector<T> &valList, int count)
{
    if (count > 0)
    {
        count = count % valList.size();
        rotate(valList.begin(), valList.begin() + (valList.size() - count), valList.end());
    }
    else
    {
        count = -count;
        count = count % valList.size();

        rotate(valList.begin(), valList.begin() +  count, valList.end());
    }

    return valList;
}







extern void UtlSetupShell();