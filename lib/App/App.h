#pragma once

// #include "Ble.h"
// #include "Esb.h"
#include "Evm.h"
// #include "Filesystem.h"
// #include "Flash.h"
// #include "I2C.h"
#include "JSONMsgRouter.h"
#include "KMessagePassing.h"
#include "KTask.h"
#include "Log.h"
// #include "MPSL.h"
#include "PAL.h"
#include "Pin.h"
// #include "PWM.h"
#include "Shell.h"
#include "Timeline.h"
#include "USB.h"
#include "Utl.h"
// #include "VersionStr.h"
#include "WDT.h"
#include "Work.h"


class App
{
public:
    App()
    {
        NukeAppStorageFlashIfFirmwareChanged();

        // Init
        TimelineInit();
        LogInit();
        UartInit();
        PALInit();
        // FlashStoreInit();
        EvmInit();
        JSONMsgRouterInit();

        // Shell
        EvmSetupShell();
        JSONMsgRouterSetupShell();
        LogSetupShell();
        PALSetupShell();
        PinSetupShell();
        TimelineSetupShell();
        UartSetupShell();
        UtlSetupShell();
        WatchdogSetupShell();
        WorkSetupShell();

        // JSON
        JSONMsgRouterSetupJSON();
        PALSetupJSON();
        ShellSetupJSON();
    }

    void Run()
    {
        USB::Init();
        ShellInit();

        KTask<1000> t("App", []{
            Evm::MainLoop();
        }, 10);

        vTaskStartScheduler();
    }

private:
    // TODO
    void NukeAppStorageFlashIfFirmwareChanged()
    {
    #if 0
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

        uint64_t versionStored;

        // try to read the stored version, if there is one
        bool hideWarningIfNotFound = true;
        ssize_t ret = FlashStore::Read(0, &versionStored, sizeof(versionStored), hideWarningIfNotFound);

        // keep track of the upcoming decision about whether to store current version
        bool storeCurrentVersion = false;

        // evaluate attempt to get stored version number
        if (ret > 0)    // found
        {
            if (versionStored == version)
            {
                // do nothing, this is ok
            }
            else
            {
                Log("INF: Firmware upgrade detected, deleting app flash storage");
                Log("  Old version: ", versionStored);
                Log("  New version: ", version);
                LogNL();

                FlashStore::EraseAll();

                LogNL();

                storeCurrentVersion = true;
            }
        }
        else    // not found
        {
            storeCurrentVersion = true;
        }

        if (storeCurrentVersion)
        {
            FlashStore::Write(0, &version, sizeof(version));
        }
    #endif
    }
};




