#pragma once


#include "Log.h"




#if 1

#include <zephyr.h>
#include <logging/log.h>
#include <usb/usb_device.h>
#include <zephyr/storage/disk_access.h>
#include <fs/fs.h>
#include <ff.h>
#include <stdio.h>

#include <storage/flash_map.h>



static int setup_flash(struct fs_mount_t *mnt)
{
	int rc = 0;
	unsigned int id;
	const struct flash_area *pfa;

	mnt->storage_dev = (void *)FLASH_AREA_ID(storage);
	id = (uintptr_t)mnt->storage_dev;

	rc = flash_area_open(id, &pfa);
	Log("Area ", id,
        " at ", (unsigned int)pfa->fa_off,
        " on ", pfa->fa_dev_name,
        " for ", (unsigned int)pfa->fa_size, " bytes\n");

    Log("Erasing flash area ... ");
    rc = flash_area_erase(pfa, 0, pfa->fa_size);
    Log("  retVal: ", rc);

	if (rc < 0) {
		flash_area_close(pfa);
	}

	return rc;
}

static void setup_disk(void)
{
    static FATFS fat_fs;
    static struct fs_mount_t fs_mnt;
    memset(&fat_fs, 0, sizeof(fat_fs));
    memset(&fs_mnt, 0, sizeof(fs_mnt));

    fs_mnt.type = FS_FATFS;
    fs_mnt.fs_data = &fat_fs;
    // fs_mnt.mnt_point = "/RAM:";
    setup_flash(&fs_mnt);
    fs_mnt.mnt_point = "/NAND:";

    int rc = fs_mount(&fs_mnt);
    if (rc < 0) {
        Log("Failed to mount filesystem");
        return;
    }



    /* Allow log messages to flush to avoid interleaved output */
    k_sleep(K_MSEC(50));

    Log("Mount ", fs_mnt.mnt_point, ": ", rc);



    struct fs_statvfs sbuf;
    rc = fs_statvfs(fs_mnt.mnt_point, &sbuf);
    if (rc < 0) {
        Log("FAIL: statvfs: %d\n", rc);
        return;
    }

    Log(fs_mnt.mnt_point);
    Log("bsize = ", sbuf.f_bsize);
    Log("frsize = ", sbuf.f_frsize);
    Log("blocks = ", sbuf.f_blocks);
    Log("bfree = ", sbuf.f_bfree);


    struct fs_dir_t dir;
    fs_dir_t_init(&dir);
    rc = fs_opendir(&dir, fs_mnt.mnt_point);
    Log(fs_mnt.mnt_point, " opendir: ", rc);

    if (rc < 0) {
        Log("Failed to open directory");
    }

    while (rc >= 0) {
        // struct fs_dirent ent = { 0 };
        struct fs_dirent ent;

        rc = fs_readdir(&dir, &ent);
        if (rc < 0) {
            Log("Failed to read directory entries");
            break;
        }
        if (ent.name[0] == 0) {
            Log("End of files");
            break;
        }
        Log("  ", (ent.type == FS_DIR_ENTRY_FILE) ? 'F' : 'D', " ", ent.size, " ", ent.name);
    }

    (void)fs_closedir(&dir);

    return;
}

void mainfn(void)
{
    setup_disk();

    int ret = usb_enable(NULL);
    if (ret != 0) {
    	Log("Failed to enable USB");
    	return;
    }
    Log("The device is put in USB mass storage mode.");

}


class Filesystem
{
public:
    void Start()
    {
        mainfn();
    }

private:
};

#endif



#if 0

#include <zephyr.h>
#include <device.h>
#include <storage/disk_access.h>
#include <fs/fs.h>
#include <ff.h>


inline static FATFS fat_fs;
/* mounting info */
inline static struct fs_mount_t mp = {
    .type = FS_FATFS,
    .fs_data = &fat_fs,
};

// inline static const char *disk_mount_pt = "/SD:";
inline static const char *disk_mount_pt = "/NAND:";

inline static int lsdir(const char *path)
{
    int res;
    struct fs_dir_t dirp;
    static struct fs_dirent entry;

    fs_dir_t_init(&dirp);

    /* Verify fs_opendir() */
    res = fs_opendir(&dirp, path);
    if (res) {
        printk("Error opening dir %s [%d]\n", path, res);
        return res;
    }

    printk("\nListing dir %s ...\n", path);
    for (;;) {
        /* Verify fs_readdir() */
        res = fs_readdir(&dirp, &entry);

        /* entry.name[0] == 0 means end-of-dir */
        if (res || entry.name[0] == 0) {
            break;
        }

        if (entry.type == FS_DIR_ENTRY_DIR) {
            printk("[DIR ] %s\n", entry.name);
        } else {
            printk("[FILE] %s (size = %zu)\n",
                entry.name, entry.size);
        }
    }

    /* Verify fs_closedir() */
    fs_closedir(&dirp);

    return res;
}


class Filesystem
{
public:

    void Report()
    {
        do
        {
            static const char *disk_pdrv = "SD";
            uint64_t memory_size_mb;
            uint32_t block_count;
            uint32_t block_size;

            if (disk_access_init(disk_pdrv) != 0) {
                Log("Storage init ERROR!");
                break;
            }

            if (disk_access_ioctl(disk_pdrv,
                    DISK_IOCTL_GET_SECTOR_COUNT, &block_count)) {
                Log("Unable to get sector count");
                break;
            }
            Log("Block count %u", block_count);

            if (disk_access_ioctl(disk_pdrv,
                    DISK_IOCTL_GET_SECTOR_SIZE, &block_size)) {
                Log("Unable to get sector size");
                break;
            }
            Log("Sector size ", block_size);

            memory_size_mb = (uint64_t)block_count * block_size;
            Log("Memory Size(MB) ", (uint32_t)(memory_size_mb >> 20));
        } while (0);

        
        mp.mnt_point = disk_mount_pt;

        int res = fs_mount(&mp);

        if (res == FR_OK) {
            Log("Disk mounted.");
            lsdir(disk_mount_pt);
        } else {
            Log("Error mounting disk.");
        }
    }


private:
};
#endif


