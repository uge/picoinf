#pragma once

#include "lfs.h"

#include <memory>
#include <string>
#include <vector>


class FilesystemLittleFS;

/////////////////////////////////////////////////////////////////////
// FilesystemLittleFSFile
/////////////////////////////////////////////////////////////////////

class FilesystemLittleFSFile
{
    friend class FilesystemLittleFS;

private:

    FilesystemLittleFSFile(lfs_t &lfs, const std::string &fileName);


public:

    ~FilesystemLittleFSFile();
    bool Open(int flags = LFS_O_RDWR | LFS_O_CREAT);
    bool Close();
    bool Read(uint8_t *buf, uint32_t bufSize, uint32_t *bytesRead = nullptr);
    bool Read(std::vector<uint8_t> &byteList, uint32_t size, uint32_t *bytesRead = nullptr);
    bool Read(std::string &buf);
    bool Write(uint8_t *buf, uint32_t bufSize, uint32_t *bytesWritten = nullptr);
    bool Write(const std::string &str);
    uint32_t Size();
    uint32_t Tell();
    bool Trunc(uint32_t size);


private:

    void ErrNotOpen(const std::string &where);
    void ErrNo(const std::string &where, int err);
    void ErrWrongBytes(const std::string &where, int wrong, uint32_t right);


private:

    struct State
    {
        lfs_t       &lfs;
        std::string  fileName;
        bool         open = false;
        lfs_file_t   file;
    };

    std::shared_ptr<State> state_;
};


