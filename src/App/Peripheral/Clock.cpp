#include "Clock.h"
#include "KTime.h"
#include "Log.h"
#include "Pin.h"
#include "Shell.h"
#include "Timeline.h"
#include "UART.h"
#include "Utl.h"

#include "hardware/regs/intctrl.h"
#include "hardware/structs/clocks.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/scb.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/pll.h"
#include "hardware/irq.h"
#include "hardware/resets.h"
#include "hardware/sync.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "hardware/vreg.h"
#include "hardware/xosc.h"
#include "hardware/rtc.h"
#include "pico/stdlib.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
using namespace std;

#include "StrictMode.h"


/////////////////////////////////////////////////////////////////
// State
/////////////////////////////////////////////////////////////////

static bool verbose_ = false;

struct PllConfig;
static unordered_map<double, PllConfig> freq__data;


/////////////////////////////////////////////////////////////////
// Utility
/////////////////////////////////////////////////////////////////

static uint32_t GetFreqROSC()
{
    // prevent subsequent calls from having a larger frequency
    // which upsets some clock configuration logic
    static uint32_t freq = 0;

    if (freq == 0)
    {
        freq = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC) * 1'000;
    }

    return freq;
}


/////////////////////////////////////////////////////////////////
// PLL Definitions and State
/////////////////////////////////////////////////////////////////

struct PllData
{
    pll_hw_t *pll;

    // display
    string name;
    uint32_t freq;

    // data to init the pll
    uint32_t refdiv;
    uint32_t vco_freq;
    uint32_t post_div1;
    uint32_t post_div2;

    // measure
    uint32_t freqCountId;
};

// should I play with changing these values?
// p. 230-232 shows how, can save power with lower VCO freq
// at the expense of jitter, but how much jitter?
//   I care about timing bits for WSPR but that's 600ms+ apiece
//   I care about USB working.  
//     btw, can I enable/disable the pll_usb simply by detecting USB events?
//       I might be willing to do elaborate things (like monitor vbus, then enable to wait for detect)
//       because running the USB PLL all the time during flight is just pissing power for
//       USB which isn't being used.

// nominal startup state
inline static vector<PllData> pdList_ = {
    {
        .pll = pll_sys,
        .name = "pll_sys",
        .freq = 125'000'000,
        .refdiv = 1,
        .vco_freq = 1500 * MHZ,
        .post_div1 = 6,
        .post_div2 = 2,
        .freqCountId = CLOCKS_FC0_SRC_VALUE_PLL_SYS_CLKSRC_PRIMARY,
    },
    {
        .pll = pll_usb,
        .name = "pll_usb",
        .freq = 48'000'000,
        .refdiv = 1,
        .vco_freq = 1200 * MHZ,
        .post_div1 = 5,
        .post_div2 = 5,
        .freqCountId = CLOCKS_FC0_SRC_VALUE_PLL_USB_CLKSRC_PRIMARY,
    },
};


// State to capture
//
// assumptions:
// - pll_sys is always 125,000,000, source XOSC, on or off
// - pll_usb is always 48,000,000, source XOSC, on or off

struct PllState
{
    PllData pllData;
    bool on;

    void Print()
    {
        Log("PLL: ", pllData.name, ", freq: ", Commas(pllData.freq), " (", on ? "on" : "off", ")");
        Log("  refdiv    = ", Commas(pllData.refdiv));
        Log("  vco_freq  = ", Commas(pllData.vco_freq));
        Log("  post_div1 = ", pllData.post_div1);
        Log("  post_div2 = ", pllData.post_div2);
    }
};

static PllState GetPllState(pll_hw_t *pll)
{
    PllState retVal;

    for (auto &pd : pdList_)
    {
        if (pd.pll == pll)
        {
            // update state
            pd.freq      = frequency_count_khz(pd.freqCountId) * KHZ;
            pd.refdiv    = pll->cs & PLL_CS_REFDIV_BITS;
            pd.vco_freq  = pll->fbdiv_int * XOSC_KHZ * KHZ / pd.refdiv;
            pd.post_div1 = (pll->prim & PLL_PRIM_POSTDIV1_BITS) >> PLL_PRIM_POSTDIV1_LSB;
            pd.post_div2 = (pll->prim & PLL_PRIM_POSTDIV2_BITS) >> PLL_PRIM_POSTDIV2_LSB;

            // capture
            retVal.pllData = pd;
            retVal.on = pd.freq != 0;
        }
    }

    return retVal;
}


/////////////////////////////////////////////////////////////////
// Clock Definitions and State
/////////////////////////////////////////////////////////////////

struct ClockSourceData
{
    clock_index clk;
    string name;

    uint32_t freqCountId;

    struct IdData
    {
        uint8_t val;
        const char *valNameLong;
        const char *valNameShort;
    };

    vector<IdData> sourceMapPrimary;
    vector<IdData> sourceMapAux;
};

// https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#hardware_clocks
inline static vector<ClockSourceData> csdList_ = {
    {
        .clk = clk_ref,
        .name = "clk_ref",
        .freqCountId = CLOCKS_FC0_SRC_VALUE_CLK_REF,
        .sourceMapPrimary = {
            { CLOCKS_CLK_REF_CTRL_SRC_VALUE_ROSC_CLKSRC_PH,     "CLOCKS_CLK_REF_CTRL_SRC_VALUE_ROSC_CLKSRC_PH",     "ROSC" },
            { CLOCKS_CLK_REF_CTRL_SRC_VALUE_CLKSRC_CLK_REF_AUX, "CLOCKS_CLK_REF_CTRL_SRC_VALUE_CLKSRC_CLK_REF_AUX", "AUX"  },
            { CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC,        "CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC",        "XOSC" },
        },
        .sourceMapAux = {
            { CLOCKS_CLK_REF_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0,   "CLOCKS_CLK_REF_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0",   "GPIN0"   },
            { CLOCKS_CLK_REF_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1,   "CLOCKS_CLK_REF_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1",   "GPIN1"   },
            { CLOCKS_CLK_REF_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB, "CLOCKS_CLK_REF_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB", "PLL_USB" },
        },
    },
    {
        .clk = clk_sys,
        .name = "clk_sys",
        .freqCountId = CLOCKS_FC0_SRC_VALUE_CLK_SYS,
        .sourceMapPrimary = {
            { CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX, "CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX", "AUX"     },
            { CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF,            "CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF",            "CLK_REF" },
        },
        .sourceMapAux = {
            { CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, "CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS", "PLL_SYS" },
            { CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0,   "CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0",   "GPIN0"   },
            { CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1,   "CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1",   "GPIN1"   },
            { CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB, "CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB", "PLL_USB" },
            { CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_ROSC_CLKSRC,    "CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_ROSC_CLKSRC",    "ROSC"    },
            { CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,    "CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_XOSC_CLKSR",     "XOSC"    },
        },
    },
    {
        .clk = clk_gpout0,
        .name = "clk_gpout0",
        .sourceMapAux = {
            { CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, "CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS", "PLL_SYS" },
            { CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0,   "CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0",   "GPIN0"   },
            { CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1,   "CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1",   "GPIN1"   },
            { CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB, "CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB", "PLL_USB" },
            { CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_ROSC_CLKSRC,    "CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_ROSC_CLKSRC",    "ROSC"    },
            { CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,    "CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_XOSC_CLKSRC",    "XOSC"    },
            { CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_SYS,        "CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_SYS",        "CLK_SYS" },
            { CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_USB,        "CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_USB",        "CLK_USB" },
            { CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_ADC,        "CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_ADC",        "CLK_ADC" },
            { CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_RTC,        "CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_RTC",        "CLK_RTC" },
            { CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_REF,        "CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_REF",        "CLK_REF" },
        },
    },
    {
        .clk = clk_gpout1,
        .name = "clk_gpout1",
        .sourceMapAux = {
            { CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, "CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS", "PLL_SYS" },
            { CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0,   "CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0",   "GPIN0"   },
            { CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1,   "CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1",   "GPIN1"   },
            { CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB, "CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB", "PLL_USB" },
            { CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_ROSC_CLKSRC,    "CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_ROSC_CLKSRC",    "ROSC"    },
            { CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,    "CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_XOSC_CLKSRC",    "XOSC"    },
            { CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLK_SYS,        "CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLK_SYS",        "CLK_SYS" },
            { CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLK_USB,        "CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLK_USB",        "CLK_USB" },
            { CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLK_ADC,        "CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLK_ADC",        "CLK_ADC" },
            { CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLK_RTC,        "CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLK_RTC",        "CLK_RTC" },
            { CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLK_REF,        "CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLK_REF",        "CLK_REF" },
        },
    },
    {
        .clk = clk_gpout2,
        .name = "clk_gpout2",
        .sourceMapAux = {
            { CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, "CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS", "PLL_SYS" },
            { CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0,   "CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0",   "GPIN0"   },
            { CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1,   "CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1",   "GPIN1"   },
            { CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB, "CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB", "PLL_USB" },
            { CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_ROSC_CLKSRC_PH, "CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_ROSC_CLKSRC_PH", "ROSC_PH" },
            { CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,    "CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_XOSC_CLKSRC",    "XOSC"    },
            { CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLK_SYS,        "CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLK_SYS",        "CLK_SYS" },
            { CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLK_USB,        "CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLK_USB",        "CLK_USB" },
            { CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLK_ADC,        "CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLK_ADC",        "CLK_ADC" },
            { CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLK_RTC,        "CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLK_RTC",        "CLK_RTC" },
            { CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLK_REF,        "CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLK_REF",        "CLK_REF" },
        },
    },
    {
        .clk = clk_gpout3,
        .name = "clk_gpout3",
        .sourceMapAux = {
            { CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, "CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS", "PLL_SYS" },
            { CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0,   "CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0",   "GPIN0"   },
            { CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1,   "CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1",   "GPIN1"   },
            { CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB, "CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB", "PLL_USB" },
            { CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_ROSC_CLKSRC_PH, "CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_ROSC_CLKSRC_PH", "ROSC_PH" },
            { CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,    "CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_XOSC_CLKSRC",    "XOSC"    },
            { CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLK_SYS,        "CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLK_SYS",        "CLK_SYS" },
            { CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLK_USB,        "CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLK_USB",        "CLK_USB" },
            { CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLK_ADC,        "CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLK_ADC",        "CLK_ADC" },
            { CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLK_RTC,        "CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLK_RTC",        "CLK_RTC" },
            { CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLK_REF,        "CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLK_REF",        "CLK_REF" },
        },
    },
    {
        .clk = clk_peri,
        .name = "clk_peri",
        .freqCountId = CLOCKS_FC0_SRC_VALUE_CLK_PERI,
        .sourceMapAux = {
            { CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, "CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS", "PLL_SYS" },
            { CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0,   "CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0",   "GPIN0"   },
            { CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1,   "CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1",   "GPIN1"   },
            { CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB, "CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB", "PLL_USB" },
            { CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_ROSC_CLKSRC_PH, "CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_ROSC_CLKSRC_PH", "ROSC_PH" },
            { CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,    "CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_XOSC_CLKSRC",    "XOSC"    },
            { CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,        "CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS",        "CLK_SYS" },
        },
    },
    {
        .clk = clk_usb,
        .name = "clk_usb",
        .freqCountId = CLOCKS_FC0_SRC_VALUE_CLK_USB,
        .sourceMapAux = {
            { CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, "CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS", "PLL_SYS" },
            { CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0,   "CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0",   "GPIN0"   },
            { CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1,   "CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1",   "GPIN1"   },
            { CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB, "CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB", "PLL_USB" },
            { CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_ROSC_CLKSRC_PH, "CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_ROSC_CLKSRC_PH", "ROSC_PH" },
            { CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,    "CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_XOSC_CLKSRC",    "XOSC"    },
        },
    },
    {
        .clk = clk_adc,
        .name = "clk_adc",
        .freqCountId = CLOCKS_FC0_SRC_VALUE_CLK_ADC,
        .sourceMapAux = {
            { CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,  "CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS", "PLL_SYS" },
            { CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0,    "CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0",   "GPIN0"   },
            { CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1,    "CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1",   "GPIN1"   },
            { CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,  "CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB", "PLL_USB" },
            { CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_ROSC_CLKSRC_PH,  "CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_ROSC_CLKSRC_PH", "ROSC_PH" },
            { CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,     "CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_XOSC_CLKSRC",    "XOSC"    },
        },
    },
    {
        .clk = clk_rtc,
        .name = "clk_rtc",
        .freqCountId = CLOCKS_FC0_SRC_VALUE_CLK_RTC,
        .sourceMapAux = {
            { CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,  "CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS", "PLL_SYS" },
            { CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0,    "CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0",   "GPIN0"   },
            { CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1,    "CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_CLKSRC_GPIN1",   "GPIN1"   },
            { CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,  "CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB", "PLL_USB" },
            { CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_ROSC_CLKSRC_PH,  "CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_ROSC_CLKSRC_PH", "ROSC_PH" },
            { CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,     "CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_XOSC_CLKSRC",    "XOSC"    },
        },
    },
};

// State to capture
//
// assumptions during non-sleep:
// - rosc, rosc_ph, xosc always on
// - clk_usb is always 48'000'000, tied to PLL_USB, on or off
// - clk_ref is always 12'000'000, tied to XOSC, always on
// - clk_rtc is always 46,875, tied to XOSC, always on
// - clk_gpout0-3 always off
//
// - other clocks can vary in freq and source
//   - clk_sys
//   - clk_peri
//   - clk_adc
struct ClockState
{
    string name = "clk_ref";

    // clock details
    clock_index clk    = clk_ref;
    uint32_t    src    = CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC;
    uint32_t    auxsrc = 0;

    // source
    string srcNameLong  = "CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC";
    string srcNameShort = "XOSC";

    string prisrcNameLong  = "CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC";
    string prisrcNameShort = "XOSC";

    string auxsrcNameLong  = "-";
    string auxsrcNameShort = "-";

    // source freq
    uint32_t freqSrc = 12'000'000;

    // clk freq
    uint32_t freq = 12'000'000;

    // on or off
    bool on = true;


    void Print()
    {
        Log("Clock: ", name);
        Log("  src: ", srcNameShort, " (", srcNameLong, ")");
        Log("    pri: ", prisrcNameShort, " (", prisrcNameLong, ")");
        Log("    aux: ", auxsrcNameShort, " (", auxsrcNameLong, ")");
        Log("  freqSrc: ", Commas(freqSrc));
        Log("  freq   : ", Commas(freq));
        Log("  on     : ", on);
    }
};

static ClockState GetClockState(clock_index clk, ClockState *overlay = nullptr)
{
    ClockState retVal = {
        .name = "-",
        .clk = clk,
        .src = 0,
        .auxsrc = 0,
        .srcNameLong = "-",
        .srcNameShort = "-",
        .prisrcNameLong = "-",
        .prisrcNameShort = "-",
        .auxsrcNameLong = "-",
        .auxsrcNameShort = "-",
        .freqSrc = 0,
        .freq = clock_get_hz(clk),
        .on = false,
    };

    if (overlay)
    {
        retVal.freq = overlay->freq;
    }

    for (auto &csd : csdList_)
    {
        if (csd.clk == clk)
        {
            retVal.name = csd.name;

            retVal.on = frequency_count_khz(csd.freqCountId) != 0;

            clock_hw_t *clock = &clocks_hw->clk[csd.clk];

            bool auxSourceUsed = csd.sourceMapPrimary.empty();

            // check if can even have a primary source
            // (only clk_ref and clk_sys can)
            if (csd.sourceMapPrimary.size())
            {
                uint8_t cmpVal = ((uint8_t)clock->ctrl) & 0b11;

                if (overlay)
                {
                    cmpVal = (uint8_t)overlay->src;
                }

                for (auto &idData : csd.sourceMapPrimary)
                {
                    if (cmpVal == idData.val)
                    {
                        retVal.src = idData.val;
                        retVal.prisrcNameShort = idData.valNameShort;
                        retVal.prisrcNameLong  = idData.valNameLong;

                        if (idData.val == 1)
                        {
                            // both clk_ref and clk_sys use the same value to represent aux
                            // and this is it
                            auxSourceUsed = true;
                        }
                    }
                }
            }

            if (auxSourceUsed)
            {
                uint8_t cmpVal = ((uint8_t)clock->ctrl) >> 5;

                if (overlay)
                {
                    cmpVal = (uint8_t)overlay->auxsrc;
                }

                for (auto &idData : csd.sourceMapAux)
                {
                    if (cmpVal == idData.val)
                    {
                        retVal.auxsrc = idData.val;

                        retVal.auxsrcNameShort = idData.valNameShort;
                        retVal.auxsrcNameLong  = idData.valNameLong;
                    }
                }
            }
        }
    }

    // determine source
    if (retVal.prisrcNameShort != "-" && retVal.prisrcNameShort != "AUX")
    {
        retVal.srcNameShort = retVal.prisrcNameShort;
        retVal.srcNameLong  = retVal.prisrcNameLong;
    }
    else
    {
        retVal.srcNameShort = retVal.auxsrcNameShort;
        retVal.srcNameLong  = retVal.auxsrcNameLong;
    }

    // determine source frequency
    string srcStr = retVal.srcNameShort;
    if (srcStr == "XOSC")
    {
        retVal.freqSrc = 12'000'000;
    }
    else if (srcStr == "CLK_SYS")
    {
        retVal.freqSrc = clock_get_hz(clk_sys);
    }
    else if (srcStr == "PLL_SYS")
    {
        retVal.freqSrc = GetPllState(pll_sys).pllData.freq;
    }
    else if (srcStr == "PLL_USB")
    {
        retVal.freqSrc = GetPllState(pll_usb).pllData.freq;
    }

    if (overlay)
    {
        retVal.freqSrc = overlay->freqSrc;
    }

    return retVal;
}


static ClockState OverlayClockState(clock_index clk, ClockState overlay)
{
    return GetClockState(clk, &overlay);
}



/////////////////////////////////////////////////////////////////
// State Capture and Change
/////////////////////////////////////////////////////////////////

struct State
{
    vector<PllState>   pllStateList;
    vector<ClockState> clockStateList;

    void Print()
    {
        Log("State:");

        for (auto &pllState : pllStateList)
        {
            pllState.Print();
            LogNL();
        }

        for (auto &clockState : clockStateList)
        {
            clockState.Print();
            LogNL();
        }
    }
};

static State GetState()
{
    // Timeline::Global().Event("RP2040_CLOCK_GET_STATE_START");

    State state = {
        .pllStateList = {
            GetPllState(pll_sys),
            GetPllState(pll_usb),
        },
        .clockStateList = {
            GetClockState(clk_sys),
            GetClockState(clk_peri),
            GetClockState(clk_adc),
        },
    };

    // Timeline::Global().Event("RP2040_CLOCK_GET_STATE_END");

    return state;
}

static void SetState(State state)
{
    Timeline::Global().Event("RP2040_CLOCK_SET_STATE_START");

    // scan for pll_sys being modified, if yes, switch clk_sys to be XOSC so
    // it doesn't halt during the re-adjustment (which shuts off the PLL
    // before re-init)
    bool moveClkSys = false;
    bool moveClkSysBack = false;
    uint32_t pllFreqNew = 0;
    for (auto &pllState : state.pllStateList)
    {
        // is pll_sys being changed?
        if (pllState.pllData.pll == pll_sys && pllState.on)
        {
            ClockState cState = GetClockState(clk_sys);

            // is clk_sys dependent on pll_sys?
            if (cState.src    == CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX &&
                cState.auxsrc == CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS)
            {
                pllFreqNew = pllState.pllData.vco_freq /
                                (pllState.pllData.post_div1 * pllState.pllData.post_div2);
                moveClkSys = true;
                moveClkSysBack = true;
            }
        }
    }

    // scan to see if clk_sys was moving to something else, if yes, then no need to
    // point it back to the pll_sys
    for (auto &clockState : state.clockStateList)
    {
        if (clockState.clk == clk_sys && clockState.on)
        {
            if (clockState.src    != CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX ||
                clockState.auxsrc != CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS)
            {
                moveClkSysBack = false;
            }
        }
    }

    // move clk_sys if required
    if (moveClkSys)
    {
        clock_configure(
            clk_sys,
            CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
            CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,
            12'000'000,
            12'000'000
        );
    }

    // turn things on first to avoid lockup situations
    for (auto &pllState : state.pllStateList)
    {
        if (pllState.on)
        {
            pll_init(
                pllState.pllData.pll,
                pllState.pllData.refdiv,
                pllState.pllData.vco_freq,
                pllState.pllData.post_div1,
                pllState.pllData.post_div2
            );
        }
    }

    for (auto &clockState : state.clockStateList)
    {
        if (clockState.on)
        {
            clock_configure(
                clockState.clk,
                clockState.src,
                clockState.auxsrc,
                clockState.freqSrc,
                clockState.freq
            );
        }
    }

    // then turn things off
    for (auto &clockState : state.clockStateList)
    {
        if (clockState.on == false)
        {
            clock_stop(clockState.clk);
        }
    }

    for (auto &pllState : state.pllStateList)
    {
        if (pllState.on == false)
        {
            pll_deinit(pllState.pllData.pll);
        }
    }

    // move clk_sys back if required
    if (moveClkSysBack)
    {
        clock_configure(
            clk_sys,
            CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
            CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
            pllFreqNew,
            pllFreqNew
        );
    }

    Timeline::Global().Event("RP2040_CLOCK_SET_STATE_CHANGES_DONE");

    // uart probably got messed up along the way, let's restore that
    if (UartIsEnabled(UART::UART_0)) { UartEnable(UART::UART_0); }
    if (UartIsEnabled(UART::UART_1)) { UartEnable(UART::UART_1); }

    Timeline::Global().Event("RP2040_CLOCK_SET_STATE_UART_DONE");

    // if the system clock speed changed, help deal with timing
    ClockState csSys = GetClockState(clk_sys);
    KTime::SetScalingFactor((double)csSys.freq / 125'000'000.0);

    Timeline::Global().Event("RP2040_CLOCK_SET_STATE_END");

    if (verbose_)
    {
        Clock::PrintAll();
    }
}


/////////////////////////////////////////////////////////////////
// Capture/Restore Initial State
/////////////////////////////////////////////////////////////////

static void SetInitialStateConditions()
{
    uint32_t freq = GetFreqROSC();

    // by default, clk_rtc is tied to pll_usb.
    // I don't want that to be the runtime default, so change this
    // before the system comes up so it is established as the
    // initial state reference.
    clock_configure(
        clk_rtc,
        0,
        CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_ROSC_CLKSRC_PH,
        freq,
        46'875
    );

    // clk_adc is also tied to pll_usb by default.
    // tests show safe to use from other clock sources, and
    // changing it now upfront leads to a more consistent experience
    // as the system changes clock speeds.
    // that is, the first time clock is changed, clk_adc gets pulled away
    // from pll_usb to pll_sys/xosc/rosc, and never goes back.  inconsistent
    // experience depending on the sequence of events.
    // this also allows USB to be shut off at any time/sequence without
    // affecting adc nor introducing a sequence dependence on getting adc off
    // usb before shutdown.
    clock_configure(
        clk_adc,
        0,
        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
        125'000'000,
        125'000'000
    );
}

inline static State stateInitial_;
static void CaptureInitialState()
{
    stateInitial_ = GetState();
}

static void GoToInitialState()
{
    SetState(stateInitial_);
}


/////////////////////////////////////////////////////////////////
// High-Level Clock Control
/////////////////////////////////////////////////////////////////

[[maybe_unused]]
static void SetClockInitial()
{
    Timeline::Global().Event("RP2040_CLOCK_INITIAL");

    GoToInitialState();
}

struct PllConfig
{
    double mhz;
    uint refdiv;
    uint fbdiv;
    uint vco_freq;
    uint post_div1;
    uint post_div2;

    void Print()
    {
        Log("MHz      : ", mhz);
        Log("refdiv   : ", refdiv);
        Log("vco_freq : ", Commas(vco_freq));
        Log("post_div1: ", post_div1);
        Log("post_div2: ", post_div2);

        // Log(mhz, ", ", fbdiv, ", ", post_div1, ", ", post_div2, ", ", refdiv);
    }
};

// 15.3 MHz is the lowest you can go with a PLL
// 280  MHz is the fastest you can go without some kind of hang I haven't researched
// 
// adapted from vcocalc.py in pico-sdk
static PllConfig GetPllConfigForFreq(double mhz, bool lowPowerPriority, bool mustBeExact)
{
    PllConfig retVal;

    static const double XOSC_FREQ_MHZ = 12;

    Range<uint16_t> fbdiv_range = lowPowerPriority ? Range<uint16_t>(16, 320) : Range<uint16_t>(320, 16);
    Range<uint8_t> postdiv_range(1, 7);

    uint16_t ref_min = 5;
    uint16_t refdiv_min = 1;
    uint16_t refdiv_max = 63;
    Range refdiv_range = Range(refdiv_min,
                                max(refdiv_min,
                                    min(refdiv_max, (uint16_t)(XOSC_FREQ_MHZ / ref_min))));

    uint16_t vco_min =  750;
    uint16_t vco_max = 1600;

    // Log("fbdiv_range  : ", fbdiv_range.GetStart(), " - ", fbdiv_range.GetEnd());
    // Log("postdiv_range: ", postdiv_range.GetStart(), " - ", postdiv_range.GetEnd());
    // Log("refdiv_range : ", refdiv_range.GetStart(), " - ", refdiv_range.GetEnd());
    // LogNL();

    double best_margin = mhz;

    for (uint16_t refdiv : refdiv_range)
    {
        for (uint16_t fbdiv : fbdiv_range)
        {
            double vco = XOSC_FREQ_MHZ / refdiv * fbdiv;
            // Log(vco, " = ", XOSC_FREQ_MHZ, " / ", refdiv, " * ", fbdiv);

            if (vco < vco_min || vco > vco_max)
            {
                continue;
            }

            // pd1 is inner loop so that we prefer higher ratios of pd1:pd2
            for (uint8_t pd2 : postdiv_range)
            {
                for (uint8_t pd1 : postdiv_range)
                {
                    double out = vco / pd1 / pd2;
                    // Log(out, " = ", vco, " / ", pd1, " / ", pd2);

                    double margin = fabs(out - mhz);
                    if (margin < best_margin)
                    {
                        retVal.mhz = out;
                        retVal.refdiv = refdiv;
                        retVal.fbdiv = fbdiv;
                        retVal.vco_freq = (uint16_t)(XOSC_FREQ_MHZ / refdiv * fbdiv) * MHZ;
                        retVal.post_div1 = pd1;
                        retVal.post_div2 = pd2;

                        best_margin = margin;
                        // Log("Got a new best (margin ", margin, " MHz diff)");
                        // retVal.Print();
                    }
                }
            }
        }
    }

    if (retVal.mhz != mhz && mustBeExact)
    {
        retVal = PllConfig{};
    }

    return retVal;
}































































static void DoNothing() { Log("DoNothing"); }
static void DoNothingSilent() { }

// didn't return from sleep when called from Power Manager, despite
// this being the same code as clk.sleep2, which does work interactively.
// moving on for now.
[[maybe_unused]]
static void DeepSleep(uint32_t us)
{
    uint32_t secs = us / 1'000'000;
    if (secs > 58)
    {
        secs = 58;
    }

    if (secs == 0)
    {
        return;
    }

    // capture current state
    State state = GetState();

    // enter state with no plls and everything running on stoppable clock
    // SetClock12MHzPreSleep();


    // Set RTC timer
    datetime_t tNow = {
            .year  = 2020,
            .month = 06,
            .day   = 05,
            .dotw  = 5, // 0 is Sunday, so 5 is Friday
            .hour  = 15,
            .min   = 45,
            .sec   = 00
    };

    // Alarm x seconds later, add 1 to x for the alarm to work as expected
    datetime_t t_alarm = {
            .year  = 2020,
            .month = 06,
            .day   = 05,
            .dotw  = 5, // 0 is Sunday, so 5 is Friday
            .hour  = 15,
            .min   = 45,
            .sec   = (int8_t)(secs + 1),
    };


    // Start the RTC
    rtc_init(); // needed
    rtc_set_datetime(&tNow);
    rtc_set_alarm(&t_alarm, DoNothingSilent);


    // configure chip to only leave the RTC running in sleep mode
    uint32_t cacheEn0 = clocks_hw->sleep_en0;
    uint32_t cacheEn1 = clocks_hw->sleep_en1;

    clocks_hw->sleep_en0 =
        CLOCKS_SLEEP_EN0_CLK_RTC_RTC_BITS
    ;
    clocks_hw->sleep_en1 = 0x0;

    // Enable deep sleep at the proc
    uint32_t cacheScr = scb_hw->scr;
    scb_hw->scr |= M0PLUS_SCR_SLEEPDEEP_BITS;


    // Go to sleep
    Pin::Configure(15, Pin::Type::OUTPUT, 0);
    Pin::Configure(15, Pin::Type::OUTPUT, 1);
    __wfi();
    Pin::Configure(15, Pin::Type::OUTPUT, 0);

    // restore state
    scb_hw->scr = cacheScr;

    clocks_hw->sleep_en0 = cacheEn0;
    clocks_hw->sleep_en1 = cacheEn1;

    // restore state
    SetState(state);
}


static string GetSource(clock_index clk)
{
    ClockState cs = GetClockState(clk);

    return cs.srcNameShort;
}

[[maybe_unused]]
static void PrintSources()
{
    for (auto &csd : csdList_)
    {
        Log(csd.name, ": ", GetSource(csd.clk));
    }
}

static void PrintCount()
{
    uint64_t count = 0;
    uint64_t timeStart = PAL.Micros();

    const uint64_t DURATION_US = 1'000;

    while (PAL.Micros() - timeStart < DURATION_US)
    {
        ++count;
    }

    Log("Count ", Commas(count), " in ", Commas(DURATION_US), " us");
}







































































/////////////////////////////////////////////////////////////////
// Public Interface
/////////////////////////////////////////////////////////////////


// When 12MHz, use XOSC
//    pll_sys            0  XOSC
//    pll_usb   48,000,000  XOSC
//    clk_ref   12,002,000  XOSC
//    clk_sys   12,000,000  XOSC
//   clk_peri   12,002,000  XOSC
//    clk_usb   48,000,000  PLL_USB
//    clk_adc   48,000,000  XOSC
//    clk_rtc       47,000  XOSC
//
// When something else, change pll_sys
//    pll_sys    X,000,000  XOSC
//    pll_usb   48,000,000  XOSC
//    clk_ref   12,000,000  XOSC
//    clk_sys    X,000,000  PLL_SYS
//   clk_peri    X,000,000  PLL_SYS
//    clk_usb   48,000,000  PLL_USB
//    clk_adc   48,000,000  PLL_SYS
//    clk_rtc       46,000  XOSC
void Clock::SetClockMHz(double mhz, bool lowPowerPriority, bool mustBeExact)
{
    Timeline::Global().Event("SET_CLOCK_MHZ");

    if (mhz == 6)
    {
        // set up new state
        State newState;

        // disable pll_sys
        PllState psSys = GetPllState(pll_sys);
        psSys.on = false;
        newState.pllStateList.push_back(psSys);

        uint32_t freq = GetFreqROSC();

        // change clk_sys to be driven by ROSC
        ClockState csSys = OverlayClockState(clk_sys, {
            .src     = CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
            .auxsrc  = CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_ROSC_CLKSRC,
            .freqSrc = freq,
            .freq    = freq,
        });
        newState.clockStateList.push_back(csSys);

        // change clk_peri to be driven by ROSC
        ClockState csPeri = OverlayClockState(clk_peri, {
            .src     = 0,
            .auxsrc  = CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_ROSC_CLKSRC_PH,
            .freqSrc = freq,
            .freq    = freq,
        });
        newState.clockStateList.push_back(csPeri);

        // change clk_adc to be driven by ROSC
        ClockState csAdc = OverlayClockState(clk_adc, {
            .src     = 0,
            .auxsrc  = CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_ROSC_CLKSRC_PH,
            .freqSrc = freq,
            .freq    = freq,
        });
        newState.clockStateList.push_back(csAdc);

        // apply
        SetState(newState);
    }
    else if (mhz == 12)
    {
        // set up new state
        State newState;

        // disable pll_sys
        PllState psSys = GetPllState(pll_sys);
        psSys.on = false;
        newState.pllStateList.push_back(psSys);

        // change clk_sys to be driven by XOSC
        ClockState csSys = OverlayClockState(clk_sys, {
            .src     = CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
            .auxsrc  = CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,
            .freqSrc = 12 * MHZ,
            .freq    = 12 * MHZ,
        });
        newState.clockStateList.push_back(csSys);

        // change clk_peri to be driven by XOSC
        ClockState csPeri = OverlayClockState(clk_peri, {
            .src     = 0,
            .auxsrc  = CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,
            .freqSrc = 12 * MHZ,
            .freq    = 12 * MHZ,
        });
        newState.clockStateList.push_back(csPeri);

        // change clk_adc to be driven by XOSC
        ClockState csAdc = OverlayClockState(clk_adc, {
            .src     = 0,
            .auxsrc  = CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,
            .freqSrc = 12 * MHZ,
            .freq    = 12 * MHZ,
        });
        newState.clockStateList.push_back(csAdc);

        // apply
        SetState(newState);
    }
    else 
    {
        // Figure out PLL configuration to get to requested MHz
        PllConfig pc;
        
        if (freq__data.contains(mhz))
        {
            pc = freq__data[mhz];
        }
        else
        {
            pc = GetPllConfigForFreq(mhz, lowPowerPriority, mustBeExact);
        }

        if (pc.refdiv != 0)
        {
            // set up new state
            State newState;

            // Adjust the frequency of pll_sys
            PllState psSys = GetPllState(pll_sys);
            psSys.on = true;
            psSys.pllData.refdiv = pc.refdiv;
            psSys.pllData.vco_freq = pc.vco_freq;
            psSys.pllData.post_div1 = pc.post_div1;
            psSys.pllData.post_div2 = pc.post_div2;
            newState.pllStateList.push_back(psSys);

            // change clk_sys to be driven by pll_sys
            ClockState csSys = OverlayClockState(clk_sys, {
                .src     = CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                .auxsrc  = CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                .freqSrc = (uint32_t)mhz * MHZ,
                .freq    = (uint32_t)mhz * MHZ,
            });
            newState.clockStateList.push_back(csSys);

            // change clk_peri to be driven by pll_sys
            ClockState csPeri = OverlayClockState(clk_peri, {
                .src     = 0,
                .auxsrc  = CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                .freqSrc = (uint32_t)mhz * MHZ,
                .freq    = (uint32_t)mhz * MHZ,
            });
            newState.clockStateList.push_back(csPeri);

            // change clk_adc to be driven by pll_sys
            ClockState csAdc = OverlayClockState(clk_adc, {
                .src     = 0,
                .auxsrc  = CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                .freqSrc = (uint32_t)mhz * MHZ,
                .freq    = (uint32_t)mhz * MHZ,
            });
            newState.clockStateList.push_back(csAdc);

            // apply
            SetState(newState);
        }
        else
        {
            Log("SetClockMHz ERR: Could not set frequency ", mhz);
        }
    }
}

void Clock::EnableUSB()
{
    Log("Clock::EnableUSB");

    PllState psUsb = GetPllState(pll_usb);

    pll_init(
        psUsb.pllData.pll,
        psUsb.pllData.refdiv,
        psUsb.pllData.vco_freq,
        psUsb.pllData.post_div1,
        psUsb.pllData.post_div2
    );
}

void Clock::DisableUSB()
{
    Log("Clock::DisableUSB");

    pll_deinit(pll_usb);
}

void Clock::PrepareClockMHz(double mhz, bool lowPowerPriority, bool mustBeExact)
{
    if (mhz != 6 && mhz != 12)
    {
        PllConfig pc = GetPllConfigForFreq(mhz, lowPowerPriority, mustBeExact);

        freq__data[mhz] = pc;
    }
}


/////////////////////////////////////////////////////////////////
// Debug and Console
/////////////////////////////////////////////////////////////////

void Clock::PrintAll()
{
    Log("name        freq         src");
    Log("----------------------------");

    auto Printer = [](string name, uint32_t freq, string src){
        const uint8_t BUF_SIZE = 100;
        char buf[BUF_SIZE];
        memset(buf, 0, BUF_SIZE);

        snprintf(buf, BUF_SIZE, "%10s  %11s  %s", name.c_str(), Commas(freq).c_str(), src.c_str());

        // Log(name, "\t", Commas(freq), "\t\t", src);
        Log(buf);
    };

    auto Freq = [](uint32_t id) {
        return frequency_count_khz(id) * 1'000;
    };

    Printer("rosc",    Freq(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC),    "");
    Printer("rosc_ph", Freq(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC_PH), "");
    
    Printer("xosc",    Freq(CLOCKS_FC0_SRC_VALUE_XOSC_CLKSRC),            "");
    Printer("pll_sys", Freq(CLOCKS_FC0_SRC_VALUE_PLL_SYS_CLKSRC_PRIMARY), "XOSC");
    Printer("pll_usb", Freq(CLOCKS_FC0_SRC_VALUE_PLL_USB_CLKSRC_PRIMARY), "XOSC");
    
    Printer("clk_ref",  Freq(CLOCKS_FC0_SRC_VALUE_CLK_REF),  GetSource(clk_ref));
    Printer("clk_sys",  Freq(CLOCKS_FC0_SRC_VALUE_CLK_SYS),  GetSource(clk_sys));
    Printer("clk_peri", Freq(CLOCKS_FC0_SRC_VALUE_CLK_PERI), GetSource(clk_peri));
    Printer("clk_usb",  Freq(CLOCKS_FC0_SRC_VALUE_CLK_USB),  GetSource(clk_usb));
    Printer("clk_adc",  Freq(CLOCKS_FC0_SRC_VALUE_CLK_ADC),  GetSource(clk_adc));
    Printer("clk_rtc",  Freq(CLOCKS_FC0_SRC_VALUE_CLK_RTC),  GetSource(clk_rtc));
    
    Printer("clk_gpout0", 0, GetSource(clk_gpout0));
    Printer("clk_gpout1", 0, GetSource(clk_gpout1));
    Printer("clk_gpout2", 0, GetSource(clk_gpout2));
    Printer("clk_gpout3", 0, GetSource(clk_gpout3));

    Printer("clk_gpin0", Freq(CLOCKS_FC0_SRC_VALUE_CLKSRC_GPIN0), "");
    Printer("clk_gpin1", Freq(CLOCKS_FC0_SRC_VALUE_CLKSRC_GPIN1), "");

    if (verbose_)
    {
        LogNL();
        GetPllState(pll_sys).Print();  LogNL();
        GetPllState(pll_usb).Print();  LogNL();

        GetClockState(clk_ref).Print();  LogNL();
        GetClockState(clk_sys).Print();  LogNL();
        GetClockState(clk_peri).Print();  LogNL();
        GetClockState(clk_usb).Print();  LogNL();
        GetClockState(clk_adc).Print();  LogNL();
        GetClockState(clk_rtc).Print();  LogNL();
        GetClockState(clk_gpout0).Print();  LogNL();
        GetClockState(clk_gpout1).Print();  LogNL();
        GetClockState(clk_gpout2).Print();  LogNL();
        GetClockState(clk_gpout3).Print();  LogNL();
    }
}


/////////////////////////////////////////////////////////////////
// Configuration
/////////////////////////////////////////////////////////////////

void Clock::SetVerbose(bool verbose)
{
    verbose_ = verbose;
}

void Clock::Init()
{
    Timeline::Global().Event("Clock::Init");

    SetInitialStateConditions();
    CaptureInitialState();
}

void Clock::SetupShell()
{
    Timeline::Global().Event("Clock::SetupShell");

    Shell::AddCommand("clk.show", [](vector<string> argList) {
        PrintAll();
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("clk.verbose", [](vector<string> argList) {
        SetVerbose(argList[0] == "true");
    }, { .argCount = 1, .help = "set verbose <true|false>" });

    Shell::AddCommand("clk.count", [](vector<string> argList) {
        PrintCount();
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("clk.freq.orig", [](vector<string> argList) {
        GoToInitialState();
        PrintAll();
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("clk.freq", [](vector<string> argList) {
        double mhz              = atof(argList[0].c_str());
        bool   lowPowerPriority = false;
        bool   mustBeExact      = false;

        if (argList.size() >= 2)
        {
            lowPowerPriority = atoi(argList[1].c_str());
        }

        if (argList.size() >= 3)
        {
            mustBeExact = atoi(argList[2].c_str());
        }

        SetClockMHz(mhz, lowPowerPriority, mustBeExact);

        PrintAll();
    }, { .argCount = -1, .help = "Set <x> MHz, <y> lowPowerPriority, <z> mustBeExact" });

    Shell::AddCommand("clk.prep", [](vector<string> argList) {
        double mhz              = atof(argList[0].c_str());
        bool   lowPowerPriority = false;
        bool   mustBeExact      = false;

        if (argList.size() >= 2)
        {
            lowPowerPriority = atoi(argList[1].c_str());
        }

        if (argList.size() >= 3)
        {
            mustBeExact = atoi(argList[2].c_str());
        }

        PrepareClockMHz(mhz, lowPowerPriority, mustBeExact);
    }, { .argCount = -1, .help = "Prepare <x> MHz, <y> lowPowerPriority, <z> mustBeExact" });

    Shell::AddCommand("clk.usb", [](vector<string> argList) {
        if (atoi(argList[0].c_str()))
        {
            EnableUSB();
        }
        else
        {
            DisableUSB();
        }

        PrintAll();
    }, { .argCount = 1, .help = "USB enable(1)/disable(0) " });

    Shell::AddCommand("clk.stop", [](vector<string> argList) {
        string clock = argList[0];

        if (clock == "ref")  { clock_stop(clk_ref);  }
        if (clock == "sys")  { clock_stop(clk_sys);  }
        if (clock == "peri") { clock_stop(clk_peri); }
        if (clock == "usb")  { clock_stop(clk_usb);  }
        if (clock == "adc")  { clock_stop(clk_adc);  }
        if (clock == "rtc")  { clock_stop(clk_rtc);  }

        PrintAll();
    }, { .argCount = 1, .help = "clock_stop(clk_<x>)" });

    Shell::AddCommand("clk.disable.xosc", [](vector<string> argList) {
        xosc_disable();

        PrintAll();
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("clk.disable.pll_sys", [](vector<string> argList) {
        pll_deinit(pll_sys);

        PrintAll();
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("clk.disable.pll_usb", [](vector<string> argList) {
        pll_deinit(pll_usb);

        PrintAll();
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("clk.deinit", [](vector<string> argList) {
        i2c_deinit(i2c1);

        PrintAll();
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("clk.sleep", [](vector<string> argList) {
        static Timeline t;

        t.Reset();
        t.Event("sleep2");

        // capture current state
        State state = GetState();
        t.Event("got state");

        // enter state with no plls and everything running on stoppable clock
        SetClockMHz(6); // could turn off ADC but eh(?)
        DisableUSB();
        xosc_disable();
        t.Event("Low MHz");


        // make the reference clock continue to work via ROSC?
            // it doesn't seem to work when I change it to use ROSC
            // time doesn't update under sleep, not sure why

        
        // sleep bombs out occasionally when returning from
        // clocks.c line 51 on assert(src_freq >= freq);
        // as in, the frequency of the configured source is >= the
        // frequency I expect this clock to run at.
        //
        // not sure why, haven't investigated sufficiently.
        //
        // non-deterministic, though, there must be a timining element to it,
        // because I can sleep again and again, and only occasionally does it
        // bomb out.
        // is there some number of clock cycles I'm supposed to wait before
        // doing a thing, or something?
        //
        // I am not going to chase this further at this time, I can easily
        // just drop to 12MHz (~5mA with USB and peripherals disabled, 
        // which is ~1mA more than 6MHz ROSC) to do big power savings and
        // not even have to blink out of computation, AND I get to keep a
        // sense of passing time and not wrangle with sleep.
        //
        // In short, datasheet for pico board says I can get down to 1.4 mA,
        // for deep sleep (not dormant at 1.2 mA).  I can keep operating
        // in a working way at 5mA.  I'll take it for now until I have a
        // requirement to do better.



        // Set RTC timer
        datetime_t tNow = {
                .year  = 2020,
                .month = 06,
                .day   = 05,
                .dotw  = 5, // 0 is Sunday, so 5 is Friday
                .hour  = 15,
                .min   = 45,
                .sec   = 00
        };

        // Alarm x seconds later, add 1 to x for the alarm to work as expected
        datetime_t t_alarm = {
                .year  = 2020,
                .month = 06,
                .day   = 05,
                .dotw  = 5, // 0 is Sunday, so 5 is Friday
                .hour  = 15,
                .min   = 45,
                .sec   = 2
        };


        // Start the RTC
        rtc_init(); // needed
        rtc_set_datetime(&tNow);
        rtc_set_alarm(&t_alarm, DoNothing);


        t.Event("alarm set");


        // configure chip to only leave the RTC running in sleep mode
        uint32_t cacheEn0 = clocks_hw->sleep_en0;
        uint32_t cacheEn1 = clocks_hw->sleep_en1;

        clocks_hw->sleep_en0 = CLOCKS_SLEEP_EN0_CLK_RTC_RTC_BITS;
        clocks_hw->sleep_en1 = 0x0;

        // Enable deep sleep at the proc
        uint32_t cacheScr = scb_hw->scr;
        scb_hw->scr |= M0PLUS_SCR_SLEEPDEEP_BITS;


        // Go to sleep
        __wfi();

        // restore state
        scb_hw->scr = cacheScr;

        clocks_hw->sleep_en0 = cacheEn0;
        clocks_hw->sleep_en1 = cacheEn1;


        t.Event("sleep done");


        // restore state
        xosc_init();
        SetState(state);

        t.Event("state restored");

        t.Report();

        PrintAll();
    }, { .argCount = 0, .help = "" });

    // why am I not using dormant?
    // p. 219, 225
    // apparently there's a hello_sleep and hello_dormant example
        // yeah not sure I remember why I was trying sleep and not dormant



    // do more with this, I don't remember learning if this was useful or not
    Shell::AddCommand("clk.wake.regs", [](vector<string> argList) {
        Log("WAKE_EN0: ", ToBin(clocks_hw->wake_en0));
        Log("WAKE_EN1: ", ToBin(clocks_hw->wake_en1));
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("clk.sleep.regs", [](vector<string> argList) {
        Log("SLEEP_EN0: ", ToBin(clocks_hw->sleep_en0));
        Log("SLEEP_EN1: ", ToBin(clocks_hw->sleep_en1));
    }, { .argCount = 0, .help = "" });

    Shell::AddCommand("clk.vreg", [](vector<string> argList) {

        double v = atof(argList[0].c_str());

        if      (v < 0.85) { v = 0.85; }
        else if (v > 1.30) { v = 1.30; }

        Log("req ", argList[0], ", got ", v);

        // handles increments of 0.05 starting at 0.85
        uint8_t vregVal = VREG_VOLTAGE_0_85 + (uint8_t)((v - 0.85) / 0.05);

        Log("vregVal: ", ToBin(vregVal));

        vreg_set_voltage((vreg_voltage)vregVal);
    }, { .argCount = 1, .help = "set vreg to 0.85 <= x <= 1.30" });
}



