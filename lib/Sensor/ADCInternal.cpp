#include "ADCInternal.h"
#include "Log.h"
#include "Timeline.h"

#include "hardware/adc.h"



void ADC::Init()
{
    Timeline::Global().Event("ADC::Init");

    adc_init();
}

void ADC::SetupShell()
{
    Timeline::Global().Event("ADC::SetupShell");

    Shell::AddCommand("adc.test", [](vector<string> argList) {
        Test();
    }, { .argCount = 0, .help = "" });
}














