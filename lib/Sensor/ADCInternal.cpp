#include "ADCInternal.h"
#include "Log.h"
#include "PAL.h"
#include "TempSensorInternal.h"
#include "Timeline.h"

#include "hardware/adc.h"
#include "pico/cyw43_arch.h"

#include "StrictMode.h"


    /////////////////////////////////////////////////////////////////
    // Use normal ADC channels as instantiated objects
    /////////////////////////////////////////////////////////////////

ADC::ADC(uint8_t pin)
: pin_(pin)
{
    // Nothing to do
}

uint16_t ADC::GetMilliVolts()
{
    return GetMilliVolts(pin_);
}


    /////////////////////////////////////////////////////////////////
    // Use ADC class to static read from a given pin
    /////////////////////////////////////////////////////////////////

// raw 12-bit value
uint16_t ADC::Read(uint8_t pin)
{
    adc_gpio_init(pin);

    return InputRead(GetInputFromPin(pin));
}

// raw converted to mv, assuming 3.3v reference
uint16_t ADC::GetMilliVolts(uint8_t pin)
{
    uint16_t rawVal = Read(pin);

    // 12-bit, so out of 4096
    // referencing a 3.3v internal voltage
    uint16_t retVal = (uint16_t)(((uint32_t)rawVal * 3300) / 4096);

    return retVal;
}


    /////////////////////////////////////////////////////////////////
    // Use ADC class to static read from special pins
    /////////////////////////////////////////////////////////////////

// adapted from pico-examples read_vsys/power_status.c
uint16_t ADC::GetMilliVoltsVCC()
{
    uint16_t retVal = 0;

    if (PAL.IsPicoW())
    {
        cyw43_thread_enter();
        // Make sure cyw43 is awake
        cyw43_arch_gpio_get(CYW43_WL_GPIO_VBUS_PIN);
    }

    retVal = GetMilliVolts(PICO_VSYS_PIN) * 3;
    
    if (PAL.IsPicoW())
    {
        cyw43_thread_exit();
    }

    return retVal;
}

uint16_t ADC::ReadTemperature()
{
    return InputRead(4);
}


    /////////////////////////////////////////////////////////////////
    // Test
    /////////////////////////////////////////////////////////////////

void ADC::Test()
{
    static Timeline t;

    // The first 3 are normal ADC inputs
    for (uint8_t i = 26; i <= 28; ++i)
    {
        ADC adc(i);

        uint16_t mv = adc.GetMilliVolts();

        Log("Pin ", i, " (input ", GetInputFromPin(i), "): ADC: ", mv, " mV");
    }

    // Next is VCC
    Log("Pin ", PICO_VSYS_PIN, " (input ", GetInputFromPin(PICO_VSYS_PIN), "): VCC: ", GetMilliVoltsVCC(), " mV");

    // Next is Temperature
    Log("Pin -- (input 4): (temperature)");
    Log("  TempC: ", TempSensorInternal::GetTempC());
    Log("  TempF: ", TempSensorInternal::GetTempF());

    // Show some timing    
    t.Reset();
    t.Event("Test");
    ADC::GetMilliVolts(26);
    t.Event("ADC 26");
    ADC::GetMilliVolts(27);
    t.Event("ADC 27");
    ADC::GetMilliVolts(28);
    t.Event("ADC 28");
    GetMilliVoltsVCC();
    t.Event("ADC VCC");
    TempSensorInternal::GetTempC();
    t.Event("ADC TempC");
    t.Report();
}


    /////////////////////////////////////////////////////////////////
    // Hooks
    /////////////////////////////////////////////////////////////////

void ADC::Init()
{
    Timeline::Global().Event("ADC::Init");

    adc_init();
    adc_set_temp_sensor_enabled(true);
}

void ADC::SetupShell()
{
    Timeline::Global().Event("ADC::SetupShell");

    Shell::AddCommand("adc.test", [](vector<string> argList) {
        Test();
    }, { .argCount = 0, .help = "Test all ADC functions" });

    Shell::AddCommand("adc.get", [](vector<string> argList) {
        Log(GetMilliVolts((uint8_t)atoi(argList[0].c_str())));
    }, { .argCount = 1, .help = "ADC GetMilliVolts pin x" });

    Shell::AddCommand("adc.vcc", [](vector<string> argList) {
        Log(GetMilliVoltsVCC());
    }, { .argCount = 0, .help = "ADC GetMilliVoltsVCC" });

    Shell::AddCommand("sys.temp", [](vector<string> argList) {
        Log("TempF: ", TempSensorInternal::GetTempF());
        Log("TempC: ", TempSensorInternal::GetTempC());
    }, { .argCount = 0, .help = "Get temperature" });
}


    /////////////////////////////////////////////////////////////////
    // Implementation
    /////////////////////////////////////////////////////////////////

uint8_t ADC::GetInputFromPin(uint8_t pin)
{
    uint8_t retVal = 0;

    if (pin == 26 || pin == 27 || pin == 28 || pin == 29)
    {
        retVal = pin - PICO_FIRST_ADC_PIN;
    }

    return retVal;
}

// adapted from power_status.c in pico-examples read_vsys.
// discarding samples was originally only part of reading 
// vcc, but I'm applying it to all channels because why not.
uint16_t ADC::InputRead(uint8_t input)
{
    adc_select_input(input);

    adc_fifo_setup(true, false, 0, false, false);
    adc_run(true);

    // We seem to read low values initially - this seems to fix it
    int ignore_count = PICO_POWER_SAMPLE_COUNT;
    while (!adc_fifo_is_empty() || ignore_count-- > 0) {
        adc_fifo_get_blocking();
    }

    uint32_t rawSum = 0;
    for(int i = 0; i < PICO_POWER_SAMPLE_COUNT; i++) {
        uint16_t val = adc_fifo_get_blocking();
        rawSum += val;
    }

    adc_run(false);
    adc_fifo_drain();

    uint16_t rawAvg = (uint16_t)(rawSum / PICO_POWER_SAMPLE_COUNT);

    return rawAvg;
}

