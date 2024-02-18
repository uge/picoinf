#pragma once

#include "Utl.h"

#include <algorithm>
#include <array>
#include <string>
using namespace std;


class UUID
{
public:
    UUID()
    {
        // Nothing to do
    }

    UUID(string uuid)
    {
        SetUUID(uuid);
    }

    void Reset()
    {
        for (auto &b : buf_)
        {
            b = 0;
        }

        reversed_ = false;
    }

    void ReverseBytes()
    {
        reversed_ = !reversed_;

        ReverseBytesInternal();
    }

    int GetBitCount()
    {
        int retVal = 0;

        if (uuid_.length() == 4 || (uuid_.length() == 6 && uuid_.substr(0, 2) == "0x"))
        {
            retVal = 16;
        }
        else if (uuid_.length() == 32 + 4)
        {
            retVal = 128;
        }

        return retVal;
    }

    // Support both 16-bit and 128-bit strings.
    //
    // 16 bit:
    // XXXX or 0xXXXX
    //
    // 128-bit:
    // 4 hex, 2 hex, 2 hex, 2 hex, 6 hex
    // 123e4567-e89b-12d3-a456-426614174000
    // xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx
    bool SetUUID(string uuid)
    {
        // capture raw copy
        uuid_ = uuid;

        // Trim a leading 0x (for the 16 bit ones)
        if (uuid.length() >= 2 && uuid.substr(0, 2) == "0x")
        {
            uuid = uuid.substr(2);
        }

        bool retVal = false;

        const char *p    = uuid.c_str();
        int lenRemaining = uuid.length();
        int remaining    = (GetBitCount() / 8);
        int bufIdx       = 0;

        while (remaining && lenRemaining >= 2)
        {
            if (*p == '-')
            {
                --lenRemaining;
                p += 1;
                continue;
            }
            else
            {
                char p0 = p[0];
                char p1 = p[1];

                if (IsHex(p0) && IsHex(p1))
                {
                    buf_[bufIdx] = HexToByte(p);

                    // move next byte in parsed byte buffer
                    ++bufIdx;

                    // increment to next ascii hex byte
                    p += 2;

                    // 2 hex chars removed from input string
                    lenRemaining -= 2;

                    // 1 less byte expected to be parsed
                    --remaining;
                }
                else
                {
                    break;
                }
            }
        }

        if (lenRemaining || remaining)
        {
            retVal = true;
        }

        if (reversed_)
        {
            ReverseBytesInternal();
        }

        return retVal;
    }

    uint8_t *GetBytes()
    {
        return (uint8_t *)&buf_;
    }

    uint8_t GetBytesLen()
    {
        return buf_.size();
    }

    vector<uint8_t> GetByteList()
    {
        vector<uint8_t> byteList;

        for (int i = 0; i < buf_.size(); ++i)
        {
            byteList.push_back(buf_[i]);
        }

        return byteList;
    }

    // Taking from a BLE formatted payload and reverse-formatting it
    void SetBytesReversed(const uint8_t *buf, uint8_t bufLen)
    {
        Reset();

        string uuidStr = MakeUUIDStrFromReversedBytes((uint8_t *)buf, bufLen);

        SetUUID(uuidStr);
    }

    static string MakeUUIDStrFromReversedBytes(uint8_t *buf, uint8_t bufLen)
    {
        string retVal;

        if (buf && (bufLen ==2 || bufLen == 16))
        {
            vector<uint8_t> byteList;

            for (int i = 0; i < bufLen; ++i)
            {
                byteList.push_back(buf[i]);
            }

            reverse(byteList.begin(), byteList.end());

            retVal = MakeUUIDStrFromBytes(byteList.data(), byteList.size());
        }

        return retVal;
    }

    static string MakeUUIDStrFromBytes(uint8_t *buf, uint8_t bufLen)
    {
        string uuid;

        if (buf && (bufLen == 2 || bufLen == 16))
        {
            vector<int> arr;
            if (bufLen == 16)
            {
                arr = { 4, 2, 2, 2, 6 };
            }
            else if (bufLen == 2)
            {
                arr = { 2 };
            }

            int bufIdx = 0;
            char sep = '\0';
            for (int count : arr)
            {
                if (sep != '\0')
                {
                    uuid += sep;
                }
                sep = '-';

                for (int i = 0; i < count; ++i)
                {
                    uuid += ToHex(buf[bufIdx]);

                    ++bufIdx;
                }
            }

            if (bufLen == 2)
            {
                uuid = string{"0x"} + uuid;
            }
        }

        return uuid;
    }

    uint16_t GetUint16()
    {
        uint16_t retVal = 0;

        if (reversed_)
        {
            ReverseBytesInternal();
        }

        uint8_t *pTarget = (uint8_t *)&retVal;
        uint8_t *pSource = GetBytes();

        // fill the int in Big Endian, which is also Network Byte Order
        for (int i = 0; i < 2; ++i)
        {
            pTarget[i] = pSource[i];
        }

        // convert from known network byte order to host byte order
        retVal = ntohs(retVal);

        if (reversed_)
        {
            ReverseBytesInternal();
        }

        return retVal;
    }

    string GetUUID()
    {
        return uuid_;
    }

private:
    static string ToHex(uint8_t b)
    {
        string hex;

        hex += HexChar(b, 0);
        hex += HexChar(b, 1);

        return hex;
    }

    static char HexChar(uint8_t b, int loc)
    {
        char c = (char)((b >> (loc == 0 ? 4 : 0)) & 0x0F);

        return (0 <= c && c <= 9 ? c + '0' : c + 'A' - 10);
    }

    static bool IsHex(char cIn)
    {
        char c = toupper(cIn);

        bool retVal = ('0' <= c && c <= '9') || ('A' <= c && c <= 'F');

        return retVal;
    }

    static uint8_t HexToByte(const char *XX)
    {
        return (uint8_t)((HexCharToByte(XX[0]) << 4) | HexCharToByte(XX[1]));
    }

    static uint8_t HexCharToByte(char cIn)
    {
        char c = toupper(cIn);

        return (uint8_t)('0' <= c && c <= '9' ? c - '0' : c - 'A' + 10);
    }

    void ReverseBytesInternal()
    {
        reverse(buf_.begin(), buf_.end());

        // make sure the reversed bytes are left-aligned, regardless of bit size
        int byteCount = GetBitCount() / 8;

        for (int i = 0, j = 16 - byteCount; j < 16; ++i, ++j)
        {
            uint8_t tmp = buf_[i];

            buf_[i] = buf_[j];

            buf_[j] = tmp;
        }
    }
    

private:
    string uuid_;
    array<uint8_t, 16> buf_;
    bool reversed_ = false;
};



