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
    static const char *GetNotionalDateTimeFromSystemMs(uint64_t timeMs);
    static const char *GetNotionalTimeShortFromSystemUs(uint64_t timeUs); // just time and microseconds
    static const char *GetNotionalTimeShortFromSystemMs(uint64_t timeMs); // just time and milliseconds


    /////////////////////////////////////////////////////////////////
    // Utilities
    /////////////////////////////////////////////////////////////////

    // epoch time to string conversion
    static const char *MakeDateTimeFromUs(uint64_t timeUs);
    static const char *MakeDateTimeFromMs(uint64_t timeMs);
    static const char *MakeDateTime(uint8_t hour, uint8_t minute, uint8_t second, uint32_t us);

    static const char *MakeTimeFromUs(uint64_t timeUs);
    static const char *MakeTimeFromMs(uint64_t timeMs);

    static std::string MakeTimeShortFromMs(uint64_t timeMs);


    // string to epoch time conversion
    static uint64_t MakeUsFromDateTime(std::string dt);


public:

    static void SetupShell();


private:

    static inline uint64_t timeDeltaUs_ = 0;
    static inline uint64_t systemTimeAtChange_ = 0;
};

