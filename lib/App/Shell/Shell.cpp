#include "Shell.h"
#include "Utl.h"

#define STR(s) #s
#define KEYWORD r
#define KEYWORD_STR STR(r)


void Shell::Eval(string cmd)
{
    string eval;

    EvalRoot(string{KEYWORD_STR " "} + cmd);
}

void Shell::EvalRoot(string cmd)
{
    shell_execute_cmd(shell_backend_uart_get_ptr(), cmd.c_str());
}

bool Shell::AddCommand(string name, function<void(vector<string> argList)> cbFn)
{
    return AddCommand(name, cbFn, CmdOptions{});
}

bool Shell::AddCommand(string name, function<void(vector<string> argList)> cbFn, CmdOptions cmdOptions)
{
    string argsStr;
    argsStr += "[";
    argsStr += to_string(cmdOptions.argCount);
    argsStr += "] ";

    cmdOptions.help = argsStr + cmdOptions.help;

    auto [it, success] = cmdLookup_.insert({name, {cmdOptions, cbFn}});

    return success;
}

bool Shell::RemoveCommand(string name)
{
    return 1 == cmdLookup_.erase(name);
}

int Shell::ShellCmdExecute(const struct shell *shellArg, size_t argc, char **argv)
{
    // notably this looks to be on the shell thread, need to push to main thread(?)
    // not actually an interrupt, so should be safe?

    string cmd = argv[0];

    auto it = cmdLookup_.find(cmd);
    if (it != cmdLookup_.end())
    {
        // pack arguments
        vector<string> argList;
        for (size_t i = 1; i < argc; ++i)
        {
            argList.push_back(argv[i]);
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

    return 0;
}

void Shell::ShellCmdListProbe(size_t idx, struct shell_static_entry *entry)
{
    // sadness how inefficient this is, but oh well
    size_t i = 0;
    bool found = false;
    for (auto &[name, cmdData] : cmdLookup_)
    {
        if (i == idx)
        {
            entry->syntax  = name.c_str();
            entry->handler = ShellCmdExecute;
            entry->subcmd  = NULL;
            entry->help    = cmdData.cmdOptions.help.c_str();
            entry->args =
            {
                .mandatory = (unsigned char)0,
                .optional = (unsigned char)0,
                // .mandatory = (unsigned char)
                // 	((cmdData.cmdOptions.argCount == -1) ? 0 : cmdData.cmdOptions.argCount),
                // .optional = (unsigned char)
                // 	((cmdData.cmdOptions.argCount == -1) ? 0 : cmdData.cmdOptions.argCount),
            };

            found = true;
            break;
        }

        ++i;
    }

    if (!found)
    {
        entry->syntax = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////

int ShellInit()
{
    Timeline::Global().Event("ShellInit");

    Shell::EvalRoot("shell echo off");

    return 1;
}

#include "JSONMsgRouter.h"
int ShellSetupJSON()
{
    Timeline::Global().Event("ShellSetupJSON");

    JSONMsgRouter::RegisterHandler("SHELL_COMMAND", [](auto &in, auto &out){
        UartTarget target(UART::UART_0);

        string eval = (const char *)in["cmd"];

        Shell::Eval(eval);
    });

    return 1;
}


#include <zephyr/init.h>
SYS_INIT(ShellInit, APPLICATION, 50);
SYS_INIT(ShellSetupJSON, APPLICATION, 80);



// Create root-level command which I will dynamically add fns to
SHELL_DYNAMIC_CMD_CREATE(shellData, Shell::ShellCmdListProbe);
SHELL_CMD_REGISTER(KEYWORD, &shellData, "App control", NULL);








/*

I tried hard to get a root-level dynamic command to work (eg without app first).
Kept segfaulting.

This was the approach, for posterity.


    static const struct shell_cmd_entry shell_cmd_dynamic __attribute__((section("." "shell_root_cmd_dynamic"))) __attribute__((used)) =
    {
        .is_dynamic = true,
        .u =
        {
            .dynamic_get = Shell:ShellCmdGet
        }
    };


*/



#if 0
static void dynamic_cmd_get(size_t idx, struct shell_static_entry *entry)
{
}

void Fn()
{

    ///////////////////////////////////////////////////////////////////////////////////

    /*
    SHELL_DYNAMIC_CMD_CREATE(m_sub_dynamic_set, dynamic_cmd_get);
    */

    // becomes
    static const struct shell_cmd_entry m_sub_dynamic_set =
    {
        .is_dynamic = true,
        .u =
        {
            .dynamic_get = dynamic_cmd_get
        }
    };

    ///////////////////////////////////////////////////////////////////////////////////

    /*
    SHELL_STATIC_SUBCMD_SET_CREATE(m_sub_dynamic,
        SHELL_CMD_ARG(add, NULL,
            "Add a new dynamic command.\nExample usage: [ dynamic add test "
            "] will add a dynamic command 'test'.\nIn this example, command"
            " name length is limited to 32 chars. You can add up to 20"
            " commands. Commands are automatically sorted to ensure correct"
            " shell completion.",
            cmd_dynamic_add, 2, 0),
        SHELL_CMD_ARG(execute, &m_sub_dynamic_set,
            "Execute a command.", cmd_dynamic_execute, 2, 0),
        SHELL_CMD_ARG(remove, &m_sub_dynamic_set,
            "Remove a command.", cmd_dynamic_remove, 2, 0),
        SHELL_CMD_ARG(show, NULL,
            "Show all added dynamic commands.", cmd_dynamic_show, 1, 0),
        SHELL_SUBCMD_SET_END
    );
    */

    // becomes

    static const struct shell_static_entry shell_m_sub_dynamic[] = 
    {
        {
            .syntax = (const char *)"add",
            .help = (const char *)"Add a new dynamic command.",
            .subcmd = (const struct shell_cmd_entry *)__null,
            .handler = (shell_cmd_handler)cmd_dynamic_add,
            .args =
            {
                .mandatory = 2,
                .optional = 0
            }
        },
        {
            .syntax = (const char *)"execute",
            .help = (const char *)"Execute a command.",
            .subcmd = (const struct shell_cmd_entry *)&m_sub_dynamic_set,
            .handler = (shell_cmd_handler)cmd_dynamic_execute,
            .args =
            {
                .mandatory = 2,
                .optional = 0
            }
        },
        {
            .syntax = (const char *)"remove",
            .help = (const char *)"Remove a command.",
            .subcmd = (const struct shell_cmd_entry *)&m_sub_dynamic_set,
            .handler = (shell_cmd_handler)cmd_dynamic_remove,
            .args =
            {
                .mandatory = 2,
                .optional = 0
            }
        },
        {
            .syntax = (const char *)"show",
            .help = (const char *)"Show all added dynamic commands.",
            .subcmd = (const struct shell_cmd_entry *)__null,
            .handler = (shell_cmd_handler)cmd_dynamic_show,
            .args =
            {
                .mandatory = 1,
                .optional = 0
            }
        },
        {
            __null
        }
    };
    static const struct shell_cmd_entry m_sub_dynamic =
    {
        .is_dynamic = false,
        .u =
        {
            .entry = shell_m_sub_dynamic
        }
    };

    ///////////////////////////////////////////////////////////////////////////////////

/*
    SHELL_CMD_REGISTER(dynamic, &m_sub_dynamic, "Demonstrate dynamic command usage.", NULL);
*/

    // becomes

    static const struct shell_static_entry _shell_dynamic =
    {
        .syntax = (const char *)"dynamic",
        .help = (const char *)"Demonstrate dynamic command usage.",
        .subcmd = (const struct shell_cmd_entry *)&m_sub_dynamic,
        .handler = (shell_cmd_handler)__null,
        .args =
        {
            .mandatory = 0,
            .optional = 0
        }
    };

    static const struct shell_cmd_entry shell_cmd_dynamic __attribute__((section("." "shell_root_cmd_dynamic"))) __attribute__((used)) =
    {
        .is_dynamic = false,
        .u =
        {
            .entry = &_shell_dynamic
        }
    };
}

#endif