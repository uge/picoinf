#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>

using namespace std;

#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>

#include "Evm.h"
#include "PAL.h"
#include "Log.h"


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
    static void EvalRoot(string cmd);

    // put implementation in .cpp because system would crash on startup 
    // no idea, moving on for now
    static bool AddCommand(string name, function<void(vector<string> argList)> cbFn);
    static bool AddCommand(string name, function<void(vector<string> argList)> cbFn, CmdOptions cmdOptions);
    static bool RemoveCommand(string name);


public:

    static int ShellCmdExecute(const struct shell *shellArg, size_t argc, char **argv);

    // Fill out entry when asked.
    // This function is probed each time the 'app' command is run.
    // It is called with ever-increasing indexes until a null-like
    // entry is returned
    static void ShellCmdListProbe(size_t idx, struct shell_static_entry *entry);

private:

    inline static map<string, CmdData> cmdLookup_;	
};

