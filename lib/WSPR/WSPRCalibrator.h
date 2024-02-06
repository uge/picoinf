#pragma once

#include <functional>
using namespace std;

#include "Pin.h"
#include "Shell.h"
#include "Timeline.h"


// actually, can this just be generallized into a calibrator?  (yes)
// change the 1pps to be any freq

/*



Peripherals I will use:
- TIMER (shorts = compare->stop or clear - not useful)
- GPIO (signals)
- GPIOTE (no shorts)
- EGU (no shorts)
- PPI (no shorts)




Goals
- determine output frequency of si5351
- determine my own clock speed compared to gps



Implement - determine si5351 freq
- 2 pins, gps signal and si5351 signal
- use successive rising edge of 1pps signal from gps to bound measurement
- count si5351 pulses


Pulse counting
- set up a timer as a counter
- use PPI to tie GPIOTE to TIMERp incr

GPS bounding
- use PPI to tie GPIOTE to TIMERp capture

Observation
- use GPIO interrupt to call my code to go look

*/

#include "NordicTIMER.h"
#include "NordicGPIOTE.h"
#include "NordicPPI.h"
#include "NordicEGU.h"


class WSPRCalibrator
{
public:

    // pins are expected to be set up already
    struct Configuration
    {
        Pin pinClockReference;
        function<void()> fnEnableClockReference = []{};
        function<void()> fnDisableClockReference = []{};

        Pin pinClockCalibrate;
        function<void()> fnEnableClockCalibrate = []{};
        function<void()> fnDisableClockCalibrate = []{};
    };

public:

    void SetConfiguration(Configuration cfg)
    {
        cfg_ = cfg;
    }

    void Start()
    {
        cfg_.fnEnableClockReference();
        cfg_.fnEnableClockCalibrate();

        StartPulseCounter();
        StartMonitorPulseCounterWithReferenceClock();
        StartMonitorPulseCounterWithSystemClock();
    }

    void Stop()
    {
        StopMonitorPulseCounterWithSystemClock();
        StopMonitorPulseCounterWithReferenceClock();
        StopPulseCounter();

        cfg_.fnDisableClockCalibrate();
        cfg_.fnDisableClockReference();
    }

    void SetupShell(string prefix)
    {
        // calibrator
        Shell::AddCommand(prefix + ".start", [this](vector<string> argList){
            Log("Starting");
            Start();
        }, { .argCount = 0, .help = ""});

        Shell::AddCommand(prefix + ".stop", [this](vector<string> argList){
            Log("Stopping");
            Stop();
        }, { .argCount = 0, .help = ""});

        // timer
        Shell::AddCommand(prefix + ".t.start", [this](vector<string> argList){
            Log("Timer Start");
            timerPulseCount_.Enable();
        }, { .argCount = 0, .help = ""});

        Shell::AddCommand(prefix + ".t.stop", [this](vector<string> argList){
            Log("Timer Stop");
            timerPulseCount_.Disable();
        }, { .argCount = 0, .help = ""});

        Shell::AddCommand(prefix + ".t.count", [this](vector<string> argList){
            Log("Timer Count");
            timerPulseCount_.Count();
        }, { .argCount = 0, .help = ""});

        Shell::AddCommand(prefix + ".t.clear", [this](vector<string> argList){
            Log("Timer Clearing");
            timerPulseCount_.Clear();
        }, { .argCount = 0, .help = ""});

        // gpiote channel
        Shell::AddCommand(prefix + ".gc.enable", [this](vector<string> argList){
            gcPulseCount_.Enable();
        }, { .argCount = 0, .help = ""});

        Shell::AddCommand(prefix + ".gc.disable", [this](vector<string> argList){
            gcPulseCount_.Disable();
        }, { .argCount = 0, .help = ""});

        // ppi channel
        Shell::AddCommand(prefix + ".p.enable", [this](vector<string> argList){
            ppiPulseCount_.Enable();
        }, { .argCount = 0, .help = ""});

        Shell::AddCommand(prefix + ".p.disable", [this](vector<string> argList){
            ppiPulseCount_.Disable();
        }, { .argCount = 0, .help = ""});
    }

private:

    // Count rising pulses on external pin (si5351)
    void StartPulseCounter()
    {
        // Set up GPIOTE to route pulses to PPI
        gcPulseCount_.Borrow();
        gcPulseCount_.InitInput(cfg_.pinClockCalibrate.GetPort(),
                                cfg_.pinClockCalibrate.GetPin(),
                                NordicGPIOTEChannel::Pull::None,
                                NordicGPIOTEChannel::Trigger::LowToHigh);
        gcPulseCount_.Enable();

        // Set up TIMER (as counter) to increment with pulses routed to it via PPI
        // Also set up two channels for use by other logic
        timerPulseCount_.Borrow();
        timerPulseCount_.Init(NordicTIMER::Mode::Counter);
        auto [ok1, ccIdx1] = timerPulseCount_.BorrowCC();
        timerCc1PulseCount_ = ccIdx1;
        auto [ok2, ccIdx2] = timerPulseCount_.BorrowCC();
        timerCc2PulseCount_ = ccIdx2;
        timerPulseCount_.Clear();
        timerPulseCount_.Enable();

        // Set up PPI to route pulses to counter
        ppiPulseCount_.Borrow();
        ppiPulseCount_.SetAddrEvent(gcPulseCount_.GetAddrEventIn());
        ppiPulseCount_.SetAddrTaskPrimary(timerPulseCount_.GetAddrTask(NordicTIMER::Task::Count));
        ppiPulseCount_.Enable();
    }

    void StopPulseCounter()
    {
        ppiPulseCount_.Release();
        timerPulseCount_.Release();
        gcPulseCount_.Release();
    }

    // Use external signal (GPS) to count pulses per second
    void StartMonitorPulseCounterWithReferenceClock()
    {
        // Set up GPIOTE to monitor reference clock rising edges
        gcReferenceClock_.Borrow();
        gcReferenceClock_.InitInput(cfg_.pinClockReference.GetPort(),
                                    cfg_.pinClockReference.GetPin(),
                                    NordicGPIOTEChannel::Pull::None,
                                    NordicGPIOTEChannel::Trigger::LowToHigh);
        gcReferenceClock_.Enable();

        // Set up PPI so that when the reference clock ticks:
        // - capture the pulse counter
        ppiReferenceClock_.Borrow();
        ppiReferenceClock_.SetAddrEvent(gcReferenceClock_.GetAddrEventIn());
        ppiReferenceClock_.SetAddrTaskPrimary(timerPulseCount_.GetAddrTask(NordicTIMER::Task::Capture, timerCc1PulseCount_));
        ppiReferenceClock_.Enable();

        // handle reference clock tick
        gcReferenceClock_.SetInterruptCallback([=, this]{
            static Average<uint32_t> avgrFreq(AVG_WINDOW);
            static Average<int32_t>  avgrPpb(AVG_WINDOW);
            static uint32_t valLast = 0;

            uint32_t val = timerPulseCount_.GetCCValue(timerCc1PulseCount_);
            uint32_t freq = val - valLast;
            valLast = val;

            if (valLast != 0 && freq != 0)
            {
                LogNL();
                Log("Reference Clock");
                OnMeasurement(freq, avgrFreq, avgrPpb);
            }
        });
        gcReferenceClock_.EnableInterrupts();
    }

    void StopMonitorPulseCounterWithReferenceClock()
    {
        gcReferenceClock_.Release();
        ppiReferenceClock_.Release();
    }

    // Use system clock to count pulses per second
    void StartMonitorPulseCounterWithSystemClock()
    {
        // set up to tick at max speed (16MHz)
        // set when counter hits 1 second:
        // - capture the pulse count
        // - reset the count back to zero (so we get running for next second)
        // - fire callback to go look at the capture

        static const uint32_t TICKS_16M = 16'000'000ULL;

        timerSystemClock_.Borrow();
        timerSystemClock_.Init(NordicTIMER::Mode::Timer);
        auto [ok, ccIdx] = timerSystemClock_.BorrowCC();
        timerCcSystemClock_ = ccIdx;
        timerSystemClock_.Clear();
        timerSystemClock_.SetCCValue(timerCcSystemClock_, TICKS_16M);
        timerSystemClock_.SetCompareShort(timerCcSystemClock_, NordicTIMER::OnCompareAction::Clear);
        timerSystemClock_.SetCCInterruptCallback(timerCcSystemClock_, [this]{
            static Average<uint32_t> avgrFreq(AVG_WINDOW);
            static Average<int32_t>  avgrPpb(AVG_WINDOW);
            static uint32_t valLast = 0;

            uint32_t val = timerPulseCount_.GetCCValue(timerCc2PulseCount_);
            uint32_t freq = val - valLast;
            valLast = val;

            if (valLast != 0 && freq != 0)
            {
                LogNL();
                Log("System Clock");
                OnMeasurement(freq, avgrFreq, avgrPpb);
            }
        });
        timerSystemClock_.EnableCCInterrupt(timerCcSystemClock_);
        timerSystemClock_.Enable();


        // Set up PPI so that when the system clock ticks:
        // - capture the pulse counter
        ppiSystemClock_.Borrow();
        ppiSystemClock_.SetAddrEvent(timerSystemClock_.GetAddrEventCompare(timerCcSystemClock_));
        ppiSystemClock_.SetAddrTaskPrimary(timerPulseCount_.GetAddrTask(NordicTIMER::Task::Capture, timerCc2PulseCount_));
        ppiSystemClock_.Enable();
    }

    void StopMonitorPulseCounterWithSystemClock()
    {
        timerSystemClock_.Release();
        ppiSystemClock_.Release();
    }

    static const uint8_t AVG_WINDOW = 5;

    void OnMeasurement(uint32_t freq, Average<uint32_t> &avgrFreq, Average<int32_t> &avgrPpb)
    {
        uint32_t avgFreq = avgrFreq.AddSample(freq);

        Log("Tick count: ", Commas(freq), ", avg: ", Commas(avgFreq));

        // guess the attempted frequency, use integer math to snap to 10k increments
        uint32_t freqTargetGuess = freq;
        freqTargetGuess = round((double)freqTargetGuess / 10'000);
        freqTargetGuess *= 10'000;

        // calculate how far off target freq you are
        int32_t freqOffset = freq - freqTargetGuess;

        // convert to ppb
        int32_t ppb = round(freqOffset * (1'000'000'000.0 / freqTargetGuess));
        int32_t avgPpb = avgrPpb.AddSample(ppb);

        Log("Freq Guess: ", Commas(freqTargetGuess), ", diff: ", Commas(freqOffset), ", ppb: ", Commas(ppb), ", avbPpb: ", Commas(avgPpb));
    }



private:

    Configuration cfg_;

    // Pulse count
    NordicTIMER         timerPulseCount_;
    uint8_t             timerCc1PulseCount_;
    uint8_t             timerCc2PulseCount_;
    NordicGPIOTEChannel gcPulseCount_;
    NordicPPIChannel    ppiPulseCount_;

    // Reference clock
    NordicGPIOTEChannel gcReferenceClock_;
    NordicPPIChannel    ppiReferenceClock_;

    // System clock
    NordicTIMER      timerSystemClock_;
    uint8_t          timerCcSystemClock_;
    NordicPPIChannel ppiSystemClock_;

};
