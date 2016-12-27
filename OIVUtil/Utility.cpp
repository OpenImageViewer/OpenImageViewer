#include "Utility.h"
#include <string>
#include <windows.h>
#include <Shlobj.h>
#include "StringUtility.h"
#include <tchar.h>

tstring Utility::GetExePath()
{
    TCHAR ownPth[MAX_PATH];

    // Will contain exe path
    HMODULE hModule = GetModuleHandle(nullptr);
    if (hModule != nullptr)
    {
        // When passing nullptr to GetModuleHandle, it returns handle of exe itself
        GetModuleFileName(hModule, ownPth, (sizeof(ownPth) / sizeof(ownPth[0])));

        // Use above module handle to get the path using GetModuleFileName()
        return tstring(ownPth);
    }

    return tstring();
}

tstring Utility::GetExeFolder()
{
    tstring filePath = Utility::GetExePath();
    tstring::size_type idx = filePath.find_last_of(TEXT("\\"));
    filePath.erase(idx, filePath.length() - idx);
    return filePath;
}

tstring Utility::GetAppDataFolder()
{
    TCHAR szPath[MAX_PATH];

    if (SUCCEEDED(SHGetFolderPath(nullptr,
        CSIDL_APPDATA | CSIDL_FLAG_CREATE,
        nullptr,
        0,
        szPath)))
    {
        tstring result = szPath;
        result += TEXT("\\OIV");
        EnsurePath(result);
        return result;
    }

    return tstring();
}

void Utility::EnsurePath(const tstring& path)
{
    if (DirectoryExists(path) == false)
    {
        SHCreateDirectoryEx(nullptr, path.c_str(), nullptr);
    }
}

bool Utility::DirectoryExists(const tstring& path)
{
    struct _stat64i32 info;
    if (_tstat(path.c_str(), &info) != 0)
        return false;

    else if (info.st_mode & S_IFDIR) // S_ISDIR() doesn't exist on my windows 

    //a directory
        return true;
    else
    // a file 
        return false;
}

void Utility::find_files(tstring wrkdir, ListFiles &lstFileData, bool recursive /*= false*/)
{

    tstring wrkdirtemp = wrkdir;
    if (!wrkdirtemp.empty() && (wrkdirtemp[wrkdirtemp.length() - 1] != L'\\'))
    {
        wrkdirtemp += TEXT("\\");
    }

    WIN32_FIND_DATA file_data = { 0 };
    HANDLE hFile = FindFirstFile((wrkdirtemp + TEXT("*")).c_str(), &file_data);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        return;
    }

    do
    {
        const bool isDirectory = (file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
        //const bool isFile = (file_data.dwFileAttributes & (FILE_ATTRIBUTE_ HIDDEN | FILE_ATTRIBUTE_SYSTEM));

        if (recursive && isDirectory)
        {
            if ((_tcscmp(file_data.cFileName, TEXT(".")) != 0) &&
                (_tcscmp(file_data.cFileName, TEXT("..")) != 0))
            {
                tstring directoryName = wrkdirtemp + file_data.cFileName;
                find_files(directoryName, lstFileData);
            }
        }
        else
        {
            if (!isDirectory)
            {
                lstFileData.push_back(wrkdirtemp + file_data.cFileName);
            }
        }
    } while (FindNextFile(hFile, &file_data));

    FindClose(hFile);
}

std::size_t Utility::GetFileSize(const wchar_t* filePath)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(filePath, GetFileExInfoStandard, &fad))
    {
    //errir
    }
    LARGE_INTEGER size;
    size.HighPart = fad.nFileSizeHigh;
    size.LowPart = fad.nFileSizeLow;
    std::size_t fileSize = size.QuadPart;
    return fileSize;
}

