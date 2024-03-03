#include "JSONMsgRouter.h"
#include "PAL.h"
#include "Shell.h"
#include "Timeline.h"


////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////

void JSONMsgRouter::Init()
{
    Timeline::Global().Event("JSONMsgRouter::Init");
}

void JSONMsgRouter::SetupShell()
{
    Timeline::Global().Event("JSONRouter::SetupShell");

    static JSONMsgRouter::Iface router_;
    router_.SetOnReceiveCallback([](const string &jsonStr){
        Log(jsonStr);
    });

    Shell::AddCommand("json.send", [](vector<string> argList){
        if (argList.size() >= 1)
        {
            router_.Send([&](const auto &out){
                out["type"] = argList[0];

                for (int i = 1; i + 1 < (int)argList.size(); i += 2)
                {
                    out[argList[i]] = argList[i + 1];
                }
            });
        }
    }, { .argCount = -1, .help = "send <type> [<tag> <value> ...]"});

    Shell::AddCommand("json.list", [](vector<string> argList){
        vector<string> typeList;
        for (auto &hd : JSONMsgRouter::handlerDataList_)
        {
            typeList.push_back(hd.type);
        }
        sort(typeList.begin(), typeList.end());
        for (auto &type : typeList)
        {
            Log(type);
        }
    }, { .argCount = 0, .help = "list json routing"});
}

void JSONMsgRouter::SetupJSON()
{
    Timeline::Global().Event("JSONMsgRouter::SetupJSON");

    JSONMsgRouter::RegisterHandler("REQ_PING", [](auto &in, auto &out){
        out["type"] = "REP_PING";

        out["timeNow"] = PAL.Micros();
    });

    JSONMsgRouter::RegisterHandler("REQ_PINGX", [](auto &in, auto &out){
        out["type"] = "REP_PINGX";

        out["timeNow"] = PAL.Micros();
    });
}



