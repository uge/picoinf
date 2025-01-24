#include "Evm.h"
#include "Shell.h"
#include "Timeline.h"
#include "UART.h"
#include "Utl.h"

using namespace std;

#include "StrictMode.h"


bool Shell::Eval(string input)
{
    bool didEval = false;
    bool process = true;
    vector<string> cmdList = SplitByCharQuoteAware(input, ';');

    // implement !<num>
    if (cmdList.size())
    {
        string cmd = Trim(cmdList[0]);

        if (cmd.size() >= 2)
        {
            if (cmd[0] == '!' && cmd[1] == '!' && cmd.size() == 2)
            {
                RepeatPriorCommand();
                didEval = true;
                process = false;
            }
            else if (cmd[0] == '!')
            {
                // pull up the history
                uint16_t num = (uint16_t)atoi(cmd.substr(1).c_str());
                input = HistoryGet(num);

                if (input != "")
                {
                    cmdList = SplitByCharQuoteAware(input, ';');
                }
                else
                {
                    Log("ERR: No history found for ", num);
                    didEval = true;
                    process = false;
                }
            }
        }
    }

    if (process)
    {
        string maybeNewline = "";
        bool didInternalCmd = false;
        for (auto &cmd : cmdList)
        {
            cmd = Trim(cmd);

            if (cmd.length())
            {
                didEval = true;

                didInternalCmd |= internalCommandSet_.contains(cmd);

                LogNNL(maybeNewline);
                maybeNewline = "\n";

                ShellCmdExecute(cmd);
            }
        }

        if (didEval)
        {
            // watch out for infinite loop on '.' being in the list
            if (didInternalCmd == false)
            {
                HistoryAdd(input);
            }
        }
    }

    return didEval;
}

void Shell::HistoryAdd(const string &cmd)
{
    // add to history unless it's the same as the last one
    bool addHistory = true;
    uint16_t num = 1;
    if (historyList_.size())
    {
        History &last = historyList_.back();

        addHistory    = last.cmd != cmd;
        num = last.num + 1;
    }

    if (addHistory)
    {
        if (historyList_.size() == MAX_HISTORY_SIZE)
        {
            historyList_.erase(historyList_.begin());
        }

        historyList_.push_back({
            num,
            cmd,
        });
    }
}

void Shell::HistoryShow()
{
    if (historyList_.size())
    {
        uint8_t fieldWidth = (uint8_t)to_string(historyList_.back().num).length();

        for (const auto &history : historyList_)
        {
            string num = StrUtl::PadLeft(to_string(history.num), ' ', fieldWidth);

            Log(num, "  ", history.cmd);
        }
    }
}

string Shell::HistoryGet(uint16_t num)
{
    string retVal;

    for (const auto &history : historyList_)
    {
        if (history.num == num)
        {
            retVal = history.cmd;
            break;
        }
    }

    return retVal;
}

bool Shell::RepeatPriorCommand()
{
    bool retVal = false;

    if (historyList_.size())
    {
        string &cmd = historyList_.back().cmd;

        Log(cmd);
        retVal = Shell::Eval(cmd);
    }

    return retVal;
}

bool Shell::AddCommand(string name, function<void(vector<string> argList)> cbFn)
{
    return AddCommand(name, cbFn, CmdOptions{});
}

bool Shell::AddCommand(string name, function<void(vector<string> argList)> cbFn, CmdOptions cmdOptions)
{
    auto [it, success] = cmdLookup_.insert({name, {cmdOptions, cbFn}});

    return success;
}

bool Shell::RemoveCommand(string name)
{
    return 1 == cmdLookup_.erase(name);
}

void Shell::ShowHelp(string prefixExtra)
{
    // track command length
    size_t maxLenCmd = 0;

    // build list of internal commands and note maximum lengths
    vector<string> cmdListInternal;
    for (auto &name : internalCommandSet_)
    {
        cmdListInternal.push_back(name);

        if (name.length() > maxLenCmd)
        {
            maxLenCmd = name.length();
        }
    }

    sort(cmdListInternal.begin(), cmdListInternal.end());

    // get list of commands and note some maximum lengths
    // apply any active prefix filtering
    vector<string> cmdListExternal;

    string prefix = prefix_;
    if (prefixExtra == "\"\"")
    {
        prefix = "";
    }
    else
    {
        prefix += prefixExtra;
    }
    uint32_t prefixLen = prefix.length();

    for (auto &[name, cmdData] : cmdLookup_)
    {
        if (IsPrefix(prefix, name) && internalCommandSet_.contains(name) == false)
        {
            string nameNoPrefix = name.substr(prefixLen);

            if (nameNoPrefix.length())
            {
                cmdListExternal.push_back(nameNoPrefix);

                size_t cmdLen = nameNoPrefix.length();
                if (cmdLen > maxLenCmd)
                {
                    maxLenCmd = cmdLen;
                }
            }
        }
    }

    // put commands in sorted order
    sort(cmdListExternal.begin(), cmdListExternal.end());

    auto FnShow = [&](string msg, vector<string> &cmdList, bool addPrefix)
    {
        Log(msg);
        Log("----------------------------------------");

        // print it up nicely
        const uint8_t BUF_SIZE = 150;
        char formatStr[BUF_SIZE];
        char completedStr[BUF_SIZE];

        auto fnPrint = [&](const char *fmt, size_t len, string val) {
            snprintf(formatStr,    BUF_SIZE, fmt, len);
            snprintf(completedStr, BUF_SIZE, formatStr, val.c_str());
        };

        for (auto &cmd : cmdList)
        {
            CmdData &cd = addPrefix ? cmdLookup_[prefix + cmd] : cmdLookup_[cmd];

            fnPrint("%%-%us", maxLenCmd, cmd);

            string argCount = to_string(cd.cmdOptions.argCount);
            if (cd.cmdOptions.argCount == -1)
            {
                argCount = "*";
            }

            Log(completedStr,
                " [",
                argCount,
                "] : ", 
                cd.cmdOptions.help);
        }
    };

    LogNL();
    FnShow("Shell Commands", cmdListInternal, false);
    LogNL();
    string moduleMsg = "Module Commands";
    if (prefix != "")
    {
        moduleMsg += " (scoped by \"";
        moduleMsg += prefix;
        moduleMsg += "\")";
    }
    FnShow(moduleMsg, cmdListExternal, true);
}

void Shell::ShellCmdExecute(const string &line)
{
    // this is on the serial in thread

    vector<string> linePartList = SplitQuotedString(line);

    string cmdOrig = linePartList[0];
    string cmd     = cmdOrig;
    if (internalCommandSet_.contains(cmdOrig) == false)
    {
        cmd = prefix_ + cmd;
    }

    // try the prefixed command
    auto it = cmdLookup_.find(cmd);

    // if that didn't work, try the global level (as a convenience)
    if (it == cmdLookup_.end())
    {
        it = cmdLookup_.find(cmdOrig);
    }

    if (it != cmdLookup_.end())
    {
        // pack arguments
        vector<string> argList;
        for (size_t i = 1; i < linePartList.size(); ++i)
        {
            argList.push_back(linePartList[i]);
        }

        // unpack tuple
        auto &[name, cmdData] = *it;

        if (cmdData.cmdOptions.argCount == -1 ||
            cmdData.cmdOptions.argCount == (int)argList.size())
        {
            cmdData.cbFn_(argList);
        }
        else
        {
            Log("ERR: \"", name, "\" requires ", cmdData.cmdOptions.argCount, " args");
        }
    }
    else
    {
        Log("ERR: Could not find command \"", cmd, "\"");
    }
}


////////////////////////////////////////////////////////////////////////////////
// Initialization
////////////////////////////////////////////////////////////////////////////////


void Shell::Init()
{
    Timeline::Global().Event("Shell::Init");

    Shell::AddCommand("!", [](vector<string>){
        Log("ERR: You must specify a history number, eg !34");
    }, { .help = "Repeat numbered command" });

    Shell::AddCommand("!!", [](vector<string>){
        Shell::RepeatPriorCommand();
    }, { .help = "Repeat prior command" });

    Shell::AddCommand(".", [](vector<string>){
        Shell::RepeatPriorCommand();
    }, { .help = "Repeat prior command" });

    Shell::AddCommand("?", [](vector<string> argList){
        string prefix = "";

        if (argList.size())
        {
            prefix = argList[0];
        }

        Shell::ShowHelp(prefix);
    }, { .argCount = -1, .help = "Show Help (scope by optional first argument)" });

    Shell::AddCommand("help", [](vector<string> argList){
        string prefix = "";

        if (argList.size())
        {
            prefix = argList[0];
        }

        Shell::ShowHelp(prefix);
    }, { .argCount = -1, .help = "Show Help (scope by optional first argument)" });

    Shell::AddCommand("h", [](vector<string>){
        HistoryShow();
    }, { .help = "Show history" });

    Shell::AddCommand("history", [](vector<string>){
        HistoryShow();
    }, { .help = "Show history" });

    Shell::AddCommand("scope", [](vector<string> argList){
        if (argList.size() == 0)
        {
            if (prefix_ == "")
            {
                Log("Not currently scoped");
            }
            else
            {
                Log("Current scope is \"", prefix_, "\"");
            }
        }
        else
        {
            if (argList.size() >= 1)
            {
                string prefix = argList[0];

                if (prefix == "\"\"")
                {
                    prefix_ = "";
                }
                else
                {
                    prefix_ = prefix + ".";
                }

                Log("Command scope now \"", prefix_, "\"");
                LogNL();
            }

            if (argList.size() >= 2)
            {
                if (argList[1] != "0")
                {
                    Shell::ShowHelp();
                }
            }
        }
    }, { .argCount = -1, .help = "Scope all commands within <x> as a prefix" });

    // https://stackoverflow.com/questions/37774983/clearing-the-screen-by-printing-a-character
    Shell::AddCommand("clear", [](vector<string>){
        Log("\033[2J"); // clear
        Log("\033[H");  // move to home position
        for (int i = 0; i < 150; ++i)
        {
            Log(">");
        }
    }, { .help = "Clear the screen" });

    UartAddLineStreamCallback(UART::UART_0, [](const string &line){
        if (Shell::Eval(line))
        {
            LogNL();
        }
        LogNNL(prefix_.length() ? prefix_ + " > ": "> ");
    }, false);
}

void Shell::DisplayOn()
{
    LogNNL(prefix_.length() ? prefix_ + " > ": "> ");
}

#include "JSONMsgRouter.h"
void Shell::SetupJSON()
{
    Timeline::Global().Event("Shell::SetupJSON");

    JSONMsgRouter::RegisterHandler("SHELL_COMMAND", [](auto &in, auto &out){
        UartTarget target(UART::UART_0);

        string eval = (const char *)in["cmd"];

        Shell::Eval(eval);
    });
}

