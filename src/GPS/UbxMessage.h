#pragma once

#include "UART.h"
#include "Utl.h"

#include <cstdint>


/////////////////////////////////////////////////////////////////////
// UbxMessageParser
/////////////////////////////////////////////////////////////////////

class UbxMessageParser
{
    static const uint8_t DEFAULT_MAX_PAYLOAD_LIMIT = 100;

    vector<uint8_t> buf_;
    uint16_t stopAtSize_ = 0;
    
    enum class State : uint8_t
    {
        LOOKING_FOR_HEADER = 0,
        LOOKING_FOR_CLASS,
        LOOKING_FOR_ID,
        LOOKING_FOR_LEN,
        LOOKING_FOR_CHECKSUM,
        FOUND,
        ERR,
    };

    State state_ = State::LOOKING_FOR_HEADER;


public:
    void Reset()
    {
        buf_.clear();
        stopAtSize_ = 0;

        state_ = State::LOOKING_FOR_HEADER;
    }
    
    void AddByte(uint8_t b)
    {
        if (state_ != State::FOUND)
        {
            AddByteInternal(b);
        }
        else
        {
            // make sure that adding bytes beyond the found message
            // results in an error.  At that point, more bytes have
            // been put in than actually constitute the message.
            state_ = State::ERR;
        }
    }

    bool ErrorEncountered() const
    {
        return state_ == State::ERR;
    }

    bool MessageFound() const
    {
        return state_ == State::FOUND;
    }

    uint16_t GetClassId()
    {
        uint16_t retVal = 0;

        if (MessageFound())
        {
            retVal = buf_[2] << 8 | buf_[3];
        }

        return retVal;
    }

    const vector<uint8_t> &GetData() const
    {
        return buf_;
    }
        

private:

    void AddByteInternal(uint8_t b)
    {
        if (state_ == State::LOOKING_FOR_HEADER)
        {
            // Log("LOOKING_FOR_HEADER");
            
            // store this byte
            buf_.push_back(b);

            // check if we received a byte previously
            // need 2 to match header
            if (buf_.size() == 1)
            {
                // nope, leave that byte stored and carry on
            }
            else
            {
                // yup, check if this is a valid header now
                if (buf_[0] == 0xB5 && buf_[1] == 0x62)
                {
                    // yup, move to next state
                    state_ = State::LOOKING_FOR_CLASS;
                }
                else
                {
                    // Log("    discarding ", ToHex(buf_[0]));
                    
                    // nope, maybe the last byte that came in is the start,
                    // shift it to the start and carry on
                    buf_.erase(buf_.begin());
                }
            }
        }
        else if (state_ == State::LOOKING_FOR_CLASS)
        {
            // Log("LOOKING_FOR_CLASS");

            buf_.push_back(b);
            
            // Log("    class: ", ToHex(b));

            state_ = State::LOOKING_FOR_ID;
        }
        else if (state_ == State::LOOKING_FOR_ID)
        {
            // Log("LOOKING_FOR_ID");

            buf_.push_back(b);

            // Log("       id: ", ToHex(b));

            state_ = State::LOOKING_FOR_LEN;
        }
        else if (state_ == State::LOOKING_FOR_LEN)
        {
            // Log("LOOKING_FOR_LEN");
            
            buf_.push_back(b);

            if (buf_.size() == 6)
            {
                // we have the full size
                // we want to go from little endian wire format to host endian
                // start by making a network-byte-order 16-bit int, aka big endian
                uint16_t lenBigEndian;
                char *p = (char *)&lenBigEndian;
                p[0] = buf_[5];
                p[1] = buf_[4];

                uint16_t len = ntohs(lenBigEndian);

                // Log("    lenBigEndian: ", lenBigEndian);
                // Log("    len         : ", len);

                // length refers to the payload only
                // total message size is:
                //   [2 sync bytes + 1 class byte + 1 id byte] + payload + [2 crc bytes]
                // the current buffer has the 2 sync bytes and class and id bytes
                stopAtSize_ = buf_.size() + len + 2;

                state_ = State::LOOKING_FOR_CHECKSUM;
            }
            else
            {
                // Nothing to do, keep collecting
            }
        }
        else if (state_ == State::LOOKING_FOR_CHECKSUM)
        {
            // Log("LOOKING_FOR_CHECKSUM");
            
            buf_.push_back(b);

            if (buf_.size() == stopAtSize_)
            {
                // Time to calculate and compare the checksum
                uint8_t ckA = 0;
                uint8_t ckB = 0;

                // checksum class, id, length, payload
                uint8_t idxChecksumStart = buf_.size() - 2;
                for (uint8_t i = 2; i < idxChecksumStart; ++i)
                {
                    uint8_t b = buf_[i];
                    
                    ckA += b;
                    ckB += ckA;
                }

                // extract the message checksum
                uint8_t msgCkA = buf_[idxChecksumStart + 0];
                uint8_t msgCkB = buf_[idxChecksumStart + 1];

                // Log("ckA, ckB: ", ckA, " ", ckB);
                // Log("msgCkA, msgCkB: ", msgCkA, " ", msgCkB);

                // LogBlob(buf_.data(), buf_.size());

                if (ckA == msgCkA && ckB == msgCkB)
                {
                    // success
                    state_ = State::FOUND;
                }
                else
                {
                    // Log("Checksum failed");
                    state_ = State::ERR;
                }
            }
            else
            {
                // Nothing to do, just pile on bytes until reaching the checksum
            }
        }
        else
        {
            // nothing to do, done or error state
        }
    }
};




/////////////////////////////////////////////////////////////////////
// UbxMessage Reader / Maker Base
/////////////////////////////////////////////////////////////////////

template <typename T>
class UbxMsg
{
public:
    static const uint8_t NON_PAYLOAD_BYTES = 8;
    static const uint8_t TOTAL_BYTES = NON_PAYLOAD_BYTES + T::PAYLOAD_BYTES;
    static const uint8_t IDX_CLASS = 2;
    static const uint8_t IDX_ID = 3;
    static const uint8_t IDX_SIZE = 4;
    static const uint8_t IDX_PAYLOAD = 6;

    UbxMsg()
    : byteList_(TOTAL_BYTES, 0)
    {
        Reset();
    }

    UbxMsg(const UbxMessageParser &p)
    : UbxMsg()
    {
        SetMsgData(p);
    }

    // Return class and id in a single 16-bit int
    // which will print in the order of class and id
    uint16_t GetClassId()
    {
        uint16_t retVal;

        retVal = T::CLASS << 8 | T::ID;

        return retVal;
    }

    bool SetMsgData(const UbxMessageParser &p)
    {
        return SetMsgData(p.GetData());
    }

    bool SetMsgData(const uint8_t *buf, uint8_t bufSize)
    {
        vector<uint8_t> val;

        for (int i = 0; i < (int)bufSize; ++i)
        {
            val.push_back(buf[i]);
        }

        return SetMsgData(val);
    }

    bool SetMsgData(const vector<uint8_t> &byteList)
    {
        Reset();

        if (MessageValid(byteList))
        {
            byteList_ = byteList;
            beenSet_ = true;
        }

        return Ok();
    }

    const vector<uint8_t> &GetData()
    {
        return byteList_;
    }

    bool Ok()
    {
        return beenSet_;
    }

    bool Finish()
    {
        bool retVal = false;

        // Set up sync bytes
        byteList_[0] = 0xB5;
        byteList_[1] = 0x62;

        // Set up class / id
        byteList_[IDX_CLASS] = T::CLASS;
        byteList_[IDX_ID]    = T::ID;

        // Set up size
        *(uint16_t *)&byteList_.data()[IDX_SIZE] = htols(T::PAYLOAD_BYTES);

        // calculate and compare the checksum
        uint8_t ckA = 0;
        uint8_t ckB = 0;

        // checksum class, id, length, payload
        uint8_t idxChecksumStart = byteList_.size() - 2;
        for (uint8_t i = 2; i < idxChecksumStart; ++i)
        {
            uint8_t b = byteList_[i];
            
            ckA += b;
            ckB += ckA;
        }

        // set checksum values
        byteList_[byteList_.size() - 2] = ckA;
        byteList_[byteList_.size() - 1] = ckB;

        // confirm with parse
        retVal = MessageValid(byteList_);

        // Consider this the criteria for being good data set
        beenSet_ = retVal;

        if (retVal == false)
        {
            UartTarget(UART::UART_0);
            Log("ERR: Bad finish - ", ToHex(GetClassId()));
        }

        return retVal;
    }

    void Reset()
    {
        std::fill(byteList_.begin(), byteList_.end(), 0);

        beenSet_ = false;
    }

    // Fast ID of a valid message being this particular type
    static bool MessageValid(const UbxMessageParser &p)
    {
        const vector<uint8_t> &byteList = p.GetData();

        return p.MessageFound() &&
               byteList[IDX_CLASS] == T::CLASS && 
               byteList[IDX_ID]    == T::ID;
    }

    // Slow validation
    static bool MessageValid(const vector<uint8_t> &byteList)
    {
        bool retVal = false;

        if (byteList.size() == TOTAL_BYTES)
        {
            if (byteList[2] == T::CLASS && byteList[3] == T::ID)
            {
                UbxMessageParser p;

                for (int i = 0; i < (int)byteList.size(); ++i)
                {
                    p.AddByte(byteList[i]);
                }

                retVal = p.MessageFound();
            }
        }

        return retVal;
    }


protected:

    uint8_t *GetMsgData()
    {
        return byteList_.data();
    }

    uint8_t *GetPayloadData()
    {
        return &byteList_.data()[IDX_PAYLOAD];
    }


    //
    // 8-bit
    //
    uint8_t GetU1AtIdx(uint8_t idx)
    {
        return GetPayloadData()[idx];
    }

    void SetU1AtIdx(uint8_t idx, uint8_t val)
    {
        GetPayloadData()[idx] = val;
    }

    int8_t GetI1AtIdx(uint8_t idx)
    {
        return GetU1AtIdx(idx);
    }

    void SetI1AtIdx(uint8_t idx, int16_t val)
    {
        SetU1AtIdx(idx, val);
    }


    //
    // 16-bit
    //
    uint16_t GetU2AtIdx(uint8_t idx)
    {
        return ltohs(*(uint16_t *)&GetPayloadData()[idx]);
    }

    void SetU2AtIdx(uint8_t idx, uint16_t val)
    {
        *(uint16_t *)&GetPayloadData()[idx] = htols(val);
    }

    int16_t GetI2AtIdx(uint8_t idx)
    {
        return GetU2AtIdx(idx);
    }

    void SetI2AtIdx(uint8_t idx, int16_t val)
    {
        SetU2AtIdx(idx, val);
    }


    //
    // 32-bit
    //
    uint32_t GetU4AtIdx(uint8_t idx)
    {
        return ltohl(*(uint32_t *)&GetPayloadData()[idx]);
    }

    void SetU4AtIdx(uint8_t idx, uint32_t val)
    {
        *(uint32_t *)&GetPayloadData()[idx] = htoll(val);
    }

    int32_t GetI4AtIdx(uint8_t idx)
    {
        return GetU4AtIdx(idx);
    }

    void SetI4AtIdx(uint8_t idx, int32_t val)
    {
        SetU4AtIdx(idx, val);
    }


    //
    // 8-bit bitmask convenience
    //
    uint8_t GetBitInBitmask8(uint8_t idx, uint8_t pos)
    {
        uint8_t bitMask = GetU1AtIdx(idx);

        return (bitMask & (1 << pos)) ? 1 : 0;
    }

    void SetBitInBitmask8(uint8_t idx, uint8_t pos, uint8_t val)
    {
        uint8_t bitMask = GetU1AtIdx(idx);

        bitMask = (~(1 << val) & bitMask) | // clear the bit
                  ((val & 0x01) << pos);    // apply the new bit value (0 or 1)

        SetU1AtIdx(idx, bitMask);
    }

    //
    // 16-bit bitmask convenience
    //
    uint8_t GetBitInBitmask16(uint8_t idx, uint8_t pos)
    {
        uint16_t bitMask = GetU2AtIdx(idx);

        return (bitMask & (1 << pos)) ? 1 : 0;
    }

    void SetBitInBitmask16(uint8_t idx, uint8_t pos, uint8_t val)
    {
        uint16_t bitMask = GetU2AtIdx(idx);

        bitMask = (~(1 << val) & bitMask) | // clear the bit
                  ((val & 0x01) << pos);    // apply the new bit value (0 or 1)

        SetU2AtIdx(idx, bitMask);
    }

    //
    // 32-bit bitmask convenience
    //
    uint8_t GetBitInBitmask32(uint8_t idx, uint8_t pos)
    {
        uint32_t bitMask = GetU4AtIdx(idx);

        return (bitMask & (1 << pos)) ? 1 : 0;
    }

    void SetBitInBitmask32(uint8_t idx, uint8_t pos, uint8_t val)
    {
        uint32_t bitMask = GetU4AtIdx(idx);

        bitMask = (~((uint32_t)1 << val) & bitMask) | // clear the bit
                  ((uint32_t)(val & 0x01) << pos);    // apply the new bit value (0 or 1)

        SetU4AtIdx(idx, bitMask);
    }



private:

    vector<uint8_t> byteList_;
    bool beenSet_;
};


/////////////////////////////////////////////////////////////////////
// ACK / NACK
/////////////////////////////////////////////////////////////////////

class UbxMsgAckAck
: public UbxMsg<UbxMsgAckAck>
{
public:
    static const uint8_t CLASS = 0x05;
    static const uint8_t ID    = 0x01;
    static const uint8_t PAYLOAD_BYTES = 2;

    uint8_t GetAckClass()
    {
        return GetPayloadData()[0];
    }

    uint8_t GetAckId()
    {
        return GetPayloadData()[1];
    }

    uint16_t GetAckClassId()
    {
        uint16_t retVal;

        retVal = GetAckClass() << 8 | GetAckId();

        return retVal;
    }
};

class UbxMsgAckNak
: public UbxMsg<UbxMsgAckNak>
{
public:
    static const uint8_t CLASS = 0x05;
    static const uint8_t ID    = 0x00;
    static const uint8_t PAYLOAD_BYTES = 2;

    uint8_t GetNakClass()
    {
        return GetPayloadData()[0];
    }

    uint8_t GetNakId()
    {
        return GetPayloadData()[1];
    }

    uint16_t GetNakClassId()
    {
        uint16_t retVal;

        retVal = GetNakClass() << 8 | GetNakId();

        return retVal;
    }
};


/////////////////////////////////////////////////////////////////////
// CFG-NAV5 - 0x06 0x24 (altitude mode)
/////////////////////////////////////////////////////////////////////

class UbxMsgCfgNav5GetSet
: public UbxMsg<UbxMsgCfgNav5GetSet>
{
public:
    static const uint8_t CLASS = 0x06;
    static const uint8_t ID    = 0x24;
    static const uint8_t PAYLOAD_BYTES = 36;

    uint8_t GetDynModel()
    {
        return GetPayloadData()[2];
    }

    void SetDynModel(uint8_t val)
    {
        GetPayloadData()[2] = val;

        SetBitInBitmask16(0, 0, 1);
    }

    uint8_t GetFixMode()
    {
        return GetPayloadData()[3];
    }

    void Dump()
    {
        Log("UBX CFG-NAV5 GET/SET - ", ToHex(GetClassId()));

        if (Ok())
        {
            Log("  Bitmask: ", ToBin(GetU2AtIdx(0)));

            uint8_t dynModel = GetDynModel();
            string dynModelStr;
            if      (dynModel == 0) { dynModelStr = "Portable"; }
            else if (dynModel == 2) { dynModelStr = "Stationary"; }
            else if (dynModel == 3) { dynModelStr = "Pedestrian"; }
            else if (dynModel == 4) { dynModelStr = "Automotive"; }
            else if (dynModel == 5) { dynModelStr = "Sea"; }
            else if (dynModel == 6) { dynModelStr = "Airborne < 1g Accel"; }
            else if (dynModel == 7) { dynModelStr = "Airborne < 2g Accel"; }
            else if (dynModel == 8) { dynModelStr = "Airborne < 4g Accel"; }
            else                    { dynModelStr = "Unknown"; }
            Log("  DynModel: ", dynModelStr);
        }
        else
        {
            Log("  Bad data");
        }
    }
};


/////////////////////////////////////////////////////////////////////
// CFG-RST - 0x06 0x04
/////////////////////////////////////////////////////////////////////

class UbxMsgCfgRst
: public UbxMsg<UbxMsgCfgRst>
{
public:
    static const uint8_t CLASS = 0x06;
    static const uint8_t ID    = 0x04;
    static const uint8_t PAYLOAD_BYTES = 4;

    void SetResetTypeHotImmediate()
    {
        SetU2AtIdx(0, 0x0000);
        GetPayloadData()[2] = 0x00;
    }

    // this seems to get NAK'd by ATGM336H
    void SetResetTypeWarmImmediate()
    {
        SetU2AtIdx(0, 0x0001);
        GetPayloadData()[2] = 0x00;
    }

    void SetResetTypeColdImmediate()
    {
        SetU2AtIdx(0, 0xFFFF);
        GetPayloadData()[2] = 0x00;
    }

    void Dump()
    {
        Log("UBX CFG-RST - ", ToHex(GetClassId()));

        if (Ok())
        {
            Log("  Bitmask: ", ToBin(GetU2AtIdx(0)));

            if (GetU2AtIdx(0) == 0x0000 && GetPayloadData()[2] == 0x00)
            {
                Log("  Restart type: Hot / Immediate");
            }
            else if (GetU2AtIdx(0) == 0x0001 && GetPayloadData()[2] == 0x00)
            {
                Log("  Restart type: Warm / Immediate");
            }
            else if (GetU2AtIdx(0) == 0xFFFF && GetPayloadData()[2] == 0x00)
            {
                Log("  Restart type: Cold / Immediate");
            }
            else
            {
                Log("  Restart type: Unknown");
            }            
        }
        else
        {
            Log("  Bad data");
        }
    }
};


/////////////////////////////////////////////////////////////////////
// CFG-CFG - 0x06 0x09 (save configuration)
/////////////////////////////////////////////////////////////////////

class UbxMsgCfgCfg
: public UbxMsg<UbxMsgCfgCfg>
{
public:
    static const uint8_t CLASS = 0x06;
    static const uint8_t ID    = 0x09;
    static const uint8_t PAYLOAD_BYTES = 13;

    void SetSaveConfigurationToBatteryBackedRAM()
    {
        uint8_t *pSave = (uint8_t *)&GetPayloadData()[4];

        // Lazy writing to bitfield until I can see if it works
        // (Save every setting in effect now)
        pSave[0] = 0xFF;
        pSave[1] = 0xFF;
        pSave[2] = 0xFF;
        pSave[3] = 0xFF;

        uint8_t *pWhere = (uint8_t *)&GetPayloadData()[12];
        // pWhere[0] = 0x01;   // RAM
        pWhere[0] = 0xFF;   // Everywhere you can
    }

    void SetFactoryReset()
    {
        uint8_t *pClear = (uint8_t *)&GetPayloadData()[0];

        // Lazy writing to bitfield until I can see if it works
        // (Save every setting in effect now)
        pClear[0] = 0xFF;
        pClear[1] = 0xFF;
        pClear[2] = 0xFF;
        pClear[3] = 0xFF;

        uint8_t *pWhere = (uint8_t *)&GetPayloadData()[12];
        pWhere[0] = 0xFF;   // Everything
    }

    void Dump()
    {
        Log("UBX CFG-CFG - ", ToHex(GetClassId()));

        if (Ok())
        {
            uint8_t *pClear = (uint8_t *)&GetPayloadData()[0];
            uint8_t *pSave = (uint8_t *)&GetPayloadData()[4];
            uint8_t *pWhere = (uint8_t *)&GetPayloadData()[12];

            if (pSave[0] == 0xFF && pSave[1] == 0xFF && pSave[2] == 0xFF && pSave[3] == 0xFF && pWhere[0] == 0xFF)
            {
                Log("  Save Configuration");
            }
            else if (pClear[0] == 0xFF && pClear[1] == 0xFF && pClear[2] == 0xFF && pClear[3] == 0xFF && pWhere[0] == 0xFF)
            {
                Log("  Factory Reset");
            }
            else
            {
                Log("  Unknown configuration");
            }
        }
        else
        {
            Log("  Bad data");
        }
    }
};



/////////////////////////////////////////////////////////////////////
// CFG-MSG - 0x06 0x01 (set message types to receive and frequency)
/////////////////////////////////////////////////////////////////////

class UbxMsgCfgMsg
: public UbxMsg<UbxMsgCfgMsg>
{
public:
    static const uint8_t CLASS = 0x06;
    static const uint8_t ID    = 0x01;
    static const uint8_t PAYLOAD_BYTES = 3;

    void SetClassIdRate(uint8_t msgClass, uint8_t msgId, uint8_t rateSecs)
    {
        uint8_t *p = GetPayloadData();

        p[0] = msgClass;
        p[1] = msgId;
        p[2] = rateSecs;
    }

    void Dump()
    {
        Log("UBX CFG-MSG - ", ToHex(GetClassId()));

        if (Ok())
        {
            uint8_t *p = GetPayloadData();

            uint8_t msgClass = p[0];
            uint8_t msgId    = p[1];
            uint8_t rateSecs = p[2];

            Log("  Set NMEA ClassId ",
                ToHex(msgClass, false),
                ToHex(msgId, false),
                " to rate of ", rateSecs, " per sec");
        }
        else
        {
            Log("  Bad data");
        }
    }
};

/////////////////////////////////////////////////////////////////////
// CFG-TP - 0x06 0x07 (set timepulse params - legacy)
/////////////////////////////////////////////////////////////////////

class UbxMsgCfgTpPoll
: public UbxMsg<UbxMsgCfgTpPoll>
{
public:
    static const uint8_t CLASS = 0x06;
    static const uint8_t ID    = 0x07;
    static const uint8_t PAYLOAD_BYTES = 0;

    void Dump()
    {
        Log("UBX CFG-TP Poll - ", ToHex(GetClassId()));

        if (Ok())
        {
            Log("Data good");
        }
        else
        {
            Log("  Bad data");
        }
    }
};


/////////////////////////////////////////////////////////////////////
// CFG-TP5 - 0x06 0x31 (set timepulse params)
/////////////////////////////////////////////////////////////////////

class UbxMsgCfgTp5Poll
: public UbxMsg<UbxMsgCfgTp5Poll>
{
public:
    static const uint8_t CLASS = 0x06;
    static const uint8_t ID    = 0x31;
    static const uint8_t PAYLOAD_BYTES = 0;

    void Dump()
    {
        Log("UBX CFG-TP5 Poll - ", ToHex(GetClassId()));

        if (Ok())
        {
            Log("Data good");
        }
        else
        {
            Log("  Bad data");
        }
    }
};

class UbxMsgCfgTp5
: public UbxMsg<UbxMsgCfgTp5>
{
public:
    static const uint8_t CLASS = 0x06;
    static const uint8_t ID    = 0x31;
    static const uint8_t PAYLOAD_BYTES = 32;

    // As in, don't wait for GPS lock to give me timepulses, we want
    // the benefit of its good clock
    void SetTimepulseAlways()
    {
        uint8_t *p = GetPayloadData();

        // select first timepulse (some modules have more than one)
        p[0] = 0;

        // 2 reserved bytes

        // antCableDelay - antenna delay
        SetI2AtIdx(4, 0);

        // rfGroupDelay - rf group delay
        SetI2AtIdx(6, 0);

        // freqPeriod
        // frequency or period time when not locked (selected by isFreq)
        // we want frequency of 1Hz
        SetU4AtIdx(8, 1);

        // freqPeriodLock
        // frequency or period time when locked (selected by isFreq)
        // we want frequency of 1Hz
        SetU4AtIdx(12, 1);

        // pulseLenRatio
        // pulse length (us) or duty cycle when not locked (selected by isLength)
        // we want standard 100ms = 100'000us
        SetU4AtIdx(16, 100'000);

        // pulseLenRatioLock
        // pulse length (us) or duty cycle when locked (selected by lockedOtherSet)
        SetU4AtIdx(20, 100'000);

        // userConfigDelay
        // user-configurable timepulse delay
        SetU4AtIdx(24, 0);


        // flags
        // bitmap -- not sure I've configured the above correctly...
        uint32_t bitmap = 0;
        
        // Active
        bitmap |= 0b0000'0001;   // set active

        // LockGpsFreq
        bitmap |= 0b0000'0010;   // sync to gps lock start-of-second when acquired

        // lockedOtherSet
        // don't use different configuration sets (locked vs not) for deciding
        // pulse frequency and pulse len
        // both are configured equally above "just in case"
        bitmap |= 0b0000'0000;
        
        // isFreq
        // is freqPeriod/freqPeriodLock a frequency or period?
        bitmap |= 0b0000'1000;  // it's a frequency

        // isLength
        // is pulseLenRatio/pulseLenRatioLock a length or duty cycle?
        bitmap |= 0b0001'0000;  // it's a length

        // alignToTow
        // should timepulse align to top of second when gps locked?
        bitmap |= 0b0010'0000;  // yes

        // polarity
        // should the pulse be rising or falling at the start of pulse?
        bitmap |= 0b0100'0000;  // rising

        // gridUtcGps
        // should the timegrid be utc or gps?
        bitmap |= 0b1000'0000;  // utc

        SetU4AtIdx(28, bitmap);
    }

    void Dump()
    {
        Log("UBX CFG-TP5 - ", ToHex(GetClassId()));

        if (Ok())
        {
            Log("Data good");
        }
        else
        {
            Log("  Bad data");
        }
    }
};



