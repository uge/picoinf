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

void Shell::ShowHelp(string prefix)
{
    vector<string> cmdList;

    // get list of commands and note some maximum lengths
    size_t maxLenCmd = 0;
    for (auto &[name, cmdData] : cmdLookup_)
    {
        if (prefix == "" || IsPrefix(prefix, name))
        {
            cmdList.push_back(name);

            if (name.length() > maxLenCmd)
            {
                maxLenCmd = name.length();
            }
        }
    }

    // put commands in sorted order
    sort(cmdList.begin(), cmdList.end());

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
        CmdData &cd = cmdLookup_[cmd];

        fnPrint("%%%us", maxLenCmd, cmd);

        string argCount = to_string(cd.cmdOptions.argCount);
        if (cd.cmdOptions.argCount == -1)
        {
            argCount = "*";
        }

        Log(completedStr,
            " [",
            argCount,
            " / ",
            cd.cmdOptions.executeAsync ? "evm" : "syn",
            "] : ", 
            cd.cmdOptions.help);
    }
}

void Shell::ShellCmdExecute(const string &line)
{
    // this is on the serial in thread

    vector<string> linePartList = Split(line);

    string cmd = linePartList[0];

    auto it = cmdLookup_.find(cmd);
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
            if (!cmdData.cmdOptions.executeAsync)
            {
                // the only way the shell thread gets time is when the main
                // thread is sleeping, so it is safe to call main code here
                // provided this thread doesn't get pre-empted by
                // the main thread.  we prevent this here.
                //
                // and actually this is probably what all calls should do?
                //
                // is that true?
                // injecting operations that create async action, what happens
                // then?
                //
                // eg function does a thing, sets a timer.
                //   then this function returns pretty quickly, and when the
                //   main thread sees the timer, it handles it in the main
                //   thread, so yes this is ok.
                //
                // what about literally calling Delay or DelayBusy here?
                //
                // when async Delay
                // - is later handled in main thread, blocking other timed
                //   events.  this function here returns immediately.
                // when sync (here) Delay
                // - this thread is held up, main thread unimpacted
                //
                // when async DelayBusy
                // - later handled in main thread, holding other timed
                //   events, but letting other threads run.  this function
                //   returns immediately.
                // when sync DelayBusy
                // - this thread becomes unresponsive but others can run.
                // 
                // so, unless you're looking for a particular behavior to
                // influence the main thread, safe to call from here.  this
                // should be the norm.
                PAL.SchedulerLock();
                cmdData.cbFn_(argList);
                PAL.SchedulerUnlock();
            }
            else
            {
                // register callback to execute
                Evm::QueueWork("SHELL_EVM_QUEUE", [=](){
                    cmdData.cbFn_(argList);
                });
            }
        }
        else
        {
            Log("ERR: ", name, " requires ", cmdData.cmdOptions.argCount, " args");
        }
    }
    else
    {
        Log("ERR: Could not find command ", cmd);
    }
}


////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////


void ShellInit()
{
    Timeline::Global().Event("ShellInit");

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
    }, { .argCount = -1, .help = "Show Help (filter by first argument)" });

    Shell::AddCommand("help", [](vector<string> argList){
        string prefix = "";

        if (argList.size())
        {
            prefix = argList[0];
        }

        Shell::ShowHelp(prefix);
    }, { .argCount = -1, .help = "Show Help (filter by first argument)" });

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
        LogNNL("> ");
    }, false);
    
    LogNNL("> ");
}

#include "JSONMsgRouter.h"
void ShellSetupJSON()
{
    Timeline::Global().Event("ShellSetupJSON");

    JSONMsgRouter::RegisterHandler("SHELL_COMMAND", [](auto &in, auto &out){
        UartTarget target(UART::UART_0);

        string eval = (const char *)in["cmd"];

        Shell::Eval(eval);
    });
}

