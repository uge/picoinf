#include "WDT.h"
#include "Timeline.h"


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

// I'm using the NRF registers because (as usual) the zephyr libs are dogshit
// The nrfx libs aren't much better


// Nevermind used the zephyr libs just copying an example basically


#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>

#include "Log.h"
#include "Shell.h"


const device *WDTDevice()
{
    return DEVICE_DT_GET(DT_NODELABEL(wdt0));
}

void Watchdog::SetTimeout(uint32_t timeoutMs)
{
    if (!running_)
    {
        timeoutMs_ = timeoutMs;
    }
}

void Watchdog::Start()
{
    if (!running_)
    {
        struct wdt_timeout_cfg wdt_config = {
            .window = {
                .min = 0U,          // ignore
                .max = timeoutMs_,  // configured timeout
            },
            .callback = nullptr,
            .flags = WDT_FLAG_RESET_SOC,    // hardest reset possible
        };

        channelId_ = wdt_install_timeout(WDTDevice(), &wdt_config);

        wdt_setup(WDTDevice(), WDT_OPT_PAUSE_HALTED_BY_DBG);

        running_ = true;
    }
}

void Watchdog::Stop()
{
    // intentionally does not attempt to see if already running.
    // this allows for on-restart stopping, where this code will have
    // no memory of whether a prior run enabled the watchdog.
    //
    // the running bool does prevent multi-start

    running_ = false;

    wdt_disable(WDTDevice());
}

void Watchdog::Feed()
{
    if (running_)
    {
        wdt_feed(WDTDevice(), channelId_);
    }
}

// ok, stopping not possible seemingly (-EPERM),
// and not sure the underlying hw supports it.
// I don't care.  It's going to run forever anyway.


////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////

int WatchdogInit()
{
    Timeline::Global().Event("WatchdogInit");

    // We want the watchdog to not be enabled until the application decides
    // it should be running.
    //
    // The prior run of the application may or may not have been running
    // the watchdog.
    // If the watchdog killed/reset the device, no action needed here.
    // If the system reset itself, then the watchdog is running now and
    // will kill the system eventually.
    //
    // Here we start then stop immediately in order to prevent the watchdog
    // from killing the system when the application didn't interact with it yet.
    // 
    // This is necessary because unconditionally stopping doesn't work, as the
    // underlying zephyr libs either don't support it or something else, but
    // this does work. 
    Watchdog::Start();
    Watchdog::Stop();

    return 1;
}

int WatchdogSetupShell()
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

    return 1;
}


#include <zephyr/init.h>
SYS_INIT(WatchdogInit, APPLICATION, 14);
SYS_INIT(WatchdogSetupShell, APPLICATION, 80);
