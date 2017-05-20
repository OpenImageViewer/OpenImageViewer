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

        static HANDLE CreateDIB(uint32_t width, uint32_t height,uint16_t bpp, const uint8_t* buffer)
        {
            BITMAPINFOHEADER bi = { 0 };

            bi.biSize = sizeof(BITMAPINFOHEADER);
            bi.biWidth = width;
            bi.biHeight = height;
            bi.biPlanes = 1;              // must be 1
            bi.biBitCount = bpp;          // from parameter
            bi.biCompression = BI_RGB;
          
            DWORD dwBytesPerLine = LLUtils::Utility::Align((DWORD)bpp * width, (DWORD)(sizeof(DWORD) * 8)) / 8;
            DWORD paletteSize = 0; // not supproted.
            DWORD dwLen = bi.biSize + paletteSize + (dwBytesPerLine * height);

            
            HANDLE hDIB = GlobalAlloc(GHND, dwLen);

            if (hDIB)
            {
                // lock memory and get pointer to it
                void *dib = GlobalLock(hDIB);

                *(BITMAPINFOHEADER*)(dib) = bi;

                void* pixelData = (BITMAPINFOHEADER*)(dib)+1;

                size_t size = bi.biWidth * bi.biHeight * (bi.biBitCount / 8);

                memcpy(pixelData, buffer, size);
                GlobalUnlock(hDIB);
            }

            return hDIB;
        }

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


        
            //Returns the last Win32 error, in string format. Returns an empty string if there is no error.
        static std::string GetLastErrorAsString()
        {
            //Get the error message, if any.
            DWORD errorMessageID = ::GetLastError();
            if (errorMessageID == 0)
                return std::string(); //No error message has been recorded

            LPSTR messageBuffer = nullptr;
            size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

            std::string message(messageBuffer, size);

            //Free the buffer.
            LocalFree(messageBuffer);

            return message;
        }
    };
}