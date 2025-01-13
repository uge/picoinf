#include "PAL.h"
#include "UtlTime.h"

using namespace std;

#include "StrictMode.h"


// purposefully does not do operations which may allocate memory,
// thus this function is safe to call from ISRs
char *TimestampFromUs(uint64_t usTime)
{
    uint64_t time = usTime;

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

char *TimestampFromMs(uint64_t msTime)
{
    return TimestampFromUs(msTime * 1000);
}

string TS()
{
    string retVal;

    retVal += "[";
    retVal += TimestampFromUs(PAL.Micros());
    retVal += "] ";

    return retVal;
}

// return the time without hours, with ms-resolution
string MsToMinutesStr(uint64_t ms)
{
    string ts = string{TimestampFromUs(ms  * 1'000)};

    auto pos = ts.find(':');

    if (pos != string::npos)
    {
        ts = ts.substr((uint8_t)pos + 1, 9);
    }

    return ts;
}