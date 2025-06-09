#include "Shell.h"
#include "Utl.h"
#include "VersionStr.h"

#include <string>
#include <vector>
using namespace std;

#include "StrictMode.h"


#define MY_VERSION_STRINGIFY(s) #s
#define MY_VERSION_TOSTRING(s) MY_VERSION_STRINGIFY(s)

string Version::GetVersion()
{
    // YYYY-MM-DD HH:MM:SS
    return string{MY_VERSION_TOSTRING(APP_BUILD_VERSION)}.substr(1, 19);
}

string Version::GetVersionShort()
{
    return Split(GetVersion(), " ")[0];
}

uint64_t Version::GetVersionAsInt()
{
    vector<string> partList     = Split(GetVersion(), " ");
    vector<string> datePartList = Split(partList[0], "-");
    vector<string> timePartList = Split(partList[1], ":");

    uint64_t version = 0;

    version += (uint64_t)atoi(datePartList[0].c_str());

    version *= 100;
    version += (uint64_t)atoi(datePartList[1].c_str());

    version *= 100;
    version += (uint64_t)atoi(datePartList[2].c_str());

    version *= 100;
    version += (uint64_t)atoi(timePartList[0].c_str());

    version *= 100;
    version += (uint64_t)atoi(timePartList[1].c_str());

    version *= 100;
    version += (uint64_t)atoi(timePartList[2].c_str());

    return version;
}

void Version::SetupShell()
{
    Shell::AddCommand("version", [](vector<string> argList){
        Log(GetVersion());
    }, { .argCount = 0, .help = "Display the build version"});
}

#undef MY_VERSION_STRINGIFY
#undef MY_VERSION_TOSTRING
