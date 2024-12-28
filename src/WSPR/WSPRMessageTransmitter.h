#ifndef __WSPR_MESSAGE_TRANSMITTER_H__
#define __WSPR_MESSAGE_TRANSMITTER_H__


#include "PAL.h"
#include "si5351.h"
#include "Timeline.h"
#include "WSPRMessage.h"
#include "WSPREncoder.h"

class WSPRMessageTransmitter
{
public:

    static const uint32_t WSPR_DEFAULT_FREQ = 14'097'060ULL;  // 20 meter band, lane 2
    
    static const uint8_t  WSPR_SYMBOL_COUNT               = 162;
    static const uint16_t WSPR_TONE_SPACING_HUNDREDTHS_HZ = 146;  // 1.4648 Hz
    static const uint32_t WSPR_DELAY_US                   = 682'687;
    
public:

    void SetFrequency(uint32_t freq)
    {
        frequency_ = freq;

        if (on_)
        {
            radio_.set_freq(frequency_ * 100, SI5351_CLK0);
        }
    }
    
    void SetCorrection(int32_t correction)
    {
        correction_ = correction;

        if (on_)
        {
            radio_.set_correction(correction_, SI5351_PLL_INPUT_XO);
        }
    }
    
    void SetCallbackOnTxStart(function<void()> fn)
    {
        fnOnTxStart_ = fn;
    }
    
    void SetCallbackOnBitChange(function<void()> fn)
    {
        fnOnBitChange_ = fn;
    }
    
    void SetCallbackOnTxEnd(function<void()> fn)
    {
        fnOnTxEnd_ = fn;
    }
    
    void RadioOn()
    {
        RadioOff();
        
        // Apply calibration values
        // - param 1 - assuming 10pF load capacitence
            // notably didn't see difference at 10MHz testing between 8 and 10pF
        // - param 2 - assuming 25MHz crystal, 0 means this
        // - param 3 - tunable calibration value
        // radio_.init(SI5351_CRYSTAL_LOAD_10PF, 0, correction_);
        radio_.init(SI5351_CRYSTAL_LOAD_10PF, 26'000'000, correction_);
        
        // Tune to freq
        radio_.set_freq(frequency_ * 100, SI5351_CLK0);

        // First clock used to set the frequency
        radio_.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
        radio_.set_clock_pwr(SI5351_CLK0, 1);
        radio_.output_enable(SI5351_CLK0, 1);

        // Fan out and invert the first clock signal for a
        // 180-degree phase shift on second clock
        radio_.set_clock_fanout(SI5351_FANOUT_MS, 1);
        radio_.set_clock_source(SI5351_CLK1, SI5351_CLK_SRC_MS0);
        radio_.set_clock_invert(SI5351_CLK1, 1);

        radio_.drive_strength(SI5351_CLK1, SI5351_DRIVE_8MA);
        radio_.set_clock_pwr(SI5351_CLK1, 1);
        radio_.output_enable(SI5351_CLK1, 1);

        on_ = true;
    }

    void Send(const WSPRMessage &msg)
    {
        Timeline::Global().Event("send start");

        fnOnTxStart_();

        // wow this is 34ms
        wsprEncoder_.EncodeType1(msg.GetCallsign(),
                                 msg.GetGrid(),
                                 msg.GetPowerDbm());

        Timeline::Global().Event("msg encoded");

        uint64_t timeStart = 0;
        uint64_t timeNow   = 0;
        uint64_t timeDiff  = 0;
        
        // Clock out the bits
        for(uint8_t i = 0; i < WSPR_SYMBOL_COUNT; i++)
        {
            timeStart = PAL.Micros();

            // Allow calling code to do something here
            fnOnBitChange_();

            // Determine which bit to send next
            uint32_t freqInHundrethds = 
                (frequency_ * 100) + 
                (wsprEncoder_.GetToneValForSymbol(i) * WSPR_TONE_SPACING_HUNDREDTHS_HZ) - 
                (2 * WSPR_TONE_SPACING_HUNDREDTHS_HZ);

            // Actually change the frequency to indicate the bit value
            radio_.set_freq(freqInHundrethds, SI5351_CLK0);

            // this moment is notionally when the bit starts
            timeNow = PAL.Micros();
            timeDiff = timeNow - timeStart;
            Timeline::Global().Event(i == 0 ? "bit first" : "bit marker");

            // wait a while less than full, knowing the next bit will be delayed also, during
            // which time this bit continues to be sent
            PAL.DelayBusyUs(WSPR_DELAY_US - timeDiff);
        }

        // except we have to capture some extra time for the final bit, which
        // doesn't get the benefit of a following bit delay
        PAL.DelayBusyUs(timeDiff);
        Timeline::Global().Event("bit last");

        // Allow calling code to do something here
        fnOnTxEnd_();
    }
    
    void RadioOff()
    {
        // Disable the clock and cut power for all 3 channels
        
        radio_.set_clock_pwr(SI5351_CLK0, 0);
        radio_.output_enable(SI5351_CLK0, 0);
        
        radio_.output_enable(SI5351_CLK1, 0);
        radio_.set_clock_pwr(SI5351_CLK1, 0);
        
        radio_.output_enable(SI5351_CLK2, 0);
        radio_.set_clock_pwr(SI5351_CLK2, 0);

        on_ = false;
    }

    void SetDrive(si5351_drive pwr)
    {
        radio_.drive_strength(SI5351_CLK0, pwr);
        radio_.drive_strength(SI5351_CLK1, pwr);
    }
    void SetDrive2() { SetDrive(SI5351_DRIVE_2MA); }
    void SetDrive4() { SetDrive(SI5351_DRIVE_4MA); }
    void SetDrive6() { SetDrive(SI5351_DRIVE_6MA); }
    void SetDrive8() { SetDrive(SI5351_DRIVE_8MA); }
    
    
private:

    Si5351 radio_;

    bool on_ = false;
    
    WSPREncoder wsprEncoder_;
    
    uint32_t frequency_ = WSPR_DEFAULT_FREQ;
    int32_t correction_ = 0;
    
    function<void()> fnOnTxStart_   = []{};
    function<void()> fnOnBitChange_ = []{};
    function<void()> fnOnTxEnd_     = []{};

};





#endif  // __WSPR_MESSAGE_TRANSMITTER_H__








