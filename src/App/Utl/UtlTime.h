#pragma once

#include <cstdint>
#include <string>


// purposefully does not do operations which may allocate memory,
// thus this function is safe to call from ISRs
extern char *TimestampFromUs(uint64_t usTime);

extern std::string TS();

// return the time without hours, with ms-resolution
extern std::string MsToMinutesStr(uint64_t ms);