#pragma once

#include "JerryScriptUtl.h"


class GetterSetter
{
public:

    GetterSetter(string name, double val1, double val2)
    {
        name_ = name;
        val1_ = val1;
        val2_ = val2;
    }

    double Get(string field)
    {
        double retVal = 0;

        printf("%s.Get(%s)", name_.c_str(), field.c_str());

        if (field == "val1")
        {
            retVal = val1_;

            printf(" --> %f\n", retVal);
        }
        else if (field == "val2")
        {
            retVal = val2_;

            printf(" --> %f\n", retVal);
        }
        else
        {
            printf(" --> err \n", retVal);
        }

        return retVal;
    }

    void Set(string field, double val)
    {
        printf("%s.Set(%s, %f)", name_.c_str(), field.c_str(), val);

        if (field == "val1")
        {
            printf(" %f --> %f\n", val1_, val);

            val1_ = val;
        }
        else if (field == "val2")
        {
            printf(" %f --> %f\n", val2_, val);

            val2_ = val;
        }
        else
        {
            printf(" --> err\n");
        }
    }


private:

    string name_;
    double val1_;
    double val2_;
};


class GetterSetter_JSProxy
{
public:

    static void Proxy(jerry_value_t obj, GetterSetter *gs)
    {
        JerryScript::SetPropertyDescriptorGetterSetter(obj, "val1", Getter, Setter, gs);
        JerryScript::SetPropertyDescriptorGetterSetter(obj, "val2", Getter, Setter, gs);
    }

    static jerry_value_t Getter(const jerry_call_info_t *callInfo,
                                const jerry_value_t      argv[],
                                const jerry_length_t     argc)
    {
        jerry_value_t retVal;

        GetterSetter *gs = (GetterSetter *)JerryScript::GetNativePointer(callInfo->function);
        if (gs)
        {
            string name = JerryScript::GetInternalPropertyAsString(callInfo->function, "name");

            if (name == "val1")
            {
                double val = gs->Get("val1");
                retVal = jerry_number(val);
            }
            else if (name == "val2")
            {
                double val = gs->Get("val2");
                retVal = jerry_number(val);
            }
            else
            {
                retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Failed to identify property");
            }
        }
        else
        {
            retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Failed to retrieve object");
        }

        return retVal;
    }

    static jerry_value_t Setter(const jerry_call_info_t *callInfo,
                                const jerry_value_t      argv[],
                                const jerry_length_t     argc)
    {
        jerry_value_t retVal;

        if (argc == 1 && jerry_value_is_number(argv[0]))
        {
            GetterSetter *gs = (GetterSetter *)jerry_object_get_native_ptr(callInfo->function, nullptr);
            if (gs)
            {
                double val = jerry_value_as_number(argv[0]);

                string name = JerryScript::GetInternalPropertyAsString(callInfo->function, "name");

                if (name == "val1")
                {
                    gs->Set("val1", val);
                    retVal = jerry_undefined();
                }
                else if (name == "val2")
                {
                    gs->Set("val2", val);
                    retVal = jerry_undefined();
                }
                else
                {
                    retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Failed to identify property");
                }
            }
            else
            {
                retVal = jerry_throw_sz(JERRY_ERROR_TYPE, "Failed to retrieve object");
            }
        }

        return retVal;
    }
};

