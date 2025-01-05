#include "JSON.h"
#include "Log.h"
#include "UART.h"

using namespace std;

#include "StrictMode.h"


pair<bool, JSON::JSONObj> JSON::DeSerialize(const string &jsonStr)
{
    bool ok = false;

    DynamicJsonDocument docParse(JSON_DOC_BYTE_ALLOC);

    DeserializationError ret = deserializeJson(docParse, jsonStr);

    if (ret == DeserializationError::Ok)
    {
        ok = true;
    }
    else
    {
        UartTarget target(UART::UART_0);
        Log("ERR: JSON DeSerialize: \"", jsonStr, "\": ", ret.c_str());
    }

    return { ok, docParse };
}

void JSON::UseJSON(const string &jsonStr, function<void(JsonObject &json)> fn)
{
    auto [ok, jsonDoc] = DeSerialize(jsonStr);
    if (ok)
    {
        auto json = jsonDoc.as<JsonObject>();

        fn(json);
    }
}

JSON::JSONObj JSON::GetObj()
{
    return DynamicJsonDocument{ JSON_DOC_BYTE_ALLOC };
}

string JSON::Serialize(JSONObj &json)
{
    class WriterToString
    {
    public:
        size_t write(uint8_t c)
        {
            jsonStr += c;

            return 1;
        }

        size_t write(const uint8_t *buf, size_t length)
        {
            for (size_t i = 0; i < length; ++i)
            {
                jsonStr += (char)buf[i];
            }

            return length;
        }

        string GetString()
        {
            return jsonStr;
        }

    private:
        string jsonStr;
    };

    WriterToString writer;
    serializeJson(json, writer);

    return writer.GetString();
}

