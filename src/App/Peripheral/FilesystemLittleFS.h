#pragma once

#include "FilesystemLittleFSFile.h"
#include "Timeline.h"

#include "hardware/flash.h"

#include "lfs.h"


/*

flash nuking happens when?
    version change
        this will need to get detected

interactive sub-shell for exploring the filesystem
    how to superscede the existing shell?
        stack of shells?

later on, serialized objects will just be filenames


*/




class FilesystemLittleFS
{
public:

    /////////////////////////////////////////////////////////////////
    // Init
    /////////////////////////////////////////////////////////////////

    static void Init();
    static void SetupShell();


public:

    /////////////////////////////////////////////////////////////////
    // File Accessor Commands
    /////////////////////////////////////////////////////////////////

    static FilesystemLittleFSFile GetFile(const string &fileName)
    {
        return FilesystemLittleFSFile{lfs_, fileName};
    }


    /////////////////////////////////////////////////////////////////
    // General Commands
    /////////////////////////////////////////////////////////////////

    static void NukeFilesystem()
    {
        UnMount();
        Format();
        Mount();
    }

    struct DirEnt
    {
        enum class Type : uint8_t
        {
            DIR  = LFS_TYPE_DIR,
            FILE = LFS_TYPE_REG,
        };

        Type     type = Type::FILE;
        uint32_t size = 0;
        string   name;
    };

    static bool List(const string &path, vector<DirEnt> &dirEntList)
    {
        bool retVal = false;

        DirEnt dirEntFirstGlance;
        if (Stat(path, dirEntFirstGlance))
        {
            if (dirEntFirstGlance.type == DirEnt::Type::DIR)
            {
                static lfs_dir_t dir;
                int errOpen = lfs_dir_open(&lfs_, &dir, path.c_str());
                if (errOpen < 0)
                {
                    Err("List lfs_dir_open", errOpen);
                }
                else
                {
                    retVal = true;

                    lfs_info info;
                    while (lfs_dir_read(&lfs_, &dir, &info) > 0)
                    {
                        DirEnt dirEnt;
                        LfsInfoToDirEnt(info, dirEnt);

                        if (dirEnt.name != "." && dirEnt.name != "..")
                        {
                            dirEntList.push_back(dirEnt);
                        }
                    }

                    int errClose = lfs_dir_close(&lfs_, &dir);
                    if (errClose < 0)
                    {
                        Err("List lfs_dir_close", errOpen);
                    }
                }
            }
            else
            {
                dirEntList.push_back(dirEntFirstGlance);
                retVal = true;
            }
        }
        else
        {
            retVal = false;
        }

        return retVal;
    }

    static bool Stat(const string &path, DirEnt &dirEnt)
    {
        bool retVal = false;

        lfs_info info;
        int err = lfs_stat(&lfs_, path.c_str(), &info);
        if (err)
        {
            if (err != LFS_ERR_NOENT)
            {
                Err("Stat lfs_stat", err);
            }
        }
        else
        {
            LfsInfoToDirEnt(info, dirEnt);
            retVal = true;
        }

        return retVal;
    }

    static bool FileExists(const string &path)
    {
        bool retVal = false;

        DirEnt dirEnt;
        if (Stat(path, dirEnt))
        {
            retVal = dirEnt.type == DirEnt::Type::FILE;
        }

        return retVal;
    }

    static string Read(const string &path)
    {
        string retVal;

        if (FileExists(path))
        {
            auto f = FilesystemLittleFS::GetFile(path);

            if (f.Open())
            {
                f.Read(retVal);

                f.Close();
            }
        }

        return retVal;
    }

    /////////////////////////////////////////////////////////////////
    // File Commands
    /////////////////////////////////////////////////////////////////

    static void Touch(string fileName)
    {
        auto f = GetFile(fileName);
        f.Open();
        f.Close();
    }

    static void Trunc(const string &fileName, uint32_t size)
    {
        auto f = GetFile(fileName);
        f.Open();
        f.Trunc(size);
        f.Close();
    }


    /////////////////////////////////////////////////////////////////
    // Directory Commands
    /////////////////////////////////////////////////////////////////

    static bool MkDir(string path)
    {
        bool retVal = false;

        int err = lfs_mkdir(&lfs_, path.c_str());
        if (err)
        {
            if (err != LFS_ERR_EXIST)
            {
                Err("MkDir lfs_mkdir", err);
            }
        }
        else
        {
            retVal = true;
        }

        return retVal;
    }


    /////////////////////////////////////////////////////////////////
    // File and Directory Commands
    /////////////////////////////////////////////////////////////////

    static bool Remove(const string &path)
    {
        bool retVal = false;

        int err = lfs_remove(&lfs_, path.c_str());
        if (err)
        {
            if (err != LFS_ERR_EXIST)
            {
                Err("Remove lfs_remove", err);
            }
        }
        else
        {
            retVal = true;
        }

        return retVal;
    }





















private:

    /////////////////////////////////////////////////////////////////
    // LFS Management
    /////////////////////////////////////////////////////////////////

    static bool Mount();
    static bool UnMount();
    static bool Format();

    /////////////////////////////////////////////////////////////////
    // Utility
    /////////////////////////////////////////////////////////////////

    static void Err(const string &where, int err)
    {
        Log("LFS ERR: ", where, " : ", err);
    }

    static void LfsInfoToDirEnt(lfs_info &info, DirEnt &dirEnt)
    {
        dirEnt.type = (DirEnt::Type)info.type;
        dirEnt.size = info.size;
        dirEnt.name = info.name;
    }


    /////////////////////////////////////////////////////////////////
    // Hook into RPi Pico Flash
    /////////////////////////////////////////////////////////////////

    // took pico flash operation code from here:
    // https://github.com/lurk101/littlefs-lib/blob/master/pico_hal.c

    static int Read(const lfs_config *c,
                    lfs_block_t       block,
                    lfs_off_t         off,
                    void             *buffer,
                    lfs_size_t        size)
    {
        // Log("Read block ", block, ", off ", off, ", size ", size);

        assert(block < lfsCfg_.block_count);
        assert(off + size <= lfsCfg_.block_size);
        // read flash via XIP mapped space
        memcpy(buffer, (const void *)(FS_BASE + XIP_NOCACHE_NOALLOC_BASE + (block * lfsCfg_.block_size) + off), size);

        return LFS_ERR_OK;
    }

    static int Prog(const lfs_config *c,
                    lfs_block_t       block,
                    lfs_off_t         off,
                    const void       *buffer,
                    lfs_size_t        size)
    {
        // Log("Prog block ", block, ", off ", off, ", size ", size);

        assert(block < lfsCfg_.block_count);

        IrqLock lock;
        uint32_t p = (uint32_t)FS_BASE + (block * lfsCfg_.block_size) + off;
        flash_range_program(p, (const uint8_t *)buffer, size);

        return LFS_ERR_OK;
    }

    static int Erase(const lfs_config *c, lfs_block_t block)
    {
        // Log("Erase block ", block);

        assert(block < lfsCfg_.block_count);
        
        IrqLock lock;
        uint32_t p = (uint32_t)FS_BASE + block * lfsCfg_.block_size;
        flash_range_erase(p, lfsCfg_.block_size);

        return LFS_ERR_OK;
    }

    static int Sync(const lfs_config *c)
    {
        // Log("Sync");

        return LFS_ERR_OK;
    }


/*

// Pico flash:
// #define FLASH_PAGE_SIZE (1u << 8)        //   256
// #define FLASH_SECTOR_SIZE (1u << 12)     //  4096
// #define FLASH_BLOCK_SIZE (1u << 16)      // 65536

// must erase multiples of sectors (4096)
// must program multiples of pages ( 256)

how to make files inlined?
    "non-inlined files take up at minimum one block"

Inline files live in metadata, and size must be:
- <= cache_size   = CACHE_SIZE_READ_WRITE(1024)
- <= attr_max     = LFS_ATTR_MAX(1022) (when zero) = 1022
- <= block_size/8 = FLASH_SECTOR_SIZE(4096)/8 = 512
- therefore, the max size of an inline file is 512, which is fine

*/

    inline static lfs_t      lfs_;
    inline static lfs_config lfsCfg_;

    static const uint32_t FS_SIZE = 16 * FLASH_SECTOR_SIZE;  // 65,536
    // file system offset in flash
    static const uint32_t FS_BASE = PICO_FLASH_SIZE_BYTES - FS_SIZE;

    static const uint32_t ERASE_CYCLES_BEFORE_METADATA_MOVE = 32;

    // littlefs needs a read cache, a program cache, and one additional
    // cache per-file(!).
    // Must be a multiple of the read and program sizes,
    // and a factor of the block size
    static const uint32_t CACHE_SIZE_READ_WRITE = 512;
    inline static uint8_t readBuf_[CACHE_SIZE_READ_WRITE];
    inline static uint8_t writeBuf_[CACHE_SIZE_READ_WRITE];

    static const uint32_t CACHE_SIZE_LOOKAHEAD = 32;
    inline static uint8_t lookaheadBuf_[CACHE_SIZE_LOOKAHEAD];
};

