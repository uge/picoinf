#pragma once


#include "hardware/flash.h"

#include "lfs.h"


class FilesystemLittleFS
{
public:


// Pico flash:
// #define FLASH_PAGE_SIZE (1u << 8)        //   256
// #define FLASH_SECTOR_SIZE (1u << 12)     //  4096
// #define FLASH_BLOCK_SIZE (1u << 16)      // 65536

// must erase multiples of sectors (4096)
// must program multiples of pages ( 256)

    static void Init()
    {
        struct lfs_config cfg;
    }

private:
};

