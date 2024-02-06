#pragma once

#include <string>
#include <utility>
using namespace std;

#include "ArduinoJson-v6.21.1.h"

#include "Log.h"
#include "UART.h"


class JSON
{
    static const uint32_t JSON_DOC_BYTE_ALLOC = 256;

public:
    using JSONObj = DynamicJsonDocument;

public:
    static pair<bool, JSONObj> DeSerialize(const string &jsonStr)
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

    static JSONObj GetObj()
    {
        return DynamicJsonDocument{ JSON_DOC_BYTE_ALLOC };
    }

    static void SerializeToUart(JSONObj &json, UART uart = UartCurrent())
    {
        class WriterToUart
        {
        public:
            size_t write(uint8_t c)
            {
                UartSend(&c, 1);

                return 1;
            }

            size_t write(const uint8_t *buf, size_t length)
            {
                UartSend(buf, length);

                return length;
            }
        };

        UartTarget target(uart);
        WriterToUart writer;
        serializeJson(json, writer);
        LogNL();
    }
};

