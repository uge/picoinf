#pragma once

#include "ArduinoJson-v6.21.1.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>


class JSON
{
    static const uint32_t JSON_DOC_BYTE_ALLOC = 5000;

public:
    using JSONObj = DynamicJsonDocument;

public:
    static std::pair<bool, JSONObj> DeSerialize(const std::string &jsonStr);
    static void UseJSON(const std::string &jsonStr, std::function<void(JsonObject &json)> fn);

    template <typename T>
    static bool HasKey(const T &json, const char *key)
    {
        return json.containsKey(key);
    }

    template <typename T>
    static bool HasKeyList(const T &json, const std::vector<const char *> &keyList)
    {
        bool retVal = true;

        for (auto key : keyList)
        {
            retVal &= HasKey(json, key);
        }

        return retVal;
    }

    static JSONObj GetObj();
    static std::string Serialize(JSONObj &json);
};

