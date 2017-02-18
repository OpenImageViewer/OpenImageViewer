#pragma once
#include <windows.h>
#include <Shlobj.h>
#include "StringUtility.h"
#include "Utility.h"

namespace LLUtils
{

    class PlatformUtility
    {
    public:
        static string_type GetExePath()
        {
            TCHAR ownPth[MAX_PATH];

            // Will contain exe path
            HMODULE hModule = GetModuleHandle(nullptr);
            if (hModule != nullptr)
            {
                // When passing nullptr to GetModuleHandle, it returns handle of exe itself
                GetModuleFileName(hModule, ownPth, (sizeof(ownPth) / sizeof(ownPth[0])));

                // Use above module handle to get the path using GetModuleFileName()
                return std::wstring(ownPth);
            }

            return string_type();
        }
        static string_type GetExeFolder()
        {
            string_type filePath = PlatformUtility::GetExePath();
            string_type::size_type idx = filePath.find_last_of(TEXT("\\"));
            filePath.erase(idx, filePath.length() - idx);
            return filePath;
        }

        static string_type GetAppDataFolder()
        {
            TCHAR szPath[MAX_PATH];

            if (SUCCEEDED(SHGetFolderPath(nullptr,
                CSIDL_APPDATA | CSIDL_FLAG_CREATE,
                nullptr,
                0,
                szPath)))
            {
                string_type result = szPath;
                result += TEXT("\\OIV");
                LLUtils::Utility::EnsureDirectory(result);
                return result;
            }

            return string_type();
        }

        
        static void find_files(string_type wrkdir, ListString &lstFileData, bool recursive = false)
        {
            string_type wrkdirtemp = wrkdir;
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
                    if ((wcscmp(file_data.cFileName, TEXT(".")) != 0) &&
                        (wcscmp(file_data.cFileName, TEXT("..")) != 0))
                    {
                        string_type directoryName = wrkdirtemp + file_data.cFileName;
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
    };
}