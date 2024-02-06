#pragma once

#include <zephyr/drivers/flash.h>
#include <zephyr/fs/nvs.h>

#include "Log.h"


class FlashStore
{
public:

    static void Init();

    static uint16_t GetSectorCount();
    static uint16_t GetBytesPerSector();
    static uint16_t GetTotalSpace();
    static uint16_t GetFreeSpace();

    static uint16_t GetNextId();
    static int EraseAll();
    static ssize_t Read(uint16_t id, void *buf, size_t bufLen, bool hideWarningIfNotFound = false);
    static ssize_t Write(uint16_t id, void *buf, size_t bufLen);
    static int Delete(uint16_t id);

    static void SetupShell();

private:

    inline static const uint16_t DEFAULT_ID_NEXT = 1;

    inline static flash_pages_info flashPageInfo_;
    inline static nvs_fs fs_;
    inline static bool initDone_ = false;
    inline static uint16_t idNext_ = DEFAULT_ID_NEXT;
};




template <typename T>
class Flashable
: public T
{
public:
    Flashable()
    : id_(FlashStore::GetNextId())
    {
        // Nothing to do
    }

    bool Get(bool hideWarningIfNotFound = false)
    {
        // only modify the wrapped type if the lookup worked
        T tmp;
        bool retVal = sizeof(T) == FlashStore::Read(id_, (void *)&tmp, sizeof(T), hideWarningIfNotFound);

        if (retVal)
        {
            // get reference to the wrapped type
            T &ref = *(T *)this;

            ref = tmp;
        }

        return retVal;
    }

    bool Put()
    {
        // get reference to the wrapped type
        T &ref = *(T *)this;

        ssize_t ret = FlashStore::Write(id_, (void *)&ref, sizeof(T));

        return ret == 0 || sizeof(T) == ret;
    }

    bool Delete()
    {
        return 0 == FlashStore::Delete(id_);
    }

private:

    uint16_t id_ = 0;
};


