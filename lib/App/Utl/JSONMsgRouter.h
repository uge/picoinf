#pragma once

#include <functional>

#include "JSON.h"
#include "Timeline.h"

extern void JSONMsgRouterInit();
extern void JSONMsgRouterSetupShell();
extern void JSONMsgRouterSetupJSON();

class JSONMsgRouter
{
    static const uint8_t OBSERVED_EMPTY_MESSAGE_SIZE = 4;

    // handlers receive the inbound object, which has at least the type they want
    // handlers recieve a json object they can fill out to reply
    // no reply is sent if the outJson is empty
    using Handler = function<void(const JsonObject &in, JsonObject &out)>;

    struct HandlerData
    {
        string  type;
        Handler cbHandle;
    };

public:

    static void Enable()
    {
        Disable();

        auto [ok, id] = UartAddLineStreamCallback(UART::UART_USB, [](const string &line){
            OnLine(line);
        });

        id_ = id;
    }

    static void Disable()
    {
        if (id_ != 0)
        {
            UartRemoveLineStreamCallback(uart_, id_);

            id_ = 0;
        }
    }

    static void OnLine(const string &jsonStr)
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
                            JSON::SerializeToUart(outJsonDoc, uart_);

                            replySent = true;
                        }

                        // only one handler per message type
                        handled = true;
                        break;
                    }
                }

                if (handled == false)
                {
                    Log("ERR: message was not handled");
                    JSON::SerializeToUart(inJsonDoc);
                }

                // ensure every received message is replied to, either by the
                // handling code, or automatically
                if (handled && replySent == false)
                {
                    // auto ack
                    outJson["type"] = "ACK";
                    outJson["inType"] = inJson["type"];
                    JSON::SerializeToUart(outJsonDoc, uart_);
                }
            }
            else
            {
                Log("ERR: message has no type");
                JSON::SerializeToUart(inJsonDoc);
            }
        }
    }

    // designed to allow handlers to let other handlers fill out sub-json messages
    static void InternalRoute(function<void(JsonObject &theirInJson)> preHandler, JsonObject &theirOutJson)
    {
        // create json object to get filled out and used as the faked inbound message
        auto theirInJsonDoc = JSON::GetObj();
        auto theirInJson    = theirInJsonDoc.to<JsonObject>();

        // let the handler fill out the details of the faked inbound
        preHandler(theirInJson);

        // route to handlers
        if (theirInJson.containsKey("type"))
        {
            const char *typeStr = theirInJson["type"];

            for (auto &[type, cbHandle] : handlerDataList_)
            {
                if (type == typeStr)
                {
                    // actually call handler
                    cbHandle(theirInJson, theirOutJson);
                }
            }
        }
    }

    static void Send(function<void(JsonObject &out)> handler)
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
            JSON::SerializeToUart(jsonDoc, uart_);
        }
        else
        {
            Log("ERR: JSON Send attempted with empty message");
        }
    }

    static void RegisterHandler(string type, Handler cbHandle)
    {
        handlerDataList_.push_back({type, cbHandle});
    }


private:

    inline static uint8_t id_ = 0;
    inline static UART uart_ = UART::UART_USB;

public:
    inline static vector<HandlerData> handlerDataList_;
};
