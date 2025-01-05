#include "FilesystemLittleFS.h"
#include "PAL.h"
#include "Shell.h"
#include "Timeline.h"
#include "Utl.h"

#include "lfs.h"

#include "hardware/flash.h"

using namespace std;

#include "StrictMode.h"



/////////////////////////////////////////////////////////////////
// Utility
/////////////////////////////////////////////////////////////////

static void Err(const string &where, int err)
{
    Log("LFS ERR: ", where, " : ", err);
}

static void LfsInfoToDirEnt(lfs_info &info, FilesystemLittleFS::DirEnt &dirEnt)
{
    // LFS_TYPE_REG or LFS_TYPE_DIR
    dirEnt.type = info.type == LFS_TYPE_REG              ?
                  FilesystemLittleFS::DirEnt::Type::FILE :
                  FilesystemLittleFS::DirEnt::Type::DIR;
    dirEnt.size = info.size;
    dirEnt.name = info.name;
}





















/////////////////////////////////////////////////////////////////
// LFS Management
/////////////////////////////////////////////////////////////////

inline static lfs_t      lfs_;
inline static lfs_config lfsCfg_;


static bool LfsMount()
{
    bool retVal = false;

    int err = lfs_mount(&lfs_, &lfsCfg_);

    // reformat if we can't mount the filesystem
    // this should only happen on the first boot
    if (err)
    {
        LogNNL("  New flash, formatting... ");
        err = lfs_format(&lfs_, &lfsCfg_);

        if (err)
        {
            Err("Mount lfs_format", err);
        }
        else
        {
            err = lfs_mount(&lfs_, &lfsCfg_);
            if (err)
            {
                Err("Mount lfs_mount, format OK, re-mount fail", err);
            }
            else
            {
                Log("OK");
                retVal = true;
            }
        }
    }
    else
    {
        retVal = true;
    }

    return retVal;
}

static bool LfsUnMount()
{
    bool retVal = false;

    int err = lfs_unmount(&lfs_);
    if (err)
    {
        Err("UnMount lfs_unmount", err);
    }
    else
    {
        retVal = true;
    }

    return retVal;
}

static bool LfsFormat()
{
    bool retVal = false;

    int err = lfs_format(&lfs_, &lfsCfg_);
    if (err)
    {
        Err("Format lfs_format", err);
    }
    else
    {
        retVal = true;
    }

    return retVal;
}



























/////////////////////////////////////////////////////////////////
// Hook into RPi Pico Flash
/////////////////////////////////////////////////////////////////


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



// took pico flash operation code from here:
// https://github.com/lurk101/littlefs-lib/blob/master/pico_hal.c

static int FlashRead(const lfs_config *c,
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

static int FlashProg(const lfs_config *c,
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

static int FlashErase(const lfs_config *c, lfs_block_t block)
{
    // Log("Erase block ", block);

    assert(block < lfsCfg_.block_count);
    
    IrqLock lock;
    uint32_t p = (uint32_t)FS_BASE + block * lfsCfg_.block_size;
    flash_range_erase(p, lfsCfg_.block_size);

    return LFS_ERR_OK;
}

static int FlashSync(const lfs_config *c)
{
    // Log("Sync");

    return LFS_ERR_OK;
}
























/////////////////////////////////////////////////////////////////
// Public Interface - General Commands
/////////////////////////////////////////////////////////////////


FilesystemLittleFSFile FilesystemLittleFS::GetFile(const string &fileName)
{
    return FilesystemLittleFSFile{lfs_, fileName};
}

void FilesystemLittleFS::NukeFilesystem()
{
    LfsUnMount();
    LfsFormat();
    LfsMount();
}

bool FilesystemLittleFS::List(const string &path, vector<DirEnt> &dirEntList)
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

bool FilesystemLittleFS::Stat(const string &path, DirEnt &dirEnt)
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


/////////////////////////////////////////////////////////////////
// Public Interface - File Commands
/////////////////////////////////////////////////////////////////

bool FilesystemLittleFS::FileExists(const string &path)
{
    bool retVal = false;

    DirEnt dirEnt;
    if (Stat(path, dirEnt))
    {
        retVal = dirEnt.type == DirEnt::Type::FILE;
    }

    return retVal;
}

string FilesystemLittleFS::Read(const string &path)
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

bool FilesystemLittleFS::Write(const string &path, const string &data)
{
    bool retVal = false;

    auto f = GetFile(path);
    if (f.Open())
    {
        retVal = f.Write(data);

        f.Close();
    }

    return retVal;
}

void FilesystemLittleFS::Touch(string fileName)
{
    auto f = GetFile(fileName);
    f.Open();
    f.Close();
}

void FilesystemLittleFS::Trunc(const string &fileName, uint32_t size)
{
    auto f = GetFile(fileName);
    f.Open();
    f.Trunc(size);
    f.Close();
}


/////////////////////////////////////////////////////////////////
// Public Interface - Directory Commands
/////////////////////////////////////////////////////////////////

bool FilesystemLittleFS::MkDir(string path)
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
// Public Interface - File and Directory Commands
/////////////////////////////////////////////////////////////////

bool FilesystemLittleFS::Remove(const string &path)
{
    bool retVal = false;

    int err = lfs_remove(&lfs_, path.c_str());
    if (err)
    {
        if (err != LFS_ERR_NOENT)
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


/////////////////////////////////////////////////////////////////
// Public Interface - Init
/////////////////////////////////////////////////////////////////

void FilesystemLittleFS::Init()
{
    Timeline::Global().Event("FilesystemLittleFS::Init");

    lfsCfg_ = {
        .read  = FlashRead,
        .prog  = FlashProg,
        .erase = FlashErase,
        .sync  = FlashSync,

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
    Log("Mounting LittleFS (", Commas(FS_SIZE / 1024), " KB)");
    LfsMount();
}

void FilesystemLittleFS::SetupShell()
{
    /////////////////////////////////////////
    // General commands
    /////////////////////////////////////////

    static bool showTimeline_ = false;

    Shell::AddCommand("lfs.timeline", [](vector<string> argList){
        showTimeline_ = argList[0] == "on";

        Log("Timeline now ", showTimeline_ ? "on" : "off");
    }, { .argCount = 1, .help = "timeline on/off" });

    Shell::AddCommand("lfs.format", [](vector<string> argList){
        Timeline t;
        t.Event("start");

        LfsUnMount();
        t.Event("UnMount");
        LfsFormat();
        t.Event("Format");
        LfsMount();

        t.Event("end");
        if (showTimeline_) { t.ReportNow(); }
    }, { .argCount = 0, .help = "format the entire system" });

    Shell::AddCommand("lfs.ls", [](vector<string> argList){
        Timeline t;
        t.Event("start");

        vector<DirEnt> dirEntList;

        string path = "/";
        if (argList.size() >= 1)
        {
            path = argList[0];
        }

        List(path, dirEntList);

        uint32_t totalSize = 0;
        uint8_t maxLen = 0;
        for (auto &dirEnt : dirEntList)
        {
            uint8_t len = (uint8_t)Commas(dirEnt.size).length();

            if (len > maxLen)
            {
                maxLen = len;
            }

            totalSize += dirEnt.size;
        }

        string sizeHeader = "Bytes";
        if (sizeHeader.length() > maxLen)
        {
            maxLen = (uint8_t)sizeHeader.length();
        }

        Log(dirEntList.size(), " element", dirEntList.size() == 1 ? "" : "s", " found, ", Commas(totalSize), " bytes");
        LogNL();
        Log(sizeHeader, "  Name");
        LogXNNL('-', maxLen);
        Log("------");

        string formatString = FormatStr("%%%is", maxLen);

        for (const auto &dirEnt : dirEntList)
        {
            string size = FormatStr(formatString, Commas(dirEnt.size).c_str());

            Log(size, "  ", dirEnt.name, dirEnt.type == DirEnt::Type::DIR ? "/" : "");
        }

        t.Event("end");
        if (showTimeline_) { t.ReportNow(); }
    }, { .argCount = -1, .help = "ls -la [optional <x> target]" });

    Shell::AddCommand("lfs.stat", [](vector<string> argList){
        Timeline t;
        t.Event("start");

        string path = argList[0];

        Log("Stat ", path);
        DirEnt dirEnt;
        bool ok = FilesystemLittleFS::Stat(path, dirEnt);

        if (ok)
        {
            Log("Type: ", dirEnt.type == FilesystemLittleFS::DirEnt::Type::DIR ? "DIR" : "FILE");
            Log("Size: ", Commas(dirEnt.size));
        }
        else
        {
            Log("Could not stat ", path);
        }

        t.Event("end");
        if (showTimeline_) { t.ReportNow(); }
    }, { .argCount = 1, .help = "stat path <x>" });

    Shell::AddCommand("lfs.is.file", [](vector<string> argList){
        Timeline t;
        t.Event("start");

        string fileName = argList[0];

        bool retVal = FileExists(fileName);

        Log(fileName, ": ", retVal ? "True" : "False");

        t.Event("end");
        if (showTimeline_) { t.ReportNow(); }
    }, { .argCount = 1, .help = "path <x> exists and is a file" });


    /////////////////////////////////////////
    // File commands
    /////////////////////////////////////////

    Shell::AddCommand("lfs.touch", [](vector<string> argList){
        Timeline t;
        t.Event("start");

        string fileName = argList[0];

        LogNNL("Touch file ", fileName);

        FilesystemLittleFS::Touch(fileName);

        t.Event("end");
        if (showTimeline_) { t.ReportNow(); }
    }, { .argCount = 1, .help = "touch file <x>" });

    Shell::AddCommand("lfs.trunc", [](vector<string> argList){
        Timeline t;
        t.Event("start");

        string path = argList[0];
        uint32_t size = (uint32_t)atoi(argList[1].c_str());

        Log("Trunc ", path, " to ", size);
        FilesystemLittleFS::Trunc(path, size);

        t.Event("end");
        if (showTimeline_) { t.ReportNow(); }
    }, { .argCount = 2, .help = "trunc <x> file to <y> bytes" });

    Shell::AddCommand("lfs.cat", [](vector<string> argList){
        string fileName = argList[0];

        Log("Cat file ", fileName);

        DirEnt dirEnt;
        bool ok = Stat(fileName, dirEnt);

        if (ok)
        {
            if (dirEnt.type == DirEnt::Type::FILE)
            {
                auto f = GetFile(fileName);
                f.Open();

                uint32_t bytesRemaining = dirEnt.size;
                uint32_t byteOffset = 0;
                while (bytesRemaining)
                {
                    vector<uint8_t> byteList;

                    uint32_t bytesToRead = min((uint32_t)8, bytesRemaining);

                    f.Read(byteList, bytesToRead);

                    LogBlobRow((uint16_t)byteOffset, byteList.data(), (uint16_t)byteList.size(), 1, 1);

                    byteOffset += bytesToRead;
                    bytesRemaining -= bytesToRead;
                }

                f.Close();
            }
            else
            {
                Log("Cat ERR: cannot cat a directory");
            }
        }
        else
        {
            Log("Cat ERR: file does not exist");
        }
    }, { .argCount = 1, .help = "cat file <x>" });

    Shell::AddCommand("lfs.read", [](vector<string> argList){
        string &fileName = argList[0];

        Timeline t;
        t.Event("start");

        string retVal = Read(fileName);
        Log("Bytes: ", retVal.size());
        Log("\"", retVal, "\"");

        t.Event("end");
        if (showTimeline_) { t.ReportNow(); }
    }, { .argCount = 1, .help = "read file <x>" });

    Shell::AddCommand("lfs.write", [](vector<string> argList){
        string &fileName = argList[0];
        string &data     = argList[1];

        Timeline t;
        t.Event("start");

        Write(fileName, data);

        t.Event("end");
        if (showTimeline_) { t.ReportNow(); }
    }, { .argCount = 2, .help = "write to file <x> string <y>" });


    /////////////////////////////////////////
    // Directory commands
    /////////////////////////////////////////

    Shell::AddCommand("lfs.mkdir", [](vector<string> argList){
        Timeline t;
        t.Event("start");

        string path = argList[0];

        Log("MkDir ", path);
        bool ok = FilesystemLittleFS::MkDir(path);

        Log(ok ? "Success" : "Failure");

        t.Event("end");
        if (showTimeline_) { t.ReportNow(); }
    }, { .argCount = 1, .help = "mkdir <x>" });


    /////////////////////////////////////////
    // File and Directory commands
    /////////////////////////////////////////

    Shell::AddCommand("lfs.rm", [](vector<string> argList){
        Timeline t;
        t.Event("start");

        string fileName = argList[0];

        Log("Remove ", fileName);
        FilesystemLittleFS::Remove(fileName);

        t.Event("end");
        if (showTimeline_) { t.ReportNow(); }
    }, { .argCount = 1, .help = "rm <x>" });
}



