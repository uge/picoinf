#pragma once

#include "PAL.h"
#include "Log.h"

#include <string>
#include <functional>
#include <map>
#include <unordered_set>
#include <vector>


class Shell
{
public:

    struct CmdOptions
    {
        int         argCount = 0;    // -1 means allow any
        std::string help     = "";
    };


private:

    struct CmdData
    {
        CmdOptions cmdOptions;
        std::function<void(std::vector<std::string> argList)> cbFn_;
    };


public:

    static void Init();
    static void DisplayOn();
    static void SetupJSON();

    static bool Eval(std::string cmd);

    // put implementation in .cpp because system would crash on startup 
    // no idea, moving on for now
    static bool AddCommand(std::string name, std::function<void(std::vector<std::string> argList)> cbFn);
    static bool AddCommand(std::string name, std::function<void(std::vector<std::string> argList)> cbFn, CmdOptions cmdOptions);
    static bool RemoveCommand(std::string name);

    static void ShowHelp(std::string prefix = "");
    static bool RepeatPriorCommand();


private:

    static void ShellCmdExecute(const std::string &line);

    static void HistoryAdd(const std::string &cmd);
    static void HistoryShow();
    static std::string HistoryGet(uint16_t num);


private:

    inline static std::map<std::string, CmdData> cmdLookup_;	
    inline static std::string prefix_;

    inline static std::unordered_set<std::string> internalCommandSet_ = {
        "!",
        "!!",
        ".",
        "?",
        "clear",
        "help",
        "h",
        "history",
        "scope",
    };

    struct History
    {
        uint16_t num;
        std::string cmd;
    };

    inline static uint8_t MAX_HISTORY_SIZE = 20;
    inline static std::vector<History> historyList_;
};

