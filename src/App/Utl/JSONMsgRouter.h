#pragma once

#include "JSON.h"

#include <functional>
#include <string>


class JSONMsgRouter
{
    static const uint8_t OBSERVED_EMPTY_MESSAGE_SIZE = 4;

    // handlers receive the inbound object, which has at least the type they want
    // handlers recieve a json object they can fill out to reply
    // no reply is sent if the outJson is empty
    using Handler = std::function<void(const JsonObject &in, JsonObject &out)>;

    struct HandlerData
    {
        std::string type;
        Handler     cbHandle;
    };

public:

    // An abstract interface class lets callers send and receive messages,
    // adapted to their particular needs.
    class Iface
    {
    public:

        void Route(const std::string &jsonStr) const
        {
            JSONMsgRouter::OnLine(*this, jsonStr);
        }

        void Send(std::function<void(JsonObject &out)> handler) const
        {
            JSONMsgRouter::Send(*this, handler);
        }

        void OnReceive(const std::string &jsonStr) const
        {
            cbFnOnReceive_(jsonStr);
        }

        void SetOnReceiveCallback(std::function<void(const std::string &jsonStr)> cbFn)
        {
            cbFnOnReceive_ = cbFn;
        }

    private:
        std::function<void(const std::string &jsonStr)> cbFnOnReceive_ = [](const std::string &jsonStr){};
    };

public:

    static void RegisterHandler(std::string type, Handler cbHandle)
    {
        handlerDataList_.push_back({type, cbHandle});
    }


private:

    static void OnLine(const Iface &iface, const std::string &jsonStr);
    static void Send(const Iface &iface, std::function<void(JsonObject &out)> handler);

public:

    static void Init();
    static void SetupShell();
    static void SetupJSON();


private:

    inline static std::vector<HandlerData> handlerDataList_;
};
