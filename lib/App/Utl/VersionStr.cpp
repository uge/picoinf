#include "VersionStr.h"
#include "Utl.h"

#include <string>
#include <vector>
using namespace std;

#define MY_VERSION_STRINGIFY(s) #s
#define MY_VERSION_TOSTRING(s) MY_VERSION_STRINGIFY(s)

string Version::GetVersion()
{
    // YYYY-MM-DD_HH:MM:SS
    return MY_VERSION_TOSTRING(APP_BUILD_VERSION);
}

string Version::GetVersionShort()
{
    return Split(GetVersion(), "_")[0];
}

uint64_t Version::GetVersionAsInt()
{
    vector<string> partList     = Split(GetVersion(), "_");
    vector<string> datePartList = Split(partList[0], "-");
    vector<string> timePartList = Split(partList[1], ":");

    uint64_t version = 0;

    version += atoi(datePartList[0].c_str());

    version *= 100;
    version += atoi(datePartList[1].c_str());

    version *= 100;
    version += atoi(datePartList[2].c_str());

    version *= 100;
    version += atoi(timePartList[0].c_str());

    version *= 100;
    version += atoi(timePartList[1].c_str());

    version *= 100;
    version += atoi(timePartList[2].c_str());

    return version;
}

#undef MY_VERSION_STRINGIFY
#undef MY_VERSION_TOSTRING
