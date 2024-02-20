#pragma once

#include "PAL.h"
#include "Log.h"

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <unordered_set>
using namespace std;


class Shell
{
public:
    struct CmdOptions
    {
        int    argCount = 0;	// -1 means allow any
        string help     = "";
    };

private:
    struct CmdData
    {
        CmdOptions cmdOptions;
        function<void(vector<string> argList)> cbFn_;
    };

public:
    static void Init();
    static void DisplayOn();
    static void SetupJSON();

    static bool Eval(string cmd);

    // put implementation in .cpp because system would crash on startup 
    // no idea, moving on for now
    static bool AddCommand(string name, function<void(vector<string> argList)> cbFn);
    static bool AddCommand(string name, function<void(vector<string> argList)> cbFn, CmdOptions cmdOptions);
    static bool RemoveCommand(string name);

    static void ShowHelp(string prefix = "");
    static bool RepeatPriorCommand();

private:

    static void ShellCmdExecute(const string &line);

private:

    inline static map<string, CmdData> cmdLookup_;	
    inline static string cmdLast_;
    inline static string prefix_;

    inline static unordered_set<string> internalCommandSet_ = {
        ".",
        "?",
        "clear",
        "help",
        "scope",
    };
};

