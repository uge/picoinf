#include "WDT.h"
#include "Timeline.h"
#include "Log.h"
#include "Shell.h"

#include "hardware/watchdog.h"

// uses LFCLK
//   will automatically force the 32.768 kHz RC oscillator on as
//   long as no other 32.768 kHz clock source is running
// Timeout uses the LFCLK, by populating a CRV (counter reload value)
//   timeout secs = ( CRV + 1 ) / 32768
//   range of 458us to 36 hours
// by default, watchdog runs during sleep and debug
//   can be configured to auto-pause during either

// Reloading (feeding) consists of setting a RR reload request register
// 8 of them possible (configurable number)
//  (I intend to use just 1)
// to a special 0x6E524635 value

// The watchdog must be configured before it is started.
// After it is started, the watchdog’s configuration registers,
// which comprise registers CRV, RREN, and CONFIG,
// will be blocked for further configuration.

// Watchdog can be reset from
// - Wakeup from System OFF mode reset
// - Watchdog reset
// - Pin reset
// - Brownout reset
// - Power-on reset

// expected use case:
// - configure timeout
// - start
// - feed, feed, feed

// If the watchdog goes off, I will reset the board (no logging)


void Watchdog::SetTimeout(uint32_t timeoutMs)
{
    timeoutMs_ = timeoutMs;
}

void Watchdog::Start()
{
    watchdog_enable(timeoutMs_, true);
}

void Watchdog::Stop()
{
    hw_clear_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_ENABLE_BITS);
}

void Watchdog::Feed()
{
    watchdog_update();
}

bool Watchdog::CausedReboot()
{
    return watchdog_caused_reboot() || watchdog_enable_caused_reboot();
}



////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////

void WatchdogSetupShell()
{
    Timeline::Global().Event("WatchdogSetupShell");

    Shell::AddCommand("wdt.timeout", [&](vector<string> argList){
        Watchdog w;

        uint32_t timeoutMs = atol(argList[0].c_str());
        Log("timeoutMs ", timeoutMs);

        w.SetTimeout(timeoutMs);
    }, { .argCount = 1, .help = "wdt set timeout <ms>"});

    Shell::AddCommand("wdt.start", [&](vector<string> argList){
        Watchdog w;

        Log("Watchdog start");

        w.Start();
    }, { .argCount = 0, .help = "wdt start"});

    Shell::AddCommand("wdt.stop", [&](vector<string> argList){
        Watchdog w;

        Log("Watchdog stop");

        w.Stop();
    }, { .argCount = 0, .help = "wdt stop"});

    Shell::AddCommand("wdt.feed", [&](vector<string> argList){
        Watchdog w;

        Log("Watchdog feed");

        w.Feed();
    }, { .argCount = 0, .help = "wdt feed"});
}

