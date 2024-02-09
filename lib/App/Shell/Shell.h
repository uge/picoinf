#pragma once

#include "PAL.h"
#include "Log.h"

#include <string>
#include <vector>
#include <functional>
#include <map>
using namespace std;


extern void ShellInit();

class Shell
{
public:
    struct CmdOptions
    {
        int    argCount = 0;	// -1 means allow any
        string help     = "";
        bool   executeAsync = false;
    };

private:
    struct CmdData
    {
        CmdOptions cmdOptions;
        function<void(vector<string> argList)> cbFn_;
    };

public:
    static void Eval(string cmd);

    // put implementation in .cpp because system would crash on startup 
    // no idea, moving on for now
    static bool AddCommand(string name, function<void(vector<string> argList)> cbFn);
    static bool AddCommand(string name, function<void(vector<string> argList)> cbFn, CmdOptions cmdOptions);
    static bool RemoveCommand(string name);

    static void ShowHelp();

private:

    static void ShellCmdExecute(const string &line);

private:

    inline static map<string, CmdData> cmdLookup_;	
};

