#pragma once

#include <string>
using namespace std;

#include "GPS.h"


class JSProxy_GPS
{
public:

    static void Proxy(jerry_value_t obj, Fix3DPlus *gpsFix)
    {
        vector<const char *> fieldNameList = {
            "Year",
            "Month",
            "Day",
            "Hour",
            "Minute",
            "Second",
            "Millisecond",
            "LatDeg",
            "LatMin",
            "LatSec",
            "LatDegMillionths",
            "LngDeg",
            "LngMin",
            "LngSec",
            "LngDegMillionths",
            "Grid6",
            "AltitudeFeet",
            "AltitudeMeters",
            "SpeedMPH",
            "SpeedKPH",
            "CourseDegrees",
        };

        for (const auto &fieldName : fieldNameList)
        {
            string getFnName = string{"Get"} + fieldName;
            JerryScript::SetPropertyToJerryNativeFunction(obj, getFnName, Getter, gpsFix);
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

                 if (fieldName == "Year")             { retVal = jerry_number(gpsFix->year);                      }
            else if (fieldName == "Month")            { retVal = jerry_number(gpsFix->month);                     }
            else if (fieldName == "Day")              { retVal = jerry_number(gpsFix->day);                       }
            else if (fieldName == "Hour")             { retVal = jerry_number(gpsFix->hour);                      }
            else if (fieldName == "Minute")           { retVal = jerry_number(gpsFix->minute);                    }
            else if (fieldName == "Second")           { retVal = jerry_number(gpsFix->second);                    }
            else if (fieldName == "Millisecond")      { retVal = jerry_number(gpsFix->millisecond);               }
            else if (fieldName == "LatDeg")           { retVal = jerry_number(gpsFix->latDeg);                    }
            else if (fieldName == "LatMin")           { retVal = jerry_number(gpsFix->latMin);                    }
            else if (fieldName == "LatSec")           { retVal = jerry_number(gpsFix->latSec);                    }
            else if (fieldName == "LatDegMillionths") { retVal = jerry_number(gpsFix->latDegMillionths);          }
            else if (fieldName == "LngDeg")           { retVal = jerry_number(gpsFix->lngDeg);                    }
            else if (fieldName == "LngMin")           { retVal = jerry_number(gpsFix->lngMin);                    }
            else if (fieldName == "LngSec")           { retVal = jerry_number(gpsFix->lngSec);                    }
            else if (fieldName == "LngDegMillionths") { retVal = jerry_number(gpsFix->lngDegMillionths);          }
            else if (fieldName == "Grid6")            { retVal = jerry_string_sz(gpsFix->maidenheadGrid.c_str()); }
            else if (fieldName == "AltitudeFeet")     { retVal = jerry_number(gpsFix->altitudeFt);                }
            else if (fieldName == "AltitudeMeters")   { retVal = jerry_number(gpsFix->altitudeM);                 }
            else if (fieldName == "SpeedMPH")         { retVal = jerry_number(gpsFix->speedMph);                  }
            else if (fieldName == "SpeedKPH")         { retVal = jerry_number(gpsFix->speedKph);                  }
            else if (fieldName == "CourseDegrees")    { retVal = jerry_number(gpsFix->courseDegrees);             }
        }

        return retVal;
    }
};
