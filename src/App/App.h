#pragma once

#include "ADCInternal.h"
#if PICO_INF_ENABLE_WIRELESS == 1
#include "Ble.h"
#endif
#include "Clock.h"
#include "Evm.h"
#include "FilesystemLittleFS.h"
#include "Flashable.h"
#include "I2C.h"
#include "JerryScriptIntegration.h"
#include "JSONMsgRouter.h"
#include "KMessagePassing.h"
#include "KStats.h"
#include "KTask.h"
#include "Log.h"
#include "PAL.h"
#include "Pin.h"
#include "PWM.h"
#include "PeripheralControl.h"
#include "Sensor.h"
#include "Shell.h"
#include "Timeline.h"
#include "USB.h"
#include "Utl.h"
#include "VersionStr.h"
#include "WDT.h"
#include "Work.h"


template <typename T>
class App
{
public:
    static void Run()
    {
        // main() stack vanishes on scheduler start, so
        // run the whole application state within the task
        // and make sure the task itself isn't on the main()
        // stack
        static KTask<2000> task("Application", [&]{
            // Init in specific sequence
            Timeline::Init();
            LogInit();
            UartInit();
            PlatformAbstractionLayer::Init();
            LogNL();
            FilesystemLittleFS::Init();
            LogNL();
            NukeAppStorageFlashIfFirmwareChanged();

            // Init everything else
            ADC::Init();
            Clock::Init();
            EvmInit();
            I2C::Init0();
            JerryScriptIntegration::Init();
            JSONMsgRouter::Init();
            KStats::Init();

            // Shell
            ADC::SetupShell();
#if PICO_INF_ENABLE_WIRELESS == 1
            Ble::SetupShell();
#endif
            Clock::SetupShell();
            EvmSetupShell();
            FilesystemLittleFS::SetupShell();
            I2C::SetupShell0();
            JerryScriptIntegration::SetupShell();
            JSONMsgRouter::SetupShell();
            KStats::SetupShell();
            LogSetupShell();
            PlatformAbstractionLayer::SetupShell();
            PinSetupShell();
            PWM::SetupShell();
            PeripheralControl::SetupShell();
            Sensor::SetupShell();
            Shell::Init();
            Timeline::SetupShell();
            UartSetupShell();
            USB::SetupShell();
            UtlSetupShell();
            Watchdog::SetupShell();
            Work::SetupShell();

            // JSON
            JSONMsgRouter::SetupJSON();
            PlatformAbstractionLayer::SetupJSON();
            Shell::SetupJSON();

            // let app instantiate and potentially configure
            // some core systems
            T t;

            // init configurable core systems
            USB::Init();
            LogNL();

#if PICO_INF_ENABLE_WIRELESS == 1
            Ble::Init();
            LogNL();
#endif

            // run app code which depends on prior init
            t.Run();

            Log("Event Manager Start");

            // make shell visible
            LogNL();
            Shell::DisplayOn();

            Evm::MainLoop();
        }, 10);

        vTaskStartScheduler();
    }

private:
    
    static void NukeAppStorageFlashIfFirmwareChanged()
    {
        // We of course want app settings to persist across reboots.
        //
        // But we don't want that if the firmware gets upgraded.
        //   This totally invalidates the auto-id-allocator scheme
        //   going at present (things could be allocated in a different
        //   order, reading back in another object's data).
        //
        // So the scheme is:
        // - special NVS slot 0 is reserved, we're using it
        // - check if any version number stored there
        //   - if yes, compare to current version number
        //     - if different, nuke
        //     - store current version number
        //   - else (if not stored)
        //     - store current version number
        //
        // The end result is
        // - upon first start ever, version stored
        // - upon reboot of same firmware, no change
        // - upon firmware upgrade, the app storage is nuked, new version stored

        // The version is a build date of YYYY-MM-DD.
        // Let's turn that into a big integer for easy storing, retrieval, comparing.


        uint64_t version = Version::GetVersionAsInt();

        // uint64_t versionStored;
        struct VersionData
        {
            uint64_t version = 0;
        };
        Flashable<VersionData> versionData(0);

        // try to read the stored version, if there is one
        bool hideWarningIfNotFound = true;
        bool ret = versionData.Get(hideWarningIfNotFound);

        // keep track of the upcoming decision about whether to store current version
        bool storeCurrentVersion = false;

        // evaluate attempt to get stored version number
        if (ret)    // found
        {
            if (versionData.version == version)
            {
                // do nothing, this is ok
            }
            else
            {
                Log("INF: Firmware upgrade detected, deleting app flash storage");
                Log("  Old version: ", versionData.version);
                Log("  New version: ", version);

                FilesystemLittleFS::NukeFilesystem();

                LogNL();

                storeCurrentVersion = true;
            }
        }
        else    // not found
        {
            Log("INF: Firmware first run, formatting app flash storage");

            FilesystemLittleFS::NukeFilesystem();

            LogNL();

            storeCurrentVersion = true;
        }

        if (storeCurrentVersion)
        {
            versionData.version = version;
            versionData.Put();
        }
    }
};




