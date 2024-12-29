#include <cstdint>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

#include "JerryScript.h"
#include "JS_I2C.h"
#include "GetterSetter.h"
#include "WsprMessageTelemetryExtendedUserDefined_JSProxy.h"



inline static string ReadFile(const string& filename) {
    string contents;
    ifstream file(filename);
    if (file.is_open())
    {
        stringstream buffer;
        buffer << file.rdbuf();
        contents = buffer.str();
    }
    return contents;
}

///////////////////////////////////////////////

struct GpsFix
{
    uint32_t speedKnots;
    int32_t  altitudeFt;
    int32_t  altitudeM;
    double   protectedVal;
};

GpsFix gpsFix = {
    .speedKnots = 17,
    .altitudeFt = 1200,
    .altitudeM = 4000,
    .protectedVal = 20,
};


static void RegisterGPS()
{
    JerryScript::UseThenFree(jerry_object(), [&](auto gps){
        JerryScript::SetGlobalPropertyNoFree("gps", gps);

        struct PropertyNumberValue
        {
            string name;
            double value;
        };

        // these are write protected
        vector<PropertyNumberValue> pvNumList = {
            { "speedKnots",     (double)gpsFix.speedKnots },
            { "altitudeFeet",   (double)gpsFix.altitudeFt },
        };

        for (const auto & pv : pvNumList)
        {
            JerryScript::SetProperty(gps, pv.name, jerry_number(pv.value));
            JerryScript::SetPropertyDescriptorReadonly(gps, pv.name);
        }

        // read/write
        JerryScript::SetProperty(gps, "altitudeMeters", jerry_number(gpsFix.altitudeM));
    });
}

static void RegisterGPS2()
{
    string script;

    auto MakeGetter = [](string name, string fieldName, double val){
        return name + ".Get" + fieldName + " = () => { return " + to_string(val) + "; };\n";
    };

    script += "let gps2 = new Object();\n";
    script += MakeGetter("gps2", "SpeedKnots", gpsFix.speedKnots);
    // script += MakeGetter("gps2", "AltitudeFeet", gpsFix.altitudeFt);
    // script += MakeGetter("gps2", "AltitudeMeters", gpsFix.altitudeM);

    // printf("GPS2: About to run script:\n");
    // printf("%s\n", script.c_str());

    // string ret = JerryScript::Run(script);

    // printf("RET: %s\n", ret.c_str());
}



GetterSetter gs1("gs1", 1, 2);
GetterSetter gs2("gs2", 3, 4);

static void RegisterGetterSetter()
{
    // try out giving access to already-constructed object properties
    // this simulates having object state already constructed, whose values remain
    // intact across invocations, but we want the fields available for set/get.
    vector<GetterSetter *> gsList = { &gs1, &gs2 };

    for (int i = 0; const auto gs : gsList)
    {
        ++i;
        string objName = "gs" + to_string(i);

        JerryScript::UseThenFree(jerry_object(), [&](auto obj){
            JerryScript::SetGlobalPropertyNoFree(objName, obj);

            GetterSetter_JSProxy::Proxy(obj, gs);
        });
    }
}

// TODO - make a dynamically-sized interface to this
// - basically, make a version that is given a buffer of n field defs
// - then you can pass it around, typelessly
// - can still have the templated types do the allocation, etc
// - I need the typeless kind though for on-the-fly reconfiguration and iteration
// - would rather not put malloc/new or whatever inside the lib
//   - so take the data from outside somehow I guess?
// - update the lib while you're in there to extend the bit count
WsprMessageTelemetryExtendedUserDefined<29> msg;

static void RegisterUserDefined()
{
    using Msg = WsprMessageTelemetryExtendedUserDefined<29>;

    // TODO this will have been done elsewhere, here for debugging
    msg.DefineField("AltitudeFeet", -10, 20, 1);

    // pretend we're doing this for a lot of messages
    vector<Msg *> msgList = { &msg };
    
    // put them all in in sequentially-named order
    for (int i = 0; auto msg : msgList)
    {
        ++i;
        string objName = "msg" + to_string(i);

        JerryScript::UseThenFree(jerry_object(), [&](auto obj){
            JerryScript::SetGlobalPropertyNoFree(objName, obj);

            WsprMessageTelemetryExtendedUserDefined_JSProxy::Proxy(obj, msg);
        });
    }
}

static void VerifyUserDefined()
{
    string fieldName = "AltitudeFeet";

    printf("Final value of %s: %f\n", fieldName.c_str(), msg.Get(fieldName.c_str()));
}

static void RegisterDelay()
{
    // register a bare function
    JerryScript::SetGlobalFunction("Delay", [](const jerry_call_info_t *callInfo,
                                               const jerry_value_t      argv[],
                                               const jerry_length_t     argc) -> jerry_value_t {
        jerry_value_t retVal = jerry_undefined();
        
        if (argc != 1 || !jerry_value_is_number(argv[0]))
        {
            retVal = jerry_throw_sz(JERRY_ERROR_SYNTAX, "Invalid function arguments");
        }
        else
        {
            int ms = (int)jerry_value_as_number(argv[0]);

            if (ms >= 0)
            {
                uint64_t timeStart = JerryScript::TimeNow();

                while (JerryScript::TimeNow() - timeStart < ms)
                {
                    // do nothing
                }
            }
            else
            {
                retVal = jerry_throw_sz(JERRY_ERROR_SYNTAX, "Invalid function arguments");
            }
        }
        
        return retVal;
    });
}


int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s <script>\n", argv[0]);
        return 1;
    }

    string script = ReadFile(argv[1]);

    printf("Script\n");
    printf("------\n");
    printf("%s\n", script.c_str());
    printf("------\n");
    printf("\n");

    printf("[Pre-Init]\n");

    string ret;
    JerryScript::UseVM([&]{
        JS_I2C::Register();
        RegisterGPS();
        RegisterGPS2();
        RegisterGetterSetter();
        RegisterUserDefined();
        RegisterDelay();

        JerryScript::EnableJerryX();

        printf("[Post-Init]\n");
        printf("\n");

        ret = JerryScript::ParseAndExecuteScript(script, 100);
    });

    if (ret != "")
    {
        printf("%s\n", ret.c_str());
    }

    printf("[Post-Cleanup]\n");
    printf("\n");

    VerifyUserDefined();

    return 0;
}
