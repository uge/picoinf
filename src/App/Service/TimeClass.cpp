#include "Log.h"
#include "Shell.h"
#include "TimeClass.h"
#include "Utl.h"

#include <cinttypes>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <string>
using namespace std;

#include "StrictMode.h"


[[maybe_unused]]
static void CompareClocks()
{
    // these all return the same thing

    uint64_t timeNowUs = PAL.Micros();
    uint64_t timeSystemClock = (uint64_t)chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now().time_since_epoch()).count();
    uint64_t timeSteadyClock = (uint64_t)chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now().time_since_epoch()).count();

    Log("palTimeUs : ", timeNowUs);
    Log("systemTime: ", timeSystemClock);
    Log("steadyTime: ", timeSteadyClock);
}





static void Overlay(char *dtBuf, uint8_t hour, uint8_t minute, uint8_t second, uint32_t us)
{
    // 2206-11-01 18:30:30.000002
    // 00000000001111111111222222
    // 01234567890123456789012345

    // have to restore the null-overwritten characters after each operation
    FormatStrC(&dtBuf[11], 3, "%02u", hour);   dtBuf[11 + 2] = ':';
    FormatStrC(&dtBuf[14], 3, "%02u", minute); dtBuf[14 + 2] = ':';
    FormatStrC(&dtBuf[17], 3, "%02u", second); dtBuf[17 + 2] = '.';
    FormatStrC(&dtBuf[20], 7, "%06u", us);
}






/////////////////////////////////////////////////////////////////
// Notional time
/////////////////////////////////////////////////////////////////


int64_t Time::SetNotionalDateTimeUs(uint64_t notionalDateTimeUs, uint64_t systemTimeUs)
{
    systemTimeUsAtChange_ = systemTimeUs;

    uint64_t timeDeltaUsBefore = timeDeltaUs_;

    timeDeltaUs_ = notionalDateTimeUs - systemTimeUsAtChange_;

    return (int64_t)(timeDeltaUs_ - timeDeltaUsBefore);
}

int64_t Time::SetNotionalDateTime(string dt)
{
    return SetNotionalDateTimeUs(MakeUsFromDateTime(dt));
}

int64_t Time::SetNotionalTime(uint8_t hour, uint8_t minute, uint8_t second, uint32_t us)
{
    // take the string that represents the current time and modify it
    char *dateTimeBuffer = (char *)GetNotionalDateTime();

    Overlay(dateTimeBuffer, hour, minute, second, us);

    return SetNotionalDateTime(dateTimeBuffer);
}

const char *Time::GetNotionalDateTime()
{
    return MakeDateTimeFromUs(PAL.Micros() + timeDeltaUs_);
}

uint64_t Time::GetNotionalTimeDeltaUs()
{
    return timeDeltaUs_;
}



/////////////////////////////////////////////////////////////////
// Relationship between Notional time and System time
/////////////////////////////////////////////////////////////////

uint64_t Time::GetSystemUsAtLastTimeChange()
{
    return systemTimeUsAtChange_;
}


const char *Time::GetNotionalDateTimeFromSystemUs(uint64_t timeUs)
{
    return MakeDateTimeFromUs(timeUs + timeDeltaUs_);
}



const char *Time::GetNotionalTimeFromSystemUs(uint64_t timeUs)
{
    // starts as full datetime including microseconds
    // 2025-01-14 16:43:24.259829
    char *timeAt = (char *)GetNotionalDateTimeFromSystemUs(timeUs);

    // trim to just time and milliseconds
    timeAt = &timeAt[11];

    return timeAt;
}



/////////////////////////////////////////////////////////////////
// Utilities
/////////////////////////////////////////////////////////////////


const char *Time::MakeDateTimeFromUs(uint64_t timeUs)
{
    // Static buffer to hold the datetime string (enough space for full datetime and microseconds)
    // 2206-11-01 18:30:30.000002   (26 char)
    static char buffer[50];

    // Convert microseconds to time_point
    std::chrono::microseconds microseconds(timeUs);
    std::chrono::system_clock::time_point tp(microseconds);

    // Convert time_point to std::time_t (for seconds)
    std::time_t time = std::chrono::system_clock::to_time_t(tp);

    // Convert std::time_t to std::tm (local time).
    // local time is the same as gmtime.
    std::tm tm = *std::localtime(&time);

    // Extract microseconds (remainder after seconds)
    int64_t usPart = microseconds.count() % 1000000;

    // Print the formatted datetime into the buffer with microseconds, supporting year beyond 4 digits
    std::snprintf(buffer,
                sizeof(buffer),
                "%d-%02d-%02d %02d:%02d:%02d.%06lld",
                tm.tm_year + 1900,
                tm.tm_mon + 1,
                tm.tm_mday,
                tm.tm_hour,
                tm.tm_min,
                tm.tm_sec,
                usPart);

    return buffer;
}


const char *Time::MakeDateTimeFromMs(uint64_t timeMs)
{
    return MakeDateTimeFromUs(timeMs * 1'000);
}

const char *Time::MakeDateTime(uint8_t hour, uint8_t minute, uint8_t second, uint32_t us)
{
    static char retVal[] = "1970-01-01 00:00:00.000000";

    Overlay(retVal, hour, minute, second, us);

    return retVal;
}



const char *Time::MakeTimeFromUs(uint64_t timeUs, bool replaceDateWithSpaces)
{
    char *timeAt = (char *)MakeDateTimeFromUs(timeUs);

    if (replaceDateWithSpaces == false)
    {
        // start from hour
        timeAt = &timeAt[11];
    }
    else
    {
        // blank out
        memset(timeAt, ' ', 11);
    }

    return timeAt;
}

// return the time without hours, with ms-resolution
const char *Time::MakeTimeMMSSmmmFromUs(uint64_t timeMs)
{
    char *timeAt = (char *)MakeTimeFromUs(timeMs);

    // start from minute
    timeAt = &timeAt[3];

    // trim off microseconds
    timeAt[9] = '\0';

    return timeAt;
}


Time::TimePoint Time::ParseDateTime(const string &dt)
{
    TimePoint retVal;

    if (sscanf(dt.c_str(),
               "%4" SCNu16 "-%2" SCNu8 "-%2" SCNu8 " %2" SCNu8 ":%2" SCNu8 ":%2" SCNu8 "",
               &retVal.year,
               &retVal.month,
               &retVal.day,
               &retVal.hour,
               &retVal.minute,
               &retVal.second) != 6)
    {
        return {};
    }

    // sub-second portion optional
    size_t pos = dt.find('.');
    if (pos != string::npos) {
        string subSecPart = dt.substr(pos);

        if (subSecPart.size() == 1 + 3) // account for the dot
        {
            // assume milliseconds
            uint16_t ms = 0;
            if (sscanf(subSecPart.c_str(), ".%3" SCNu16 "", &ms) != 1)
            {
                return {};
            }
            retVal.us = ms * 1'000;
        }
        else
        {
            // assume microseconds
            if (sscanf(subSecPart.c_str(), ".%6" SCNu32 "", &retVal.us) != 1)
            {
                return {};
            }
        }
    }

    return retVal;
}


uint64_t Time::MakeUsFromDateTime(const string &dt)
{
    TimePoint tp = ParseDateTime(dt);

    std::tm tm = {};
    tm.tm_year = tp.year - 1900;  // tm_year is years since 1900
    tm.tm_mon  = tp.month - 1;    // tm_mon is 0-based (0 = January)
    tm.tm_mday = tp.day;
    tm.tm_hour = tp.hour;
    tm.tm_min  = tp.minute;
    tm.tm_sec  = tp.second;

    // Convert tm struct to time_t (seconds since epoch)
    std::time_t secsSinceEpoch = std::mktime(&tm);
    if (secsSinceEpoch == -1)
    {
        return 0;
    }

    // Convert seconds since epoch to microseconds
    uint64_t usSinceEpoch = ((uint64_t)secsSinceEpoch * 1'000'000) + (uint64_t)tp.us;

    return usSinceEpoch;
}






// purposefully does not do operations which may allocate memory,
// thus this function is safe to call from ISRs
const char *Time::MakeDurationFromUs(uint64_t timeUs)
{
    uint64_t time = timeUs;

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
    uint32_t us = (uint32_t)(time % 1'000'000);
    time /= 1'000'000;

    // extract hours
    uint64_t hours = time / (60 * 60);
    time -= (hours * 60 * 60);

    // extract minutes
    uint8_t mins = (uint8_t)(time / 60);
    time -= mins * 60;

    // extract seconds
    uint8_t secs = (uint8_t)time;

    // combine
    snprintf(buf, BUF_SIZE, "%lld:%02u:%02u.%06lu", hours, mins, secs, us);

    return buf;
}


const char *Time::MakeDurationFromMs(uint64_t timeMs)
{
    return MakeDurationFromUs(timeMs * 1000);
}


















void Time::SetupShell()
{
    Shell::AddCommand("time.set.dt", [](vector<string> argList){
        Log("Time Before: ", GetNotionalDateTime());
        int64_t offsetUs = SetNotionalDateTime(argList[0]);
        Log("Time After : ", GetNotionalDateTime());
        if (offsetUs < 0)
        {
            Log("DateTime moved backward by ", MakeDurationFromUs((uint64_t)-offsetUs));
        }
        else
        {
            Log("DateTime moved forward by ", MakeDurationFromUs((uint64_t)offsetUs));
        }
    }, { .argCount = 1, .help = "wall clock set datetime"});

    Shell::AddCommand("time.set.t", [](vector<string> argList){
        uint8_t  hour = (uint8_t)atoi(argList[0].c_str());
        uint8_t  min  = (uint8_t)atoi(argList[1].c_str());
        uint8_t  sec  = (uint8_t)atoi(argList[2].c_str());
        uint32_t us   = (uint32_t)atoi(argList[3].c_str());

        Log("Time Before: ", GetNotionalDateTime());
        int64_t offsetUs = SetNotionalTime(hour, min, sec, us);
        Log("Time After : ", GetNotionalDateTime());
        if (offsetUs < 0)
        {
            Log("DateTime moved backward by ", MakeDurationFromUs((uint64_t)-offsetUs));
        }
        else
        {
            Log("DateTime moved forward by ", MakeDurationFromUs((uint64_t)offsetUs));
        }
    }, { .argCount = 4, .help = "wall clock set time <hour> <min> <sec> <us>"});

    Shell::AddCommand("time.set.delta", [](vector<string> argList){
        timeDeltaUs_ = stoull(argList[0].c_str());
    }, { .argCount = 1, .help = "wall clock set time delta us"});

    Shell::AddCommand("time.get", [](vector<string> argList){
        Log(GetNotionalDateTime());
    }, { .argCount = 0, .help = "wall clock get datetime"});

    Shell::AddCommand("time.get.delta", [](vector<string> argList){
        Log(Commas(GetNotionalTimeDeltaUs()));
    }, { .argCount = 0, .help = "wall clock get time delta us"});

    Shell::AddCommand("time.make.from.us", [](vector<string> argList){
        Log(MakeDateTimeFromUs((uint64_t)stoull(argList[0].c_str())));
    }, { .argCount = 1, .help = ""});
}