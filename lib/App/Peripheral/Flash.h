#pragma once

#include "FilesystemLittleFS.h"
#include "Log.h"


template <typename T>
class Flashable
: public T
{
    using DirEnt = FilesystemLittleFS::DirEnt;

public:
    Flashable()
    : id_(to_string(nextId_))
    {
        ++nextId_;
    }

    bool Get(bool hideWarningIfNotFound = false)
    {
        bool retVal = false;

        DirEnt dirEnt;
        if (FilesystemLittleFS::Stat(id_, dirEnt))
        {
            if (dirEnt.type != DirEnt::Type::FILE)
            {
                Log("Get ERR: Flashable object not a file in storage");
            }
            else if (dirEnt.size != sizeof(T))
            {
                Log("Get ERR: Flashable object is the wrong size in storage to, should be ", sizeof(T), ", but is ", dirEnt.size);
            }
            else
            {
                auto f = FilesystemLittleFS::GetFile(id_);

                if (f.Open())
                {
                    T tmp;

                    retVal = f.Read((uint8_t *)&tmp, sizeof(T));

                    // only modify the wrapped type if the lookup worked
                    if (retVal)
                    {
                        // get reference to the wrapped type
                        T &ref = *(T *)this;

                        ref = tmp;
                    }
                    else
                    {
                        Log("Get ERR: Flashable object could not be read");
                    }

                    f.Close();
                }
            }
        }
        else
        {
            if (hideWarningIfNotFound == false)
            {
                Log("ERR: Flashable object does not exist");
            }
        }

        return retVal;
    }

    bool Put()
    {
        bool retVal = false;

        bool okToTry = true;

        DirEnt dirEnt;
        if (FilesystemLittleFS::Stat(id_, dirEnt))
        {
            if (dirEnt.type != DirEnt::Type::FILE)
            {
                okToTry = false;

                Log("Put ERR: Flashable object not a file in storage");
            }
            else if (dirEnt.size != sizeof(T))
            {
                okToTry = false;

                Log("Put ERR: Flashable object is the wrong size in storage to, should be ", sizeof(T), ", but is ", dirEnt.size);
            }
        }

        if (okToTry)
        {
            auto f = FilesystemLittleFS::GetFile(id_);
            if (f.Open())
            {
                // get reference to the wrapped type
                T &ref = *(T *)this;

                retVal = f.Write((uint8_t *)&ref, sizeof(T));

                f.Close();
            }
        }

        return retVal;
    }

    bool Delete()
    {
        return FilesystemLittleFS::Remove(id_);
    }

private:

    static inline uint16_t nextId_ = 0;

    string id_;
};


