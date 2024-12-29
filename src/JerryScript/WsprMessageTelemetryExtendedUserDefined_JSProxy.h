#include "WsprMessageTelemetryExtendedUserDefined.h"


class WsprMessageTelemetryExtendedUserDefined_JSProxy
{
private:
    using Msg = WsprMessageTelemetryExtendedUserDefined<29>;

public:

    static void Proxy(jerry_value_t obj, Msg *msg)
    {
        // TODO get this list from the message itself, should be dynamic
        vector<string> fieldNameList = {
            "AltitudeFeet",
        };

        // create getter/setter for each field
        for (const auto &fieldName : fieldNameList)
        {
            string getFnName = string{"Get"} + fieldName;
            JerryScript::SetPropertyToFunction(obj, getFnName, Getter, msg);

            string setFnName = string{"Set"} + fieldName;
            JerryScript::SetPropertyToFunction(obj, setFnName, Setter, msg);
        }
    }


private:

    static jerry_value_t Getter(const jerry_call_info_t *callInfo,
                                const jerry_value_t      argv[],
                                const jerry_length_t     argc)
    {
        jerry_value_t retVal = jerry_undefined();

        if (argc == 0)
        {
            Msg *msg = nullptr;
            string fieldName;

            if (CheckArgs(retVal, callInfo, "Get", msg, fieldName))
            {
                printf("Yay, valid, Get of %s\n", fieldName.c_str());

                retVal = jerry_number(msg->Get(fieldName.c_str()));
            }
        }
        else
        {
            retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Invalid function arguments");
        }

        return retVal;
    }

    static jerry_value_t Setter(const jerry_call_info_t *callInfo,
                                const jerry_value_t      argv[],
                                const jerry_length_t     argc)
    {
        jerry_value_t retVal = jerry_undefined();

        if (argc == 1 && jerry_value_is_number(argv[0]))
        {
            Msg *msg = nullptr;
            string fieldName;

            if (CheckArgs(retVal, callInfo, "Set", msg, fieldName))
            {
                double val = jerry_value_as_number(argv[0]);

                printf("Yay, valid, Set of %s to %f\n", fieldName.c_str(), val);
                msg->Set(fieldName.c_str(), val);
            }
        }
        else
        {
            retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Invalid function arguments");
        }

        return retVal;
    }

private:

    // true if ok, false if not.
    //
    // if true
    // - msgRet is set to the message pointer
    // - fieldNameRet is set to the fieldName being accessed
    // - retValJerry unchanged
    //
    // if false
    // - the retValJerry has been set to a value that should be returned and could be an exception.
    static bool CheckArgs(jerry_value_t           &retValJerry,
                          const jerry_call_info_t *callInfo,
                          string                    prefix,
                          Msg                     *&msgRet,
                          string                   &fieldNameRet)
    {
        bool retVal = false;

        Msg *msg = (Msg *)JerryScript::GetNativePointer(callInfo->function);
        if (msg)
        {
            string name = JerryScript::GetInternalPropertyAsString(callInfo->function, "name");
            string fieldName;

            if (name.starts_with(prefix))
            {
                string fieldName = name.substr(prefix.length());

                // TODO check suffix is in the defined set of fields
                if (fieldName == "AltitudeFeet")
                {
                    retVal = true;

                    msgRet = msg;
                    fieldNameRet = fieldName;
                }
                else
                {
                    retValJerry = jerry_throw_sz(JERRY_ERROR_TYPE, "Unknown getter/setter field");
                }
            }
            else
            {
                retValJerry = jerry_throw_sz(JERRY_ERROR_TYPE, "Invalid getter/setter function");
            }
        }
        else
        {
            retValJerry = jerry_throw_sz(JERRY_ERROR_REFERENCE, "Failed to retrieve object");
        }

        return retVal;
    }
};