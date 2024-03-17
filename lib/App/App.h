#pragma once

#include "ADCInternal.h"
#include "Ble.h"
#include "Evm.h"
#include "FilesystemLittleFS.h"
#include "Flashable.h"
#include "I2C.h"
#include "JSONMsgRouter.h"
#include "KMessagePassing.h"
#include "KTask.h"
#include "Log.h"
#include "PAL.h"
#include "Pin.h"
#include "PWM.h"
#include "MYRP2040.h"
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
    App()
    {
        // Init in specific sequence
        TimelineInit();
        LogInit();
        UartInit();
        PALInit();
        LogNL();
        FilesystemLittleFS::Init();
        LogNL();
        NukeAppStorageFlashIfFirmwareChanged();

        // Init everything else
        ADC::Init();
        EvmInit();
        I2C::Init();
        JSONMsgRouter::Init();
        RP2040::Init();

        // Shell
        ADC::SetupShell();
        Ble::SetupShell();
        EvmSetupShell();
        FilesystemLittleFS::SetupShell();
        I2C::SetupShell();
        JSONMsgRouter::SetupShell();
        LogSetupShell();
        PALSetupShell();
        PinSetupShell();
        PWM::SetupShell();
        RP2040::SetupShell();
        Shell::Init();
        TimelineSetupShell();
        UartSetupShell();
        UtlSetupShell();
        WatchdogSetupShell();
        WorkSetupShell();

        // JSON
        JSONMsgRouter::SetupJSON();
        PALSetupJSON();
        Shell::SetupJSON();
    }

    void Run()
    {
        // let app instantiate and potentially configure
        // some core systems
        T t;

        // init configurable core systems
        USB::Init();
        LogNL();

        string board = PICO_BOARD_ACTUAL;
        if (board == "pico_w")
        {
            Ble::Init();
            LogNL();
        }

        // run app code which depends on prior init
        t.Run();

        // make shell visible
        LogNL();
        Shell::DisplayOn();

        // start whole application
        KTask<2000> task("Application", []{
            Log("Application running");
            Evm::MainLoop();
        }, 10);

        vTaskStartScheduler();
    }

private:
    
    void NukeAppStorageFlashIfFirmwareChanged()
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




