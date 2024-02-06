#pragma once

#include <cstring>
#include <vector>
#include <unordered_map>
#include <utility>

#include <zephyr/drivers/clock_control.h>
#include <zephyr/sys/time_units.h>

#include "hardware/regs/intctrl.h"
#include "hardware/structs/clocks.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/scb.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/pll.h"
#include "rtc.h"    // had to take private copy because rtc->irq->assembly something that didn't build properly
#include "rosc.h"   // taken from pico-extras
#include "hardware/irq.h"
#include "hardware/resets.h"
#include "hardware/sync.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "hardware/vreg.h"
#include "hardware/xosc.h"
#include "sleep.h"  // taken from pico-extras

#include "KTime.h"
#include "Log.h"
#include "Shell.h"
#include "Timeline.h"
#include "UART.h"
#include "Utl.h"


/*
enum clock_index {
    clk_gpout0 = 0,     ///< GPIO Muxing 0
    clk_gpout1,         ///< GPIO Muxing 1
    clk_gpout2,         ///< GPIO Muxing 2
    clk_gpout3,         ///< GPIO Muxing 3
    clk_ref,            ///< Watchdog and timers reference clock
    clk_sys,            ///< Processors, bus fabric, memory, memory mapped registers
    clk_peri,           ///< Peripheral clock for UART and SPI
    clk_usb,            ///< USB clock
    clk_adc,            ///< ADC clock
    clk_rtc,            ///< Real time clock
    CLK_COUNT
};
*/

class RP2040_Clock
{

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

    inline static vector<PllData> pdList_ = {
        {
            .pll = pll_sys,
            .name = "pll_sys",
            .freq = 125'000'000,
            .refdiv = 1,
            .vco_freq = 1500 * PICO_MHZ,
            .post_div1 = 6,
            .post_div2 = 2,
            .freqCountId = CLOCKS_FC0_SRC_VALUE_PLL_SYS_CLKSRC_PRIMARY,
        },
        {
            .pll = pll_usb,
            .name = "pll_usb",
            .freq = 48'000'000,
            .refdiv = 1,
            .vco_freq = 1200 * PICO_MHZ,
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
        }
    };

    static PllState GetPllState(pll_hw_t *pll)
    {
        PllState retVal;

        for (auto &pd : pdList_)
        {
            if (pd.pll == pll)
            {
                retVal.pllData = pd;
                retVal.on = frequency_count_khz(pd.freqCountId) != 0;
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

    static ClockState OverlayClockState(clock_index clk, ClockState overlay)
    {
        return GetClockState(clk, &overlay);
    }

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
                        cmpVal = overlay->src;
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
                        cmpVal = overlay->auxsrc;
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
            retVal.freqSrc = 125'000'000;
        }
        else if (srcStr == "PLL_USB")
        {
            retVal.freqSrc = 48'000'000;
        }

        if (overlay)
        {
            retVal.freqSrc = overlay->freqSrc;
        }

        return retVal;
    }



    /////////////////////////////////////////////////////////////////
    // State Capture and Change
    /////////////////////////////////////////////////////////////////

    static void SetInitialStateConditions()
    {
        // by default, clk_rtc is tied to pll_usb.
        // I don't want that to be the runtime default, so change this
        // before the system comes up so it is established as the
        // initial state reference.
        clock_configure(
            clk_rtc,
            0,
            CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,
            12'000'000,
            46875
        );
    }

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

        // then turng things off
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

        Timeline::Global().Event("RP2040_CLOCK_SET_STATE_CHANGES_DONE");

        // uart probably got messed up along the way, let's restore that
        uart_set_baudrate(uart0, UartGetBaud(UART::UART_0));
        uart_set_baudrate(uart1, UartGetBaud(UART::UART_1));

        uart_set_irq_enables(uart0, true, false);

        Timeline::Global().Event("RP2040_CLOCK_SET_STATE_UART_DONE");

        // if the system clock speed changed, help deal with zephyr timing
        ClockState csSys = GetClockState(clk_sys);
        KTime::SetScalingFactor((double)csSys.freq / 125'000'000.0);

        Timeline::Global().Event("RP2040_CLOCK_SET_STATE_END");

        LogModeSync();
        PrintAll();
        LogModeAsync();
    }

    inline static bool initialStateCaptured_ = false;
    inline static State stateInitial_;
    static void EnsureInitialStateCaptured()
    {
        if (initialStateCaptured_ == false)
        {
            initialStateCaptured_ = true;

            stateInitial_ = GetState();
        }
    }

    //       rosc    5,235,000
    //    rosc_ph    5,237,000
    //       xosc   12,000,000
    //    pll_sys  125,000,000  XOSC
    //    pll_usb   48,000,000  XOSC
    //    clk_ref   12,001,000  XOSC
    //    clk_sys  125,001,000  PLL_SYS
    //   clk_peri  125,000,000  CLK_SYS
    //    clk_usb   48,000,000  PLL_USB
    //    clk_adc   48,000,000  PLL_USB
    //    clk_rtc       47,000  XOSC
    // clk_gpout0            0  PLL_SYS
    // clk_gpout1            0  PLL_SYS
    // clk_gpout2            0  PLL_SYS
    // clk_gpout3            0  PLL_SYS
    //  clk_gpin0            0
    //  clk_gpin1            0
    static void GoToInitialState()
    {
        EnsureInitialStateCaptured();

        SetState(stateInitial_);
    }

public:

    static void SetClockInitial()
    {
        Timeline::Global().Event("RP2040_CLOCK_INITIAL");

        GoToInitialState();
    }

    static void SetClockNoUsb()
    {
        Timeline::Global().Event("RP2040_CLOCK_NOUSB");

        GoToInitialState();

        // set up new state
        State newState;

        // testing -- shut off pllusb
        PllState psUsb = GetPllState(pll_usb);
        psUsb.on = false;
        newState.pllStateList.push_back(psUsb);


        // change clk_adc to be driven by XOSC
        ClockState csAdc = OverlayClockState(clk_adc, {
            .src     = 0,
            .auxsrc  = CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,
            .freqSrc = 12'000'000,
            .freq    = 12'000'000,
        });
        newState.clockStateList.push_back(csAdc);
        

        // if (verbose_)
        {
            Log("Applying new state for NOUSB");
            newState.Print();
            PrintCount();
        }

        // apply
        SetState(newState);
    }


    //    pll_sys            0  XOSC    (change)
    //    pll_usb   48,000,000  XOSC
    //    clk_ref   12,001,000  XOSC
    //    clk_sys   48,000,000  PLL_USB (change)
    //   clk_peri   48,000,000  CLK_SYS (change)
    //    clk_usb   48,000,000  PLL_USB
    static void SetClock48MHzNew()
    {
        Timeline::Global().Event("RP2040_CLOCK_48");

        pll_init(
            pll_usb,
            1,
            1200 * PICO_MHZ,
            5,
            5
        );

        clock_configure(
            clk_sys,
            CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
            CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
            48'000'000,
            48'000'000
        );

        clock_configure(
            clk_peri,
            0,
            CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
            48'000'000,
            48'000'000
        );

        clock_configure(
            clk_adc,
            0,
            CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,
            12'000'000,
            12'000'000
        );

        pll_deinit(pll_sys);

        uart_set_baudrate(uart0, UartGetBaud(UART::UART_0));
        uart_set_baudrate(uart1, UartGetBaud(UART::UART_1));

        uart_set_irq_enables(uart0, true, false);

        ClockState csSys = GetClockState(clk_sys);
        KTime::SetScalingFactor((double)csSys.freq / 125'000'000.0);
    }

    //    pll_sys            0  XOSC    (change)
    //    pll_usb   48,000,000  XOSC
    //    clk_ref   12,001,000  XOSC
    //    clk_sys   48,000,000  PLL_USB (change)
    //   clk_peri   48,000,000  CLK_SYS (change)
    //    clk_usb   48,000,000  PLL_USB
    static void SetClock48MHz()
    {
        Timeline::Global().Event("RP2040_CLOCK_48");

        GoToInitialState();

        // set up new state
        State newState;

        // shut off pll_sys
        PllState psSys = GetPllState(pll_sys);
        psSys.on = false;
        newState.pllStateList.push_back(psSys);

        // change clk_sys to be driven by PLL_USB
        ClockState csSys = OverlayClockState(clk_sys, {
            .src     = CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
            .auxsrc  = CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
            .freqSrc = 48'000'000,
            .freq    = 48'000'000,
        });
        newState.clockStateList.push_back(csSys);

        // change clk_peri to be driven by clk_sys at clk_sys's new frequency
        ClockState csPeri = OverlayClockState(clk_peri, {
            .src     = 0,
            .auxsrc  = CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
            .freqSrc = 48'000'000,
            .freq    = 48'000'000,
        });
        newState.clockStateList.push_back(csPeri);

        if (verbose_)
        {
            Log("Applying new state for 48MHz");
            newState.Print();
            PrintCount();
        }

        // apply
        SetState(newState);
    }


    //    pll_sys            0  XOSC
    //    pll_usb            0  XOSC
    //    clk_ref   12,001,000  XOSC
    //    clk_sys   12,000,000  XOSC
    //   clk_peri   12,000,000  CLK_SYS
    //    clk_usb            0  PLL_USB
    //    clk_adc   12,002,000  XOSC
    //
    // runs at 4mA idle, 5mA processing UART
    static void SetClock12MHzNew()
    {
        Timeline::Global().Event("RP2040_CLOCK_12");

        clock_configure(
            clk_sys,
            CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
            CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,
            12'000'000,
            12'000'000
        );

        clock_configure(
            clk_peri,
            0,
            CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
            12'000'000,
            12'000'000
        );

        clock_configure(
            clk_adc,
            0,
            CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,
            12'000'000,
            12'000'000
        );

        pll_deinit(pll_sys);
        pll_deinit(pll_usb);

        uart_set_baudrate(uart0, UartGetBaud(UART::UART_0));
        uart_set_baudrate(uart1, UartGetBaud(UART::UART_1));

        uart_set_irq_enables(uart0, true, false);

        ClockState csSys = GetClockState(clk_sys);
        KTime::SetScalingFactor((double)csSys.freq / 125'000'000.0);

        PrintAll();
    }

    //    pll_sys            0  XOSC
    //    pll_usb            0  XOSC
    //    clk_ref   12,001,000  XOSC
    //    clk_sys   12,000,000  XOSC
    //   clk_peri   12,000,000  CLK_SYS
    //    clk_usb            0  PLL_USB
    //    clk_adc   12,002,000  XOSC
    //
    // runs at 4mA idle, 5mA processing UART
    static void SetClock12MHzOldMod()
    {
        Timeline::Global().Event("RP2040_CLOCK_12");

        // GoToInitialState();

        // set up new state
        State newState;

        // // shut off pll_sys
        // PllState psSys = GetPllState(pll_sys);
        // psSys.on = false;
        // newState.pllStateList.push_back(psSys);

        // shut off pll_usb (shaves off 3.5mA)
        PllState psUsb = GetPllState(pll_usb);
        psUsb.on = false;
        newState.pllStateList.push_back(psUsb);

        // change clk_sys to be driven by PLL_USB
        // no observed savings using clk_ref vs using XOSC
        ClockState csSys = OverlayClockState(clk_sys, {
            .src     = CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
            .auxsrc  = CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,
            .freqSrc = 12'000'000,
            .freq    = 12'000'000,
        });
        newState.clockStateList.push_back(csSys);

        // using clk_sys is a 500uA savings over using XOSC
        // (even though that's what clk_sys is using)
        ClockState csPeri = OverlayClockState(clk_peri, {
            .src     = 0,
            .auxsrc  = CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
            .freqSrc = 12'000'000,
            .freq    = 12'000'000,
        });
        newState.clockStateList.push_back(csPeri);

        // change clk_adc to be driven by XOSC
        ClockState csAdc = OverlayClockState(clk_adc, {
            .src     = 0,
            .auxsrc  = CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,
            .freqSrc = 12'000'000,
            .freq    = 12'000'000,
        });
        newState.clockStateList.push_back(csAdc);

        if (verbose_)
        {
            Log("Applying new state for 12MHz");
            newState.Print();
            PrintCount();
        }

        // apply
        SetState(newState);
    }

    //    pll_sys            0  XOSC
    //    pll_usb            0  XOSC
    //    clk_ref   12,001,000  XOSC
    //    clk_sys   12,000,000  XOSC
    //   clk_peri   12,000,000  CLK_SYS
    //    clk_usb            0  PLL_USB
    //    clk_adc   12,002,000  XOSC
    //
    // runs at 4mA idle, 5mA processing UART
    static void SetClock12MHzOld()
    {
        Timeline::Global().Event("RP2040_CLOCK_12");

        GoToInitialState();

        // set up new state
        State newState;

        // shut off pll_sys
        PllState psSys = GetPllState(pll_sys);
        psSys.on = false;
        newState.pllStateList.push_back(psSys);

        // shut off pll_usb (shaves off 3.5mA)
        PllState psUsb = GetPllState(pll_usb);
        psUsb.on = false;
        newState.pllStateList.push_back(psUsb);

        // change clk_sys to be driven by PLL_USB
        // no observed savings using clk_ref vs using XOSC
        ClockState csSys = OverlayClockState(clk_sys, {
            .src     = CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
            .auxsrc  = CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,
            .freqSrc = 12'000'000,
            .freq    = 12'000'000,
        });
        newState.clockStateList.push_back(csSys);

        // using clk_sys is a 500uA savings over using XOSC
        // (even though that's what clk_sys is using)
        ClockState csPeri = OverlayClockState(clk_peri, {
            .src     = 0,
            .auxsrc  = CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
            .freqSrc = 12'000'000,
            .freq    = 12'000'000,
        });
        newState.clockStateList.push_back(csPeri);

        // change clk_adc to be driven by XOSC
        ClockState csAdc = OverlayClockState(clk_adc, {
            .src     = 0,
            .auxsrc  = CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,
            .freqSrc = 12'000'000,
            .freq    = 12'000'000,
        });
        newState.clockStateList.push_back(csAdc);

        if (verbose_)
        {
            Log("Applying new state for 12MHz");
            newState.Print();
            PrintCount();
        }

        // apply
        SetState(newState);
    }


    //    pll_sys            0  XOSC
    //    pll_usb            0  XOSC
    //    clk_ref   12,001,000  XOSC
    //    clk_sys   12,000,000  XOSC
    //   clk_peri   12,000,000  CLK_SYS
    //    clk_usb            0  PLL_USB
    //    clk_adc            0  PLL_USB
    //
    // this isn't really about power saving, this was setting up
    // for complete shutdown.  effort was to move off of
    // PLLs.  clock speed unimportant.
    // better to change the name to reflect that.
    static void SetClock12MHzPreSleep()
    {
        GoToInitialState();

        // set up new state
        State newState;

        // shut off pll_sys
        PllState psSys = GetPllState(pll_sys);
        psSys.on = false;
        newState.pllStateList.push_back(psSys);

        // shut off pll_usb (shaves off 3.5mA)
        PllState psUsb = GetPllState(pll_usb);
        psUsb.on = false;
        newState.pllStateList.push_back(psUsb);

        // change clk_sys to be driven by XOSC
        ClockState csSys = GetClockState(clk_sys);
        csSys.src     = CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX;
        csSys.auxsrc  = CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_XOSC_CLKSRC;
        csSys.freqSrc = 12'000'000;
        csSys.freq    = 12'000'000;
        newState.clockStateList.push_back(csSys);

        // change clk_peri to be driven by clk_sys at clk_sys's new frequency
        ClockState csPeri = GetClockState(clk_peri);
        csPeri.src     = 0;
        csPeri.auxsrc  = CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS;
        csPeri.freqSrc = 12'000'000;
        csPeri.freq    = 12'000'000;
        newState.clockStateList.push_back(csPeri);

        if (verbose_)
        {
            Log("Applying new state for 12MHzPreSleep");
            newState.Print();
            PrintCount();
        }

        // apply
        SetState(newState);
    }



public:

    /////////////////////////////////////////////////////////////////
    // Configuration
    /////////////////////////////////////////////////////////////////

    inline static bool verbose_ = false;

    static void SetVerbose(bool verbose)
    {
        verbose_ = verbose;
    }


    // didn't return from sleep when called from Power Manager, despite
    // this being the same code as clk.sleep2, which does work interactively.
    // moving on for now.
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
        SetClock12MHzPreSleep();


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
        Pin::Configure(0, 15, Pin::Type::OUTPUT, 0);
        Pin::Configure(0, 15, Pin::Type::OUTPUT, 1);
        __wfi();
        Pin::Configure(0, 15, Pin::Type::OUTPUT, 0);

        // restore state
        scb_hw->scr = cacheScr;

        clocks_hw->sleep_en0 = cacheEn0;
        clocks_hw->sleep_en1 = cacheEn1;

        // restore state
        SetState(state);
    }


    /////////////////////////////////////////////////////////////////
    // Debug and Console
    /////////////////////////////////////////////////////////////////

    static void PrintAll()
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
            GetPllState(pll_sys).Print();
            GetPllState(pll_usb).Print();

            LogNL();
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

    static void PrintSources()
    {
        for (auto &csd : csdList_)
        {
            Log(csd.name, ": ", GetSource(csd.clk));
        }
    }

    static string GetSource(clock_index clk)
    {
        ClockState cs = GetClockState(clk);

        return cs.srcNameShort;
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



static void DoNothing() { Log("DoNothing"); }
static void DoNothingSilent() { }


    static void Init()
    {
        Timeline::Global().Event("RP2040_Clock::Init");

        SetInitialStateConditions();
    }

    static void SetupShell()
    {
        Timeline::Global().Event("RP2040_Clock::SetupShell");

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
        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("clk.freq.48", [](vector<string> argList) {
            SetClock48MHz();
        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("clk.freq.12", [](vector<string> argList) {
            // SetClock12MHz();
        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("clk.disable.rosc", [](vector<string> argList) {
            rosc_disable();
        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("clk.disable.pll_sys", [](vector<string> argList) {
            pll_deinit(pll_sys);
        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("clk.disable.pll_usb", [](vector<string> argList) {
            pll_deinit(pll_usb);
        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("clk.deinit", [](vector<string> argList) {
            i2c_deinit(i2c1);
        }, { .argCount = 0, .help = "" });


        Shell::AddCommand("clk.sleep2", [](vector<string> argList) {
            static Timeline t;

            t.Reset();
            t.Event("sleep2");

            // capture current state
            State state = GetState();
            t.Event("got state");

            // enter state with no plls and everything running on stoppable clock
            SetClock12MHzPreSleep();
            t.Event("12 MHz");


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
                    .sec   = 6
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
            Pin::Configure(0, 15, Pin::Type::OUTPUT, 0);
            Pin::Configure(0, 15, Pin::Type::OUTPUT, 1);
            __wfi();
            Pin::Configure(0, 15, Pin::Type::OUTPUT, 0);

            // restore state
            scb_hw->scr = cacheScr;

            clocks_hw->sleep_en0 = cacheEn0;
            clocks_hw->sleep_en1 = cacheEn1;


            t.Event("sleep done");


            // restore state
            SetState(state);

            t.Event("state restored");

            t.Report();

            PrintAll();
        }, { .argCount = 0, .help = "" });


        Shell::AddCommand("clk.sleep", [](vector<string> argList) {
            // sleep_run_from_xosc();

            clock_configure(clk_sys,
                            CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                            CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,
                            12'000'000,
                            12'000'000);



            clock_stop(clk_usb);
            // clock_stop(clk_adc);




            clock_configure(clk_rtc,
                            0,
                            CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,
                            12'000'000,
                            46875);

            // CLK PERI = clk_sys. Used as reference clock for Peripherals. No dividers so just select and enable
            clock_configure(clk_peri,
                            0,
                            CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                            12'000'000,
                            12'000'000);


            pll_deinit(pll_sys);
            pll_deinit(pll_usb);





            while (false)
            {
                Pin::Configure(0, 25, Pin::Type::OUTPUT, 1);
                PAL.DelayBusy(100);
                Pin::Configure(0, 25, Pin::Type::OUTPUT, 0);
                PAL.DelayBusy(100);
            }

            uart_set_baudrate(uart0, UartGetBaud(UART::UART_0));
            uart_set_irq_enables(uart0, true, false);

            KTime::SetScalingFactor(12.0 / 125.0);



            PrintAll();


            Log("ROSC CTRL  : ", Format::ToBin(rosc_hw->ctrl));
            Log("ROSC STATUS: ", Format::ToBin(rosc_hw->status));

            PAL.Delay(500);


            // rosc_disable();  // 200 uA, I'd rather leave it running so resets work


            PrintAll();


            Log("ROSC CTRL  : ", Format::ToBin(rosc_hw->ctrl));
            Log("ROSC STATUS: ", Format::ToBin(rosc_hw->status));



            // return;

            // rtc_sleep();

            // return;
















            datetime_t t = {
                    .year  = 2020,
                    .month = 06,
                    .day   = 05,
                    .dotw  = 5, // 0 is Sunday, so 5 is Friday
                    .hour  = 15,
                    .min   = 45,
                    .sec   = 00
            };

            // Alarm 10 seconds later
            datetime_t t_alarm = {
                    .year  = 2020,
                    .month = 06,
                    .day   = 05,
                    .dotw  = 5, // 0 is Sunday, so 5 is Friday
                    .hour  = 15,
                    .min   = 45,
                    .sec   = 5
            };

            // Start the RTC
            rtc_init();

            Log("Sleeping");
            PAL.Delay(50);

            // sleep_goto_sleep_until(&t_alarm, nullptr);

                volatile irq_handler_t *vtor = (volatile irq_handler_t *)scb_hw->vtor;
                volatile uint32_t *ptrHandler = (volatile uint32_t *)&(vtor[VTABLE_FIRST_IRQ + RTC_IRQ]);
                Log("test: ", &vtor[VTABLE_FIRST_IRQ + RTC_IRQ] - &vtor[0]);
                Log("sizeof(*vtor) : ", sizeof(*vtor));
                Log("VTOR          : ", Format::ToHex((uint32_t)vtor), ", ", Format::ToBin((uint32_t)vtor));
                Log("&VTOR[RTC_IRQ]: ", Format::ToHex((uint32_t)ptrHandler), ", ", Format::ToBin((uint32_t)ptrHandler));
                Log("diff: ", (uint32_t)ptrHandler - (uint32_t)vtor);
                Log("VTOR[RTC_IRQ] : ", Format::ToHex(*ptrHandler), ", ", Format::ToBin(*ptrHandler));
                Log("Callback      : ", Format::ToHex((uint32_t)DoNothing), ", ", Format::ToBin((uint32_t)DoNothing));
                *ptrHandler = (uint32_t)DoNothing;
                __dmb();
                Log("VTOR[RTC_IRQ] : ", Format::ToHex(*ptrHandler), ", ", Format::ToBin(*ptrHandler));
                PAL.Delay(500);

            clocks_hw->sleep_en0 = CLOCKS_SLEEP_EN0_CLK_RTC_RTC_BITS;
            clocks_hw->sleep_en1 = 0x0;

            rtc_set_datetime(&t);
            rtc_set_alarm(&t_alarm, DoNothing);


                Log("VTOR[RTC_IRQ] : ", Format::ToHex(*ptrHandler), ", ", Format::ToBin(*ptrHandler));
                *ptrHandler = (uint32_t)DoNothing;
                __dmb();
                Log("VTOR[RTC_IRQ] : ", Format::ToHex(*ptrHandler), ", ", Format::ToBin(*ptrHandler));
                PAL.Delay(500);



            uint save = scb_hw->scr;
            // Enable deep sleep at the proc
            scb_hw->scr = save | M0PLUS_SCR_SLEEPDEEP_BITS;

            // Go to sleep
            __wfi();


            // mine, trying to get reset to work again
            scb_hw->scr = save;
            rosc_write(&rosc_hw->ctrl, ROSC_CTRL_ENABLE_BITS);  // rosc_enable basically













        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("clk.stop", [](vector<string> argList) {
            clock_stop(clk_rtc);
            // clock_stop(clk_usb); // prevents USB plug-in event
        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("clk.wake.regs", [](vector<string> argList) {
            Log("WAKE_EN0: ", Format::ToBin(clocks_hw->wake_en0));
            Log("WAKE_EN1: ", Format::ToBin(clocks_hw->wake_en1));
        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("clk.sleep.regs", [](vector<string> argList) {
            Log("SLEEP_EN0: ", Format::ToBin(clocks_hw->sleep_en0));
            Log("SLEEP_EN1: ", Format::ToBin(clocks_hw->sleep_en1));
        }, { .argCount = 0, .help = "" });

        Shell::AddCommand("clk.vreg", [](vector<string> argList) {

            double v = atof(argList[0].c_str());

            if      (v < 0.85) { v = 0.85; }
            else if (v > 1.30) { v = 1.30; }

            Log("req ", argList[0], ", got ", v);

            // handles increments of 0.05 starting at 0.85
            uint8_t vregVal = VREG_VOLTAGE_0_85 + (uint8_t)((v - 0.85) / 0.05);

            Log("vregVal: ", Format::ToBin(vregVal));

            vreg_set_voltage((vreg_voltage)vregVal);
        }, { .argCount = 1, .help = "set vreg to 0.85 <= x <= 1.30" });


    }




private:
};

