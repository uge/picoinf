#include "FilesystemLittleFS.h"
#include "Format.h"


bool FilesystemLittleFS::Mount()
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

bool FilesystemLittleFS::UnMount()
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

bool FilesystemLittleFS::Format()
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

void FilesystemLittleFS::Init()
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
    Log("Mounting LittleFS (", Commas(FS_SIZE / 1024), " KB)");
    Mount();

    LogNL();
}

void FilesystemLittleFS::SetupShell()
{
    /////////////////////////////////////////
    // General commands
    /////////////////////////////////////////

    Shell::AddCommand("lfs.format", [](vector<string> argList){
        FilesystemLittleFS::UnMount();
        FilesystemLittleFS::Format();
        FilesystemLittleFS::Mount();
    }, { .argCount = 0, .help = "format the entire system" });

    Shell::AddCommand("lfs.ls", [](vector<string> argList){
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
            uint8_t len = Commas(dirEnt.size).length();

            if (len > maxLen)
            {
                maxLen = len;
            }

            totalSize += dirEnt.size;
        }

        Log(dirEntList.size(), " elements found, ", Commas(totalSize), " bytes");

        string formatString = Format::Str("%%%is", maxLen);

        for (const auto &dirEnt : dirEntList)
        {
            string size = Format::Str(formatString, Commas(dirEnt.size).c_str());

            Log(size, " ", dirEnt.name, dirEnt.type == DirEnt::Type::DIR ? "/" : "");
        }
    }, { .argCount = -1, .help = "ls -la [optional <x> target]" });

    Shell::AddCommand("lfs.stat", [](vector<string> argList){
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
    }, { .argCount = 1, .help = "stat path <x>" });


    /////////////////////////////////////////
    // File commands
    /////////////////////////////////////////

    Shell::AddCommand("lfs.touch", [](vector<string> argList){
        string fileName = argList[0];

        LogNNL("Touch file ", fileName);

        FilesystemLittleFS::Touch(fileName);
    }, { .argCount = 1, .help = "touch file <x>" });

    Shell::AddCommand("lfs.trunc", [](vector<string> argList){
        string path = argList[0];
        uint32_t size = atoi(argList[1].c_str());

        Log("Trunc ", path, " to ", size);
        FilesystemLittleFS::Trunc(path, size);
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

                    LogBlobRow(byteOffset, byteList.data(), byteList.size(), 1, 1);

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



    /////////////////////////////////////////
    // Directory commands
    /////////////////////////////////////////

    Shell::AddCommand("lfs.mkdir", [](vector<string> argList){
        string path = argList[0];

        Log("MkDir ", path);
        bool ok = FilesystemLittleFS::MkDir(path);

        Log(ok ? "Success" : "Failure");
    }, { .argCount = 1, .help = "mkdir <x>" });


    /////////////////////////////////////////
    // File and Directory commands
    /////////////////////////////////////////

    Shell::AddCommand("lfs.rm", [](vector<string> argList){
        string fileName = argList[0];

        Log("Remove ", fileName);
        FilesystemLittleFS::Remove(fileName);
    }, { .argCount = 1, .help = "rm <x>" });
}



