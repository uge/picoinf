#pragma once

#include <string>
#include <vector>


// Assumes one line at a time, ascii only, no newlines
// Format of the line:
// $<data>*<CC>
// CC is the 2-digit hex checksum of <data>
class NMEAStringParser
{
public:

    // return only <data> length between $ and *
    // -1 on invalid line
    static int GetDataLength(const std::string &line);

    // empty on invalid line
    static std::string GetLineData(const std::string &line);

    // empty string if invalid
    static std::string GetCalcdChecksum(const std::string &line);

    // everything between $ and *
    static std::string CalculateChecksum(const std::string &data);

    static std::string GetNmeaStringChecksum(const std::string &line);

    static bool IsValid(const std::string &line);

    static std::vector<std::string> GetLineDataPartList(const std::string &line);

    static std::string GetMessageTypeFull(const std::string &line);

private:

    static bool MeetsPreconditions(const std::string &line);
};
