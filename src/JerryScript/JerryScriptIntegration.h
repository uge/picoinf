#pragma once

#include <string>
using namespace std;

#include "JerryScriptUtl.h"
#include "Log.h"
#include "PAL.h"
#include "Shell.h"
#include "Timeline.h"
#include "Utl.h"
#include "WDT.h"


class JerryScriptIntegration
{
public:

    static void Init()
    {
        Timeline::Global().Event("JerryScriptIntegration::Init");

        JerryScript::SetPreRunHook([]{
            Watchdog::Feed();
        });
    }

    static void SetupShell()
    {
        Timeline::Global().Event("JerryScriptIntegration::SetupShell");

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

        string err;
        JerryScript::UseVM([&]{
            err = JerryScript::ParseAndRunScript(script, timeoutMs);
        });

        Log("VM total     : ", Commas(JerryScript::GetVMDurationMs()), " ms");
        Log("VM overhead  : ", Commas(JerryScript::GetVMOverheadDurationMs()), " ms");
        Log("VM callback  : ", Commas(JerryScript::GetVMCallbackDurationMs()), " ms");
        Log("Script parse : ", Commas(JerryScript::GetScriptParseDurationMs()), " ms");
        Log("Script run   : ", Commas(JerryScript::GetScriptRunDurationMs()), " ms");
        Log("Script status: ", err == "" ? "OK" : "ERR");
        if (err == "")
        {
            Log("Script err   : <None>");
        }
        else
        {
            Log("Script err   :");
            Log(err);
        }
        Log("Script output:");
        Log(JerryScript::GetScriptOutput());
    }

private:
};