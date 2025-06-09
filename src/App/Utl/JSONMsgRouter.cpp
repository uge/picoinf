#include "JSONMsgRouter.h"
#include "PAL.h"
#include "Shell.h"
#include "Timeline.h"
#include "UART.h"

using namespace std;

#include "StrictMode.h"


void JSONMsgRouter::OnLine(const Iface &iface, const string &jsonStr)
{
    // default to outputting to regular serial so
    // all code following this can log normally
    UartTarget target(UART::UART_0);

    auto [ok, inJsonDoc] = JSON::DeSerialize(jsonStr);

    // 'as' the document to a json object ('to' would reset it before doing so)
    auto inJson = inJsonDoc.as<JsonObject>();

    if (ok)
    {
        if (inJson.containsKey("type"))
        {
            bool handled = false;

            const char *typeStr = inJson["type"];

            // get root document.
            // convert to an object.
            // can't 'as' to an object, it's null at the moment
            auto outJsonDoc = JSON::GetObj();
            auto outJson = outJsonDoc.to<JsonObject>();

            // keep track of whether a reply is sent
            bool replySent = false;

            // find handler
            for (auto &[type, cbHandle] : handlerDataList_)
            {
                if (type == typeStr)
                {
                    // actually call handler
                    cbHandle(inJson, outJson);

                    // look at return size to decide if a reply is to be sent
                    size_t size = measureJson(outJsonDoc);
                    if (size > OBSERVED_EMPTY_MESSAGE_SIZE)
                    {
                        iface.OnReceive(JSON::Serialize(outJsonDoc));

                        replySent = true;
                    }

                    // only one handler per message type
                    handled = true;
                    break;
                }
            }

            if (handled == false)
            {
                Log("ERR: message was not handled: \"", JSON::Serialize(inJsonDoc), "\"");
            }

            // ensure every received message is replied to, either by the
            // handling code, or automatically
            if (handled && replySent == false)
            {
                // auto ack
                outJson["type"] = "ACK";
                outJson["inType"] = inJson["type"];
                iface.OnReceive(JSON::Serialize(outJsonDoc));
            }
        }
        else
        {
            Log("ERR: message has no type: \"", JSON::Serialize(inJsonDoc), "\"");
        }
    }
}

void JSONMsgRouter::Send(const Iface &iface, function<void(JsonObject &out)> handler)
{
    // create json object to send
    auto jsonDoc = JSON::GetObj();
    auto out     = jsonDoc.to<JsonObject>();

    // let the handler fill out the outbound message
    handler(out);

    // look at return size to decide if it should be sent
    size_t size = measureJson(jsonDoc);

    if (size > OBSERVED_EMPTY_MESSAGE_SIZE)
    {
        iface.OnReceive(JSON::Serialize(jsonDoc));
    }
    else
    {
        Log("ERR: JSON Send attempted with empty message");
    }
}


















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

                for (size_t i = 1; i + 1 < argList.size(); i += 2)
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



