#pragma once

#include "FilesystemLittleFSFile.h"

#include <string>
#include <vector>


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

    static FilesystemLittleFSFile GetFile(const std::string &fileName);


    /////////////////////////////////////////////////////////////////
    // General Commands
    /////////////////////////////////////////////////////////////////

    struct DirEnt
    {
        enum class Type : uint8_t
        {
            DIR,
            FILE,
        };

        Type        type = Type::FILE;
        uint32_t    size = 0;
        std::string name;
    };


    static void NukeFilesystem();
    static bool List(const std::string &path, std::vector<DirEnt> &dirEntList);
    static bool Stat(const std::string &path, DirEnt &dirEnt);


    /////////////////////////////////////////////////////////////////
    // File Commands
    /////////////////////////////////////////////////////////////////

    static bool FileExists(const std::string &path);
    static std::string Read(const std::string &path);
    static bool Write(const std::string &path, const std::string &data);
    static void Touch(std::string fileName);
    static void Trunc(const std::string &fileName, uint32_t size);
    static bool Copy(const std::string &fromPath, const std::string &toPath);
    static bool Move(const std::string &fromPath, const std::string &toPath);

    /////////////////////////////////////////////////////////////////
    // Directory Commands
    /////////////////////////////////////////////////////////////////

    static bool MkDir(std::string path);


    /////////////////////////////////////////////////////////////////
    // File and Directory Commands
    /////////////////////////////////////////////////////////////////

    static bool Remove(const std::string &path);
};

