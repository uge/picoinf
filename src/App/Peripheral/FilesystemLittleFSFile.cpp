#include "FilesystemLittleFS.h"
#include "FilesystemLittleFSFile.h"
#include "Log.h"

#include "lfs.h"

using namespace std;

#include "StrictMode.h"



FilesystemLittleFSFile::FilesystemLittleFSFile(lfs_t &lfs, const string &fileName)
: state_(make_shared<State>(lfs, fileName))
{
    // Nothing to do
}

FilesystemLittleFSFile::~FilesystemLittleFSFile()
{
    if (state_.use_count() == 1 && state_->open)
    {
        Log("~FilesystemLittleFSFile ERR: Closing unclosed file ", state_->fileName);
        Close();
    }
}

bool FilesystemLittleFSFile::Open(int flags)
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

bool FilesystemLittleFSFile::Close()
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

bool FilesystemLittleFSFile::Read(uint8_t *buf, uint32_t bufSize, uint32_t *bytesRead)
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
                *bytesRead = (uint32_t)err;
            }
        }
    }
    else
    {
        ErrNotOpen("Read");
    }

    return retVal;
}

bool FilesystemLittleFSFile::Read(vector<uint8_t> &byteList, uint32_t size, uint32_t *bytesRead)
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

bool FilesystemLittleFSFile::Read(string &buf)
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

bool FilesystemLittleFSFile::Write(uint8_t *buf, uint32_t bufSize, uint32_t *bytesWritten)
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
                *bytesWritten = (uint32_t)err;
            }
        }
    }
    else
    {
        ErrNotOpen("Write");
    }

    return retVal;
}

bool FilesystemLittleFSFile::Write(const string &str)
{
    return Write((uint8_t *)str.c_str(), (uint32_t)str.length());
}

uint32_t FilesystemLittleFSFile::Size()
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
            retVal = (uint32_t)err;
        }
    }
    else
    {
        ErrNotOpen("Size");
    }

    return retVal;
}

uint32_t FilesystemLittleFSFile::Tell()
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
            retVal = (uint32_t)err;
        }
    }
    else
    {
        ErrNotOpen("Tell");
    }

    return retVal;
}

bool FilesystemLittleFSFile::Trunc(uint32_t size)
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

void FilesystemLittleFSFile::ErrNotOpen(const string &where)
{
    Log(where, " ERR: ", state_->fileName, " not open");
}

void FilesystemLittleFSFile::ErrNo(const string &where, int err)
{
    Log(where, " ERR: ", err, " on ", state_->fileName);
}

void FilesystemLittleFSFile::ErrWrongBytes(const string &where, int wrong, uint32_t right)
{
    Log(where, " ERR: Byte mismatch - ", wrong, " bytes instead of ", right, " to ", state_->fileName);
}
