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

    // set and access the nominal time
    static void SetDateTime(std::string dt);
    static const char *GetDateTime();

    // see system epoch in terms of the nominal time
    static const char *GetDateTimeFromUs(uint64_t timeUs);
    static const char *GetDateTimeFromMs(uint64_t timeMs);

    // trim to just time and microseconds
    static const char *GetTimeShortFromUs(uint64_t timeUs);

    // trim to just time and milliseconds
    static const char *GetTimeShortFromMs(uint64_t timeMs);


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

