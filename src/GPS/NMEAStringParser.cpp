#include "NMEAStringParser.h"
#include "Utl.h"

using namespace std;

#include "StrictMode.h"



// return only <data> length between $ and *
// -1 on invalid line
int NMEAStringParser::GetDataLength(const string &line)
{
    int dataLength = -1;

    if (MeetsPreconditions(line))
    {
        // ignore the first char ($)
        // ignore the last 3 chars (checksum)
        dataLength = (int)(line.length() - 4);
    }

    return dataLength;
}

// empty on invalid line
string NMEAStringParser::GetLineData(const string &line)
{
    string retVal;

    if (MeetsPreconditions(line))
    {
        int dataLength = GetDataLength(line);

        retVal = line.substr(1, (size_t)dataLength);
    }

    return retVal;
}

// empty string if invalid
string NMEAStringParser::GetCalcdChecksum(const string &line)
{
    string retVal;

    if (MeetsPreconditions(line))
    {
        // XOR of all the bytes between the $ and the *, written in hexadecimal
        string lineData = GetLineData(line);

        retVal = CalculateChecksum(lineData);
    }

    return retVal;
}

// everything between $ and *
string NMEAStringParser::CalculateChecksum(const string &data)
{
    string retVal;

    uint8_t checksum = 0;
    for (char c : data)
    {
        checksum = checksum ^ c;
    }

    retVal = ToHex(checksum, false);

    return retVal;
}

string NMEAStringParser::GetNmeaStringChecksum(const string &line)
{
    string retVal;

    if (MeetsPreconditions(line))
    {
        retVal = line.substr(1 + (size_t)GetDataLength(line) + 1, 2);
    }

    return retVal;
}

bool NMEAStringParser::IsValid(const string &line)
{
    bool retVal = false;

    if (MeetsPreconditions(line))
    {
        string checksumCalcd = GetCalcdChecksum(line);
        string checksumNmea  = GetNmeaStringChecksum(line);

        retVal = (checksumCalcd == checksumNmea);
    }

    return retVal;
}

vector<string> NMEAStringParser::GetLineDataPartList(const string &line)
{
    vector<string> retVal;

    if (IsValid(line))
    {
        string lineData = GetLineData(line);

        bool trimmed = true;
        bool allowEmpty = true;
        retVal = Split(lineData, ",", trimmed, allowEmpty);
    }

    return retVal;
}

string NMEAStringParser::GetMessageTypeFull(const string &line)
{
    string retVal;

    if (IsValid(line))
    {
        auto linePartList = GetLineDataPartList(line);

        if (linePartList.size())
        {
            retVal = linePartList[0];
        }
    }

    return retVal;
}

bool NMEAStringParser::MeetsPreconditions(const string &line)
{
    bool retVal = false;

    if (line.length() >= 4)
    {
        size_t starIdx = line.length() - 3;

        if (line[0] == '$' && line[starIdx] == '*')
        {
            retVal = true;
        }
    }

    return retVal;
}
