#pragma once

#include "Log.h"

#include "lfs.h"

#include <memory>
#include <vector>
using namespace std;

// Forward declaration
class FilesystemLittleFS;


/////////////////////////////////////////////////////////////////////
// FilesystemLittleFSFile
/////////////////////////////////////////////////////////////////////

class FilesystemLittleFSFile
{
    friend class FilesystemLittleFS;

private:
    FilesystemLittleFSFile(lfs_t &lfs, const string &fileName)
    : state_(make_shared<State>(lfs, fileName))
    {
        // Nothing to do
    }

    void ErrNotOpen(const string &where)
    {
        Log(where, " ERR: ", state_->fileName, " not open");
    }

    void ErrNo(const string &where, int err)
    {
        Log(where, " ERR: ", err, " on ", state_->fileName);
    }

    void ErrWrongBytes(const string &where, int wrong, uint32_t right)
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
                if ((uint32_t)err == bufSize)
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

    bool Read(string &buf)
    {
        bool retVal = false;

        buf = "";

        uint32_t size = Size();
        if (size)
        {
            buf.resize(size);

            uint32_t bytesRead = 0;
            if (Read((uint8_t *)buf.data(), size, &bytesRead))
            {
                retVal = bytesRead == size;
            }
        }

        return retVal;
    }

    bool Write(uint8_t *buf, uint32_t bufSize, uint32_t *bytesWritten = nullptr)
    {
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
                if ((uint32_t)err == bufSize)
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

    bool Write(const string &str)
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

    bool Trunc(uint32_t size)
    {
        bool retVal = false;

        if (state_->open)
        {
            int32_t err = lfs_file_truncate(&state_->lfs, &state_->file, size);

            if (err < 0)
            {
                ErrNo("Trunc", err);
            }
            else
            {
                retVal = true;
            }
        }
        else
        {
            ErrNotOpen("Trunc");
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


