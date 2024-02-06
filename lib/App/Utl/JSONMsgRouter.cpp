#include "JSONMsgRouter.h"
#include "PAL.h"
#include "Shell.h"
#include "Timeline.h"


////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////

int JSONMsgRouterInit()
{
    Timeline::Global().Event("JSONMsgRouterInit");
    
    JSONMsgRouter::Enable();

    return 1;
}

int JSONMsgRouterSetupShell()
{
    Timeline::Global().Event("JSONRouterSetupShell");

    Shell::AddCommand("json.send", [](vector<string> argList){
        if (argList.size() >= 1)
        {
            JSONMsgRouter::Send([&](const auto &out){
                out["type"] = argList[0];

                for (int i = 1; i + 1 < (int)argList.size(); i += 2)
                {
                    out[argList[i]] = argList[i + 1];
                }
            });
        }
    }, { .argCount = -1, .help = "send <type> [<tag> <value> ...]"});

    return 1;
}

int JSONMsgRouterSetupJSON()
{
    Timeline::Global().Event("JSONMsgRouterSetupJSON");

    JSONMsgRouter::RegisterHandler("REQ_PING", [](auto &in, auto &out){
        out["type"] = "REP_PING";

        out["timeNow"] = PAL.Micros();
    });

    JSONMsgRouter::RegisterHandler("REQ_PINGX", [](auto &in, auto &out){
        out["type"] = "REP_PINGX";

        out["timeNow"] = PAL.Micros();
    });

    return 1;
}



#include <zephyr/init.h>
SYS_INIT(JSONMsgRouterInit, APPLICATION, 50);
SYS_INIT(JSONMsgRouterSetupShell, APPLICATION, 80);
SYS_INIT(JSONMsgRouterSetupJSON, APPLICATION, 80);




