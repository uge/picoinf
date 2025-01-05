#include "Format.h"
#include "Shell.h"
#include "Timeline.h"

using namespace std;

#include "StrictMode.h"


////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////

int FormatSetupShell()
{
    Timeline::Global().Event("FormatSetupShell");

    Shell::AddCommand("fmt.str", [](vector<string> argList){
        string fmt = argList[0];
        uint8_t val = atoi(argList[1].c_str());

        string cvt = Format::Str(fmt, val);
        Log(val, " -> \"", cvt, "\"");
    }, { .argCount = 2, .help = "Format <fmt> <uint8_t>"});

    Shell::AddCommand("fmt.tohex8", [](vector<string> argList){
        uint8_t val = atoi(argList[0].c_str());

        // prepend 0x if odd
        bool prepend = val % 2;

        string cvt = Format::ToHex(val, prepend);
        Log(val, " -> \"", cvt, "\"");
    }, { .argCount = 1, .help = "ToHex <fmt> <uint8_t>"});

    Shell::AddCommand("fmt.tohex16", [](vector<string> argList){
        uint16_t val = atoi(argList[0].c_str());

        // prepend 0x if odd
        bool prepend = val % 2;

        string cvt = Format::ToHex(val, prepend);
        Log(val, " -> \"", cvt, "\"");
    }, { .argCount = 1, .help = "ToHex <fmt> <uint16_t>"});

    Shell::AddCommand("fmt.fromhex", [](vector<string> argList){
        string valStr = argList[0];

        uint32_t val = Format::FromHex(valStr);

        Log(valStr, " -> ", val);
    }, { .argCount = 1, .help = "FromHex <str>"});

    Shell::AddCommand("fmt.tobin8", [](vector<string> argList){
        uint8_t val = atoi(argList[0].c_str());

        // prepend 0x if odd
        bool prepend = val % 2;

        string cvt = Format::ToBin(val, prepend);
        Log(val, " -> \"", cvt, "\"");
    }, { .argCount = 1, .help = "ToBin <fmt> <uint8_t>"});

    return 1;
}
