#include "Utl.h"
#include "Timeline.h"


static string num(25, '\0');
static string retVal(25, '\0');

string &CommasStatic(const char *numStr)
{
    num = numStr;
    retVal.clear();

    reverse(num.begin(), num.end());

    int count = 0;
    for (auto c : num)
    {
        ++count;

        if (count == 4)
        {
            retVal.push_back(',');
            count = 1;
        }

        retVal.push_back(c);
    }

    reverse(retVal.begin(), retVal.end());

    return retVal;
}

#include "Blinker.h"
static Blinker blinker;


void UtlSetupShell()
{
    Timeline::Global().Event("UtlSetupShell");

    blinker.SetName("TIMER_BLINKER_UTL");

    Shell::AddCommand("blinker.set.pin", [](vector<string> argList){
        uint8_t pin = atoi(argList[0].c_str());

        blinker.SetPin({pin});
    }, { .argCount = 1, .help = "set <pin>"});

    Shell::AddCommand("blinker.set.onoff", [](vector<string> argList){
        uint32_t onMs = atoi(argList[0].c_str());
        uint32_t offMs = atoi(argList[1].c_str());

        blinker.SetBlinkOnOffTime(onMs, offMs);
    }, { .argCount = 2, .help = "set <on> <off> ms"});

    Shell::AddCommand("blinker.blink", [](vector<string> argList){
        uint32_t blinkCount = atoi(argList[0].c_str());

        blinker.Blink(blinkCount);
    }, { .argCount = 1, .help = "blink <x> times"});

    Shell::AddCommand("blinker.on", [](vector<string> argList){
        blinker.On();
    }, { .argCount = 0, .help = ""});

    Shell::AddCommand("blinker.off", [](vector<string> argList){
        blinker.Off();
    }, { .argCount = 0, .help = ""});

    Shell::AddCommand("blinker.async.on", [](vector<string> argList){
        blinker.EnableAsyncBlink();
    }, { .argCount = 0, .help = ""});

    Shell::AddCommand("blinker.async.off", [](vector<string> argList){
        blinker.DisableAsyncBlink();
    }, { .argCount = 0, .help = ""});
}

