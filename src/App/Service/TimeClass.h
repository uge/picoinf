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
    static int64_t     SetNotionalUs(uint64_t notionalDateTimeUs, uint64_t systemTimeUs = PAL.Micros());
    static int64_t     SetNotionalDateTime(std::string dt);
    static int64_t     SetNotionalTime(uint8_t hour, uint8_t minute, uint8_t second, uint32_t us);

    static uint64_t    GetNotionalUsAtSystemUs(uint64_t timeUs);
    static uint64_t    GetNotionalUs();
    static const char *GetNotionalDateTime();
    static const char *GetNotionalTime(bool replaceDateWithSpaces = false);

    static uint64_t    GetNotionalTimeDeltaUs();


    /////////////////////////////////////////////////////////////////
    // Relationship between Notional time and System time
    /////////////////////////////////////////////////////////////////

    static uint64_t GetSystemUsAtLastTimeChange();

    // see system epoch in terms of the notional time
    static const char *GetNotionalDateTimeAtSystemUs(uint64_t timeUs);
    static const char *GetNotionalTimeAtSystemUs(uint64_t timeUs);


    /////////////////////////////////////////////////////////////////
    // Utilities
    /////////////////////////////////////////////////////////////////

    // epoch time to string conversion
    static const char *MakeDateTimeFromUs(uint64_t timeUs);
    static const char *MakeDateTimeFromMs(uint64_t timeMs);
    static const char *MakeDateTime(uint8_t hour, uint8_t minute, uint8_t second, uint32_t us);

    static const char *MakeTimeFromUs(uint64_t timeUs, bool replaceDateWithSpaces = false);
    static const char *MakeTimeMMSSmmmFromUs(uint64_t timeUs);
    static const char *MakeTimeMMSSmmmFromMs(uint64_t timeMs);

    // time duration (compared to a reference, possibly negative).
    // this would be MakeDuration... except it is unconditional 2-digit hours.
    static const char *MakeTimeRelativeFromUs(uint64_t timeAtUs, uint64_t timeRefUs = PAL.Micros());


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


    // duration (compared to 0, so always positive)
    static const char *MakeDurationFromUs(uint64_t timeUs);
    static const char *MakeDurationFromMs(uint64_t timeMs);





public:

    static void SetupShell();


private:

    static inline uint64_t timeDeltaUs_ = 0;
    static inline uint64_t systemTimeUsAtChange_ = 0;
};

