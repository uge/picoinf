#include "Evm.h"
#include "Shell.h"
#include "Timeline.h"
#include "UART.h"
#include "Utl.h"

bool Shell::Eval(string cmd)
{
    bool didEval = false;

    cmd = trim(cmd);

    if (cmd.length())
    {
        didEval = true;

        ShellCmdExecute(cmd);

        if (cmd != ".")
        {
            cmdLast_ = cmd;
        }
    }

    return didEval;
}

bool Shell::RepeatPriorCommand()
{
    return Shell::Eval(cmdLast_);
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

                int32_t cmdLen = nameNoPrefix.length();
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

    vector<string> linePartList = Split(line);

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
// Initilization
////////////////////////////////////////////////////////////////////////////////


void Shell::Init()
{
    Timeline::Global().Event("Shell::PreInit");

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

