#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>
// #include <zephyr.h>

#include "Flash.h"
#include "Log.h"
#include "Shell.h"
#include "Timeline.h"


/*

-----------
Persistence
-----------
Flash from VSCode wipes everything.
Adafruit bootloader scenarios tbd.


------
Sizing
------
4064 blob write max size I can accomplish before -22 invalid argument


---------------
Latency Testing
---------------

32k filesystem
--------------
init - 250ms
erase all - 1.25 sec (250ms of which is re-init)
free - 60us


400 byte structure
------------------
put - 6ms
put (not needed) - 300us
get - 200us
del - 850us
get (miss) - 300us


2000 byte structure
-------------------
put - 25ms
put (not needed) - 900us
get - 350us
del - 800us
get (miss) - 300


4064 byte structure
-------------------
put - 140ms
get - 400us
del - 850us


*/

#define PARTITION app_storage_partition


uint16_t FlashStore::GetSectorCount()
{
    return FIXED_PARTITION_SIZE(PARTITION) / GetBytesPerSector();
}

uint16_t FlashStore::GetBytesPerSector()
{
    return flashPageInfo_.size;
}

uint16_t FlashStore::GetTotalSpace()
{
    return GetSectorCount() * GetBytesPerSector();
}

uint16_t FlashStore::GetFreeSpace()
{
    Init();

    uint16_t retVal = 0;

    ssize_t ret = nvs_calc_free_space(&fs_);
    if (ret >= 0)
    {
        retVal = ret;
    }

    return retVal;
}

uint16_t FlashStore::GetNextId()
{
    uint16_t retVal = idNext_;

    ++idNext_;

    return retVal;
}

int FlashStore::EraseAll()
{
    Init();

    int retVal = nvs_clear(&fs_);

    if (retVal == 0)
    {
        // Log("EraseAll ok");
    }
    else
    {
        Log("EraseAll ERR: ", retVal, ": ", strerror(-retVal));
    }

    // this wrecks the prior mount, so re-establish it
    initDone_ = false;
    Init();

    return retVal;
}

ssize_t FlashStore::Read(uint16_t id, void *buf, size_t bufLen, bool hideWarningIfNotFound)
{
    Init();

    ssize_t retVal = nvs_read(&fs_, id, buf, bufLen);

    if (retVal < 0)
    {
        if (hideWarningIfNotFound != true)
        {
            Log("Read ERR: ", retVal, ", ", strerror(-retVal));
        }
    }
    else if ((size_t)retVal == bufLen)
    {
        // Log("Read ok");
    }
    else
    {
        Log("Read partial");
    }

    return retVal;
}

ssize_t FlashStore::Write(uint16_t id, void *buf, size_t bufLen)
{
    Init();

    ssize_t retVal = nvs_write(&fs_, id, buf, bufLen);

    if ((size_t)retVal == bufLen)
    {
        // Log("Write ok");
    }
    else if (retVal == 0)
    {
        // Log("Write not required");
    }
    else if (retVal < 0)
    {
        Log("Write ERR: ", retVal, ", ", strerror(-retVal));
    }
    else
    {
        Log("Write unknown state");
    }

    return retVal;
}

int FlashStore::Delete(uint16_t id)
{
    Init();

    int retVal = nvs_delete(&fs_, id);

    if (retVal == 0)
    {
        // Log("Delete ok");
    }
    else if (retVal < 0)
    {
        Log("Delete ERR: ", retVal);
    }
    else
    {
        Log("Delete unkown state");
    }

    return retVal;
}








////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////

void FlashStore::Init()
{
    if (!initDone_)
    {
        initDone_ = true;

        fs_.flash_device = FIXED_PARTITION_DEVICE(PARTITION);
        fs_.offset = FIXED_PARTITION_OFFSET(PARTITION);

        if (!device_is_ready(fs_.flash_device))
        {
            Log("Flash device ", fs_.flash_device->name, " is not ready");
            return;
        }

        int rc = flash_get_page_info_by_offs(fs_.flash_device, fs_.offset, &flashPageInfo_);
        if (rc)
        {
            Log("Unable to get page info");
            return;
        }

        fs_.sector_size = GetBytesPerSector();
        fs_.sector_count = GetSectorCount();

        rc = nvs_mount(&fs_);
        if (rc) {
            Log("Flash Init failed");
            return;
        }

        LogNL();
        Log("Non-Volatile Storage Mounted");
        uint32_t allocSize = GetTotalSpace();
        Log("  Allocated: ", Commas(allocSize), " (", GetSectorCount(), " sectors of ", GetBytesPerSector(), " bytes)");

        // this takes like 60 seconds or more on rpi, so we're just going to not.
        // uint32_t freeSpace = GetFreeSpace();
        // Log("  Free     : ", Commas(freeSpace), " (", freeSpace * 100 / allocSize," %)");
    }
}

void FlashStore::SetupShell()
{
    Shell::AddCommand("nvs.init", [&](vector<string> argList){
        Timeline t;
        t.Reset();
        t.Event("start");
        FlashStore::Init();
        t.Event("stop");
        t.ReportNow();
    }, { .argCount = 0, .help = ""});

    Shell::AddCommand("nvs.eraseall", [&](vector<string> argList){
        Timeline t;
        t.Reset();
        t.Event("start");
        FlashStore::EraseAll();
        t.Event("stop");
        t.ReportNow();
    }, { .argCount = 0, .help = ""});

    Shell::AddCommand("nvs.free", [&](vector<string> argList){
        Timeline t;
        t.Reset();
        t.Event("start");
        uint32_t freeSpace = FlashStore::GetFreeSpace();
        t.Event("stop");
        Log("Free space: ", freeSpace);
        t.ReportNow();
    }, { .argCount = 0, .help = ""});

    Shell::AddCommand("nvs.put", [&](vector<string> argList){
        Timeline t;

        uint16_t id = atoi(argList[0].c_str());
        uint16_t len = atoi(argList[1].c_str());
        uint8_t val = atoi(argList[2].c_str());

        uint8_t *buf = new uint8_t[len];
        memset(buf, 0, len);
        buf[0] = val;

        t.Reset();
        t.Event("start");
        FlashStore::Write(id, buf, len);
        t.Event("stop");
        t.ReportNow();

        delete[] buf;
    }, { .argCount = 3, .help = ""});

    Shell::AddCommand("nvs.get", [&](vector<string> argList){
        Timeline t;

        uint16_t id = atoi(argList[0].c_str());
        uint16_t len = atoi(argList[1].c_str());

        uint8_t *buf = new uint8_t[len];
        memset(buf, 0, len);

        t.Reset();
        t.Event("start");
        FlashStore::Read(id, buf, len);
        t.Event("stop");

        Log("Val: ", buf[0]);

        t.ReportNow();

        delete[] buf;
    }, { .argCount = 2, .help = ""});

    Shell::AddCommand("nvs.set.int", [&](vector<string> argList){
        uint16_t id = atoi(argList[0].c_str());
        uint64_t val = atoi(argList[1].c_str());

        ssize_t ret = FlashStore::Write(id, &val, sizeof(val));

        if (ret > 0)
        {
            Log("Val stored ok");
        }
        else
        {
            Log("Could not store value at id ", id, ", ret: ", ret);;
        }
    }, { .argCount = 2, .help = "set id <x> to be uint64_t value <y>"});

    Shell::AddCommand("nvs.get.int", [&](vector<string> argList){
        uint16_t id = atoi(argList[0].c_str());

        uint64_t val = 0;

        ssize_t ret = FlashStore::Read(id, &val, sizeof(val));

        if (ret > 0)
        {
            Log("Val: ", val);
        }
        else
        {
            Log("Could not retrieve value at id ", id, ", ret: ", ret);
        }
    }, { .argCount = 1, .help = "get uint64_t stored with id <x>"});

    Shell::AddCommand("nvs.del", [&](vector<string> argList){
        Timeline t;

        uint16_t id = atoi(argList[0].c_str());

        t.Reset();
        t.Event("start");
        FlashStore::Delete(id);
        t.Event("stop");
        t.ReportNow();
    }, { .argCount = 1, .help = ""});

    Shell::AddCommand("nvs.count", [&](vector<string> argList){
        Log(idNext_ - DEFAULT_ID_NEXT, " IDs allocated");
    }, { .argCount = 0, .help = ""});

    Shell::AddCommand("nvs.touch", [&](vector<string> argList){
        uint32_t touchCount = atol(argList[0].c_str());

        Log("There are ", GetSectorCount(), " sectors of ", GetBytesPerSector(), " bytes each for ", GetTotalSpace(), " bytes");
        Log("Writing a 32-bit value ", Commas(touchCount), " times");

        struct Data
        {
            uint32_t val = 0;
        };

        Flashable<Data> data;

        uint8_t pctDoneLast = 0;
        for (uint64_t i = 0; i < touchCount; ++i)
        {
            data.val = i;
            data.Put();

            uint8_t pctDone = (i + 1) * 100 / touchCount;

            while (pctDone >= (pctDoneLast + 10))
            {
                pctDoneLast += 10;

                Log(pctDoneLast, "\% done");
            }
        }
    }, { .argCount = 1, .help = ""});

    struct Test1
    {
        uint32_t val = 1;
        uint32_t valList[750];
    };

    static Flashable<Test1> test1;

    Shell::AddCommand("nvs.1.set", [&](vector<string> argList){
        Log("Orig set to ", test1.val);
        test1.val = atol(argList[0].c_str());
        Log("Now set to ", test1.val);
    }, { .argCount = 1, .help = ""});

    Shell::AddCommand("nvs.1.put", [&](vector<string> argList){
        Timeline t;
        t.Reset();
        t.Event("start");
        test1.Put();
        t.Event("stop");
        t.ReportNow();
    }, { .argCount = 0, .help = ""});

    Shell::AddCommand("nvs.1.show", [&](vector<string> argList){
        Log("Now set to ", test1.val);
    }, { .argCount = 0, .help = ""});

    Shell::AddCommand("nvs.1.get", [&](vector<string> argList){
        Log("Orig set to ", test1.val);
        Timeline t;
        t.Reset();
        t.Event("start");
        test1.Get();
        t.Event("stop");
        Log("Now set to ", test1.val);
        t.ReportNow();
    }, { .argCount = 0, .help = ""});

    Shell::AddCommand("nvs.1.del", [&](vector<string> argList){
        Timeline t;
        t.Reset();
        t.Event("start");
        test1.Delete();
        t.Event("stop");
        t.ReportNow();
    }, { .argCount = 0, .help = ""});
}


int FlashStoreInit()
{
    Timeline::Global().Event("FlashStoreInit");
    
    FlashStore::Init();

    return 1;
}

int FlashStoreSetupShell()
{
    Timeline::Global().Event("FlashStoreSetupShell");

    FlashStore::SetupShell();

    return 1;
}


#include <zephyr/init.h>
SYS_INIT(FlashStoreInit, APPLICATION, 50);
SYS_INIT(FlashStoreSetupShell, APPLICATION, 80);
