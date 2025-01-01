#pragma once

#include <string>
using namespace std;

#include "GPS.h"


class JSProxy_GPS
{
public:

    static void Proxy(jerry_value_t obj, Fix3DPlus *gpsFix)
    {
        vector<string> fieldNameList = {
            "Latitude",
            "Longitude",
            "AltitudeMeters",
            "AltitudeFeet",
        };

        for (const auto &fieldName : fieldNameList)
        {
            string getFnName = string{"Get"} + fieldName;
            JerryScript::SetPropertyToFunction(obj, getFnName, Getter, gpsFix);
        }
    }


private:

    static jerry_value_t Getter(const jerry_call_info_t *callInfo,
                                const jerry_value_t      argv[],
                                const jerry_length_t     argc)
    {
        jerry_value_t retVal = jerry_undefined();

        Fix3DPlus *gpsFix = (Fix3DPlus *)JerryScript::GetNativePointer(callInfo->function);

        if (argc != 0)
        {
            retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Invalid function arguments");
        }
        else if (gpsFix == nullptr)
        {
            retVal = jerry_throw_sz(JERRY_ERROR_REFERENCE, "Failed to retrieve object");
        }
        else
        {
            string fnName = JerryScript::GetInternalPropertyAsString(callInfo->function, "name");
            string fieldName = fnName.substr(3);

                 if (fieldName == "Latitude")       { retVal = jerry_number(gpsFix->latDegMillionths);            }
            else if (fieldName == "Longitude")      { retVal = jerry_number(gpsFix->lngDegMillionths);            }
            else if (fieldName == "AltitudeMeters") { retVal = jerry_number(gpsFix->altitudeM);           }
            else if (fieldName == "AltitudeFeet")   { retVal = jerry_number(gpsFix->altitudeFt); }
            else
            {
                retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Unknown getter function");
            }
        }

        return retVal;
    }
};
