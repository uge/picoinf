#include "Log.h"
#include "Shell.h"
#include "TimeClass.h"
#include "Utl.h"

#include <cinttypes>
#include <chrono>
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



void Time::SetDateTime(string dt)
{
    timeDeltaUs_ = MakeEpochTimeUsFromDateTime(dt) - PAL.Micros();
}

void Time::SetTime(uint8_t hour, uint8_t minute, uint8_t second, uint32_t us)
{
    // take the string that represents the current time and modify it
    char *dateTimeBuffer = (char *)GetDateTime();

    // 2206-11-01 18:30:30.000002
    // 00000000001111111111222222
    // 01234567890123456789012345

    // have to restore the null-overwritten characters after each operation
    FormatStrC(&dateTimeBuffer[11], 3, "%02u", hour);   dateTimeBuffer[11 + 2] = ':';
    FormatStrC(&dateTimeBuffer[14], 3, "%02u", minute); dateTimeBuffer[14 + 2] = ':';
    FormatStrC(&dateTimeBuffer[17], 3, "%02u", second); dateTimeBuffer[17 + 2] = '.';
    FormatStrC(&dateTimeBuffer[20], 7, "%06u", us);

    SetDateTime(dateTimeBuffer);
}

const char *Time::GetDateTime()
{
    return MakeDateTimeFromUs(PAL.Micros() + timeDeltaUs_);
}

const char *Time::GetDateTimeFromUs(uint64_t timeUs)
{
    return MakeDateTimeFromUs(timeUs + timeDeltaUs_);
}

const char *Time::GetDateTimeFromMs(uint64_t timeMs)
{
    return GetDateTimeFromUs(timeMs * 1'000);
}


const char *Time::GetTimeShortFromUs(uint64_t timeUs)
{
    // starts as full datetime including microseconds
    // 2025-01-14 16:43:24.259829
    char *timeAt = (char *)GetDateTimeFromUs(timeUs);

    // trim to just time and milliseconds
    timeAt = &timeAt[11];

    return timeAt;
}

const char *Time::GetTimeShortFromMs(uint64_t timeMs)
{
    char *timeAt = (char *)GetTimeShortFromUs(timeMs * 1'000);

    // trim off microseconds
    timeAt[12] = '\0';

    return timeAt;
}



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


// purposefully does not do operations which may allocate memory,
// thus this function is safe to call from ISRs
const char *Time::MakeTimeFromUs(uint64_t timeUs)
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


const char *Time::MakeTimeFromMs(uint64_t timeMs)
{
    return MakeTimeFromUs(timeMs * 1000);
}


// return the time without hours, with ms-resolution
string Time::MakeTimeShortFromMs(uint64_t timeMs)
{
    string ts = string{MakeTimeFromUs(timeMs  * 1'000)};

    auto pos = ts.find(':');

    if (pos != string::npos)
    {
        ts = ts.substr((uint8_t)pos + 1, 9);
    }

    return ts;
}


uint64_t Time::MakeEpochTimeUsFromDateTime(string dt)
{
    // Date parsing: "yyyy-mm-dd"
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int us = 0;

    if (sscanf(dt.c_str(), "%4d-%2d-%2d %2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second) != 6)
    {
        return 0;
    }

    // sub-second portion optional
    size_t pos = dt.find('.');
    if (pos != string::npos) {
        string subSecPart = dt.substr(pos);

        if (subSecPart.size() == 1 + 3) // account for the dot
        {
            // assume milliseconds
            int ms = 0;
            if (sscanf(subSecPart.c_str(), ".%3d", &ms) != 1)
            {
                return 0;
            }
            us = ms * 1'000;
        }
        else
        {
            // assume microseconds
            if (sscanf(subSecPart.c_str(), ".%6d", &us) != 1)
            {
                return 0;
            }
        }
    }

    std::tm tm = {};
    tm.tm_year = year - 1900;  // tm_year is years since 1900
    tm.tm_mon = month - 1;     // tm_mon is 0-based (0 = January)
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;

    // Convert tm struct to time_t (seconds since epoch)
    std::time_t secsSinceEpoch = std::mktime(&tm);
    if (secsSinceEpoch == -1)
    {
        return 0;
    }

    // Convert seconds since epoch to microseconds
    uint64_t usSinceEpoch = ((uint64_t)secsSinceEpoch * 1'000'000) + (uint64_t)us;

    return usSinceEpoch;
}




















void Time::SetupShell()
{
    Shell::AddCommand("time.set.dt", [](vector<string> argList){
        SetDateTime(argList[0]);
    }, { .argCount = 1, .help = "wall clock set datetime"});

    Shell::AddCommand("time.set.t", [](vector<string> argList){
        uint8_t  hour = (uint8_t)atoi(argList[0].c_str());
        uint8_t  min  = (uint8_t)atoi(argList[1].c_str());
        uint8_t  sec  = (uint8_t)atoi(argList[2].c_str());
        uint32_t us   = (uint32_t)atoi(argList[3].c_str());

        Log("Time Before: ", GetDateTime());
        SetTime(hour, min, sec, us);
        Log("Time After : ", GetDateTime());
    }, { .argCount = 4, .help = "wall clock set time <hour> <min> <sec> <us>"});

    Shell::AddCommand("time.set.delta", [](vector<string> argList){
        timeDeltaUs_ = stoull(argList[0].c_str());
    }, { .argCount = 1, .help = "wall clock set time delta us"});

    Shell::AddCommand("time.get", [](vector<string> argList){
        Log(GetDateTime());
    }, { .argCount = 0, .help = "wall clock get datetime"});
}