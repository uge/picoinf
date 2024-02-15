#pragma once


#include "Timeline.h"

#include "hardware/flash.h"

#include "lfs.h"

#include <memory>
using namespace std;


/*

flash nuking happens when?
    version change
        this will need to get detected

interactive sub-shell for exploring the filesystem
    how to superscede the existing shell?
        stack of shells?

later on, serialized objects will just be filenames


*/


// Forward declaration
class FilesystemLittleFS;


/////////////////////////////////////////////////////////////////////
// FilesystemLittleFSFile
/////////////////////////////////////////////////////////////////////

class FilesystemLittleFSFile
{
    friend class FilesystemLittleFS;

private:
    FilesystemLittleFSFile(lfs_t &lfs, string fileName)
    : state_(make_shared<State>(lfs, fileName))
    {
        // Nothing to do
    }

    void ErrNotOpen(string where)
    {
        Log(where, " ERR: ", state_->fileName, " not open");
    }

    void ErrNo(string where, int err)
    {
        Log(where, " ERR: ", err, " on ", state_->fileName);
    }

    void ErrWrongBytes(string where, int wrong, uint32_t right)
    {
        Log(where, " ERR: Byte mismatch - ", wrong, " bytes instead of ", right, " to ", state_->fileName);
    }


public:

    ~FilesystemLittleFSFile()
    {
        if (state_.use_count() == 1 && state_->open)
        {
            Log("~FilesystemLittleFSFile ERR: Closing unclosed file ", state_->fileName);
            Close();
        }
    }

    bool Open(int flags = LFS_O_RDWR | LFS_O_CREAT)
    {
        if (!state_->open)
        {
            int err = lfs_file_open(&state_->lfs, &state_->file, state_->fileName.c_str(), flags);
            if (err)
            {
                ErrNo("Open", err);
            }

            state_->open = !err;
        }

        return state_->open;
    }

    bool Close()
    {
        bool retVal = false;

        if (state_->open)
        {
            int err = lfs_file_close(&state_->lfs, &state_->file);
            if (err)
            {
                ErrNo("Close", err);
            }

            retVal = err == LFS_ERR_OK;

            state_->open = false;
        }
        else
        {
            ErrNotOpen("Close");
        }

        return retVal;
    }

    bool Read(uint8_t *buf, uint32_t bufSize, uint32_t *bytesRead = nullptr)
    {
        bool retVal = false;

        if (bytesRead)
        {
            *bytesRead = 0;
        }

        if (state_->open)
        {
            int err = lfs_file_read(&state_->lfs, &state_->file, buf, bufSize);
            if (err < 0)
            {
                ErrNo("Read", err);
            }
            else
            {
                if (err == bufSize)
                {
                    // the bytes we wanted to write were written
                    retVal = true;
                }
                else
                {
                    // some bytes read, but not the number we wanted
                    ErrWrongBytes("Read", err, bufSize);
                }

                if (bytesRead)
                {
                    *bytesRead = err;
                }
            }
        }
        else
        {
            ErrNotOpen("Read");
        }

        return retVal;
    }

    bool Read(vector<uint8_t> &byteList, uint32_t size, uint32_t *bytesRead = nullptr)
    {
        // size vector to be able to hold additional data
        uint32_t sizeStart = byteList.size();
        byteList.resize(sizeStart + size);

        uint32_t bytesReadLocal = 0;
        bool retVal = Read(&byteList.data()[sizeStart], size, &bytesReadLocal);

        if (bytesRead)
        {
            *bytesRead = bytesReadLocal;
        }

        // reduce size of vector if no or incomplete data
        if (retVal == false)
        {
            byteList.resize(sizeStart + bytesReadLocal);
        }

        return retVal;
    }

    bool Write(uint8_t *buf, uint32_t bufSize, uint32_t *bytesWritten = nullptr)
    {
        Log("Writing ", bufSize, " bytes to ", state_->fileName);
        bool retVal = false;

        if (state_->open)
        {
            int err = lfs_file_write(&state_->lfs, &state_->file, buf, bufSize);
            if (err < 0)
            {
                ErrNo("Write", err);
            }
            else
            {
                if (err == bufSize)
                {
                    // the bytes we wanted to write were written
                    retVal = true;
                }
                else
                {
                    // some bytes written, but not the number we wanted
                    ErrWrongBytes("Write", err, bufSize);
                }

                if (bytesWritten)
                {
                    *bytesWritten = err;
                }
            }
        }
        else
        {
            ErrNotOpen("Write");
        }

        return retVal;
    }

    bool Write(string str)
    {
        return Write((uint8_t *)str.c_str(), (uint32_t)str.length());
    }

    uint32_t Size()
    {
        uint32_t retVal = 0;

        if (state_->open)
        {
            int32_t err = lfs_file_size(&state_->lfs, &state_->file);

            if (err < 0)
            {
                ErrNo("Size", err);
            }
            else
            {
                retVal = err;
            }
        }
        else
        {
            ErrNotOpen("Size");
        }

        return retVal;
    }

    uint32_t Tell()
    {
        uint32_t retVal = 0;

        if (state_->open)
        {
            int32_t err = lfs_file_tell(&state_->lfs, &state_->file);

            if (err < 0)
            {
                ErrNo("Tell", err);
            }
            else
            {
                retVal = err;
            }
        }
        else
        {
            ErrNotOpen("Tell");
        }

        return retVal;
    }


private:

    struct State
    {
        lfs_t      &lfs;
        string      fileName;
        bool        open = false;
        lfs_file_t  file;
    };

    shared_ptr<State> state_;
};


/////////////////////////////////////////////////////////////////////
// FilesystemLittleFS
/////////////////////////////////////////////////////////////////////

class FilesystemLittleFS
{
public:

    static void Init()
    {
        Timeline::Global().Event("FilesystemLittleFS::Init");

        lfsCfg_ = {
            .read  = Read,
            .prog  = Prog,
            .erase = Erase,
            .sync  = Sync,

            .read_size = 1,
            .prog_size = FLASH_PAGE_SIZE,

            .block_size   = FLASH_SECTOR_SIZE,
            .block_count  = FS_SIZE / FLASH_SECTOR_SIZE,
            .block_cycles = ERASE_CYCLES_BEFORE_METADATA_MOVE,

            .cache_size = CACHE_SIZE_READ_WRITE,

            .lookahead_size = CACHE_SIZE_LOOKAHEAD,

            .read_buffer = readBuf_,
            .prog_buffer = writeBuf_,

            .lookahead_buffer = lookaheadBuf_,

            .attr_max   = 0,
            .inline_max = 0,
        };

        // mount filesystem
        int err = lfs_mount(&lfs_, &lfsCfg_);
        Log("Mounting LittleFS");

        // reformat if we can't mount the filesystem
        // this should only happen on the first boot
        if (err)
        {
            LogNNL("  New flash, formatting... ");
            err = lfs_format(&lfs_, &lfsCfg_);

            if (err)
            {
                Log("ERR - format failed, err ", err);
            }
            else
            {
                err = lfs_mount(&lfs_, &lfsCfg_);
                if (err)
                {
                    Log("format OK, re-mounting ERR, ", err);
                }
                else
                {
                    Log("OK");
                }
            }
        }

        LogNL();
    }

    static FilesystemLittleFSFile GetFile(string fileName)
    {
        return FilesystemLittleFSFile{lfs_, fileName};
    }


private:

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

    static const uint32_t FS_SIZE = 4 * FLASH_SECTOR_SIZE;  // 16,384
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

