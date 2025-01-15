#pragma once

#include <cinttypes>
#include <string>

// https://www.unixtimestamp.com/


class Time
{
public:

    /////////////////////////////////////////////////////////////////
    // Notional time
    /////////////////////////////////////////////////////////////////

    // set and access the notional time.
    // setters return offsetUs = (new delta - old delta).
    // this means:
    // - if old time < new time, you get a positive number.
    // - if old time > new time, you get a negative number.
    // this can be handy for knowing if your locally-running clock
    // is falling behind (positive) or running fast (negative).
    static int64_t     SetNotionalDateTimeUs(uint64_t us);
    static int64_t     SetNotionalDateTime(std::string dt);
    static int64_t     SetNotionalTime(uint8_t hour, uint8_t minute, uint8_t second, uint32_t us);
    static const char *GetNotionalDateTime();
    static uint64_t    GetNotionalTimeDeltaUs();


    /////////////////////////////////////////////////////////////////
    // Relationship between Notional time and System time
    /////////////////////////////////////////////////////////////////

    static uint64_t GetSystemUsAtLastTimeChange();

    // see system epoch in terms of the notional time
    static const char *GetNotionalDateTimeFromSystemUs(uint64_t timeUs);
    static const char *GetNotionalTimeFromSystemUs(uint64_t timeUs);


    /////////////////////////////////////////////////////////////////
    // Utilities
    /////////////////////////////////////////////////////////////////

    // epoch time to string conversion
    static const char *MakeDateTimeFromUs(uint64_t timeUs);
    static const char *MakeDateTimeFromMs(uint64_t timeMs);
    static const char *MakeDateTime(uint8_t hour, uint8_t minute, uint8_t second, uint32_t us);

    static const char *MakeTimeFromUs(uint64_t timeUs);
    static const char *MakeTimeMMSSmmmFromUs(uint64_t timeUs);


    // string parsing
    struct TimePoint
    {
        uint16_t year   = 0;
        uint8_t  month  = 0;
        uint8_t  day    = 0;
        uint8_t  hour   = 0;
        uint8_t  minute = 0;
        uint8_t  second = 0;
        uint32_t us     = 0;
    };
    static TimePoint ParseDateTime(const std::string &dt);


    // string to epoch time conversion
    static uint64_t MakeUsFromDateTime(const std::string &dt);


    // duration
    static const char *MakeDurationFromUs(uint64_t timeUs);
    static const char *MakeDurationFromMs(uint64_t timeMs);




public:

    static void SetupShell();


private:

    static inline uint64_t timeDeltaUs_ = 0;
    static inline uint64_t systemTimeAtChange_ = 0;
};

