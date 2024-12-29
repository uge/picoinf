#pragma once

#include <string>
using namespace std;

#include "JerryScriptUtl.h"
#include "Log.h"
#include "PAL.h"
#include "Shell.h"
#include "Utl.h"


class JerryScriptIntegration
{
public:

    static void SetupShell()
    {
        Shell::AddCommand("js.run", [](vector<string> argList){
            string script;

            if (argList.size() >= 1)
            {
                script = argList[0];
            }

            uint64_t timeoutMs = 0;

            if (argList.size() >= 2)
            {
                timeoutMs = atoi(argList[1].c_str());
            }

            if (script != "")
            {
                UseVMParseAndExecuteScript(script, timeoutMs);
            }

        }, { .argCount = -1, .help = "run \"<script>\" [timeoutMs]"});
    }

    static void UseVMParseAndExecuteScript(const string &script, uint64_t timeoutMs = 0)
    {
        Log("---Script---");
        Log(script);
        Log("------------");
        LogNL();

        uint64_t timeStart = PAL.Millis();

        string ret;
        JerryScript::UseVM([&]{
            JerryScript::EnableJerryX();

            ret = JerryScript::ParseAndExecuteScript(script, timeoutMs);
        });

        uint64_t timeDiff = PAL.Millis() - timeStart;

        Log("Script time  : ", Commas(timeDiff), " ms");
        Log("Script status: ", ret == "" ? "OK" : "ERR");
        Log("Script result: ", ret);
    }

private:
};