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
    static const char *GetDateTimeFromSystemEpochTimeUs(uint64_t timeUs);


    /////////////////////////////////////////////////////////////////
    // Utilities
    /////////////////////////////////////////////////////////////////

    // epoch time to string conversion
    static const char *GetDateTimeFromUs(uint64_t timeUs);
    static const char *GetDateTimeFromMs(uint64_t timeMs);

    static const char *GetTimeFromUs(uint64_t timeUs);
    static const char *GetTimeFromMs(uint64_t timeMs);

    static std::string GetTimeShortFromMs(uint64_t timeMs);


    // string to epoch time conversion
    static uint64_t GetEpochTimeUsFromDateTime(std::string dt);


public:

    static void SetupShell();


private:

    static inline uint64_t timeDeltaUs_ = 0;
};

