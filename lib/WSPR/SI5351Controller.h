#pragma once

#include "si5351.h"
#include "Shell.h"
#include "Utl.h"


class SI5351Controller
{
    static const uint32_t WSPR_DIAL_FREQ = 14'095'600ULL;
    static const uint32_t WSPR_FREQ_CENTER_OFFSET = 1'500ULL;
    static const uint32_t DEFAULT_FREQUENCY_HZ = WSPR_DIAL_FREQ + WSPR_FREQ_CENTER_OFFSET;
    static const int32_t  DEFAULT_CORRECTION   = 0;
public:

    static void On()
    {
        radio_.init(SI5351_CRYSTAL_LOAD_8PF, 0, corr_);
        SetCorrection(corr_);
        radio_.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
        radio_.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
        SetFrequency(freq_);
        radio_.output_enable(SI5351_CLK0, 1);
    }

    static void Off()
    {
        radio_.output_enable(SI5351_CLK0, 0);
    }

    static void SetCorrection(int32_t corr)
    {
        corr_ = corr;

        radio_.set_correction(corr_, SI5351_PLL_INPUT_XO);
    }

    static void SetFrequency(uint32_t freq)
    {
        freq_ = freq;

        radio_.set_freq(freq_ * SI5351_FREQ_MULT, SI5351_CLK0);
    }

    static void Sweep()
    {
        On();

        long steps = 100;
        unsigned long  startFreq = 14'090'000;
        unsigned long  stopFreq  = 14'100'000;

        unsigned long freqstep = (stopFreq - startFreq) / steps;

        Log("Going from ", Commas(startFreq), " to ", Commas(stopFreq), " in ", Commas(steps), " steps of ", Commas(freqstep));

        for(int i = 0; i < (steps + 1); i++ )
        {
            unsigned long freq = startFreq + (freqstep * i);
            radio_.set_freq(freq * SI5351_FREQ_MULT, SI5351_CLK0);

            PAL.Delay(50);
        }

        Off();

        Log("Done");
    }

    static void SetupShell(string prefix)
    {
        Shell::AddCommand(prefix + ".sweep", [](vector<string> argList){
            Sweep();
        }, { .argCount = 0, .help = "si5351 sweep"});

        Shell::AddCommand(prefix + ".on", [](vector<string> argList){
            On();
        }, { .argCount = 0, .help = "si5351 output enable"});

        Shell::AddCommand(prefix + ".off", [](vector<string> argList){
            Off();
        }, { .argCount = 0, .help = "si5351 output disable"});

        Shell::AddCommand(prefix + ".freq", [](vector<string> argList){
            string freqStr = argList[0];

            uint32_t freq = 14'095'600ULL;
            if (freqStr != "wspr")
            {
                freq = atof(argList[0].c_str()) * 1'000'000;
            }

            Log("Setting frequency to ", Commas(freq));
            
            SetFrequency(freq);
        }, { .argCount = 1, .help = "si5351 set freq <MHz_float>"});

        Shell::AddCommand(prefix + ".corr", [](vector<string> argList){
            int32_t corr = atoi(argList[0].c_str());

            Log("Setting correction to ", Commas(corr));

            SetCorrection(corr);
        }, { .argCount = 1, .help = "si5351 set freq <MHz_float>"});
    }

private:
    inline static Si5351 radio_;
    inline static int32_t corr_ = DEFAULT_CORRECTION;
    inline static uint32_t freq_ = DEFAULT_FREQUENCY_HZ;
};

