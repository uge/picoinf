#pragma once

#include <cinttypes>
#include <string>

// https://www.unixtimestamp.com/


class Time
{
public:

    /////////////////////////////////////////////////////////////////
    // Nominal time
    /////////////////////////////////////////////////////////////////

    // set and access the nominal time.
    // setters return offsetUs = (new delta - old delta).
    // this means:
    // - if old time < new time, you get a positive number.
    // - if old time > new time, you get a negative number.
    // this can be handy for knowing if your locally-running clock
    // is falling behind (positive) or running fast (negative).
    static int64_t SetDateTime(std::string dt);
    static int64_t SetTime(uint8_t hour, uint8_t minute, uint8_t second, uint32_t us);
    static const char *GetDateTime();
    static uint64_t GetTimeDeltaUs();

    // see system epoch in terms of the nominal time
    static const char *GetDateTimeFromUs(uint64_t timeUs);
    static const char *GetDateTimeFromMs(uint64_t timeMs);
    static const char *GetTimeShortFromUs(uint64_t timeUs); // just time and microseconds
    static const char *GetTimeShortFromMs(uint64_t timeMs); // just time and milliseconds


    /////////////////////////////////////////////////////////////////
    // Utilities
    /////////////////////////////////////////////////////////////////

    // epoch time to string conversion
    static const char *MakeDateTimeFromUs(uint64_t timeUs);
    static const char *MakeDateTimeFromMs(uint64_t timeMs);

    static const char *MakeTimeFromUs(uint64_t timeUs);
    static const char *MakeTimeFromMs(uint64_t timeMs);

    static std::string MakeTimeShortFromMs(uint64_t timeMs);


    // string to epoch time conversion
    static uint64_t MakeEpochTimeUsFromDateTime(std::string dt);


public:

    static void SetupShell();


private:

    static inline uint64_t timeDeltaUs_ = 0;
};

