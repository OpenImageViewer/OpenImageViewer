#pragma once

#include "API\defs.h"
#include <list>
typedef std::basic_string<OIVCHAR> tstring;
typedef std::list<tstring> ListFiles;
typedef ListFiles::const_iterator ListFilesIterator;

class Utility
{
public:
    static tstring GetExePath();

    static tstring GetExeFolder();

    static tstring GetAppDataFolder();

    static void EnsurePath(const tstring& path);

    static bool DirectoryExists(const tstring& path);

    static void find_files(tstring wrkdir, ListFiles &lstFileData, bool recursive = false);
    
    void GetFileSize(char* fileName);
    
    static void* GetFileMapping(const char * fileName);

};