#pragma once
#include <windows.h>
#include <Shlobj.h>
#include "StringUtility.h"
#include "Utility.h"
#include <DbgHelp.h>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

namespace LLUtils
{

    class PlatformUtility
    {
    public:
        struct StackTraceEntry
        {
            std::wstring name;
            uint64_t address;
            std::wstring fileName;
            uint32_t line;
        };

        using StackTrace = std::vector<StackTraceEntry>;

        static StackTrace GetCallStack(int framesToSkip = 0)
        {
            unsigned int   i;
            void* stack[std::numeric_limits<USHORT>::max()];
            unsigned short frames;
            HANDLE         process;

            process = GetCurrentProcess();

            SymInitializeW(process, nullptr, TRUE);

            constexpr size_t maxNameLength = 255;
            constexpr size_t sizeOfStruct = sizeof(SYMBOL_INFOW);

            frames = CaptureStackBackTrace(framesToSkip, std::numeric_limits<USHORT>::max() , stack, nullptr);
            
            SYMBOL_INFOW * symbol = reinterpret_cast<SYMBOL_INFOW *>(malloc(sizeOfStruct + (maxNameLength - 1) * sizeof(TCHAR)));
            symbol->MaxNameLen = maxNameLength;
            symbol->SizeOfStruct = sizeOfStruct;
            StackTrace stackTrace(frames);
            for (i = 0; i < frames; i++)
            {
                SymFromAddrW(process, reinterpret_cast<DWORD64>(stack[i]), 0, symbol);
                IMAGEHLP_LINEW64 line;
                line.SizeOfStruct = sizeof(IMAGEHLP_LINEW64);
                DWORD displacement;
                if (SymGetLineFromAddrW64(process, (DWORD64)(stack[i]), &displacement, &line) == TRUE)
                    stackTrace[i] = { symbol->Name, symbol->Address,line.FileName, line.LineNumber };
            } 

            free(symbol);

            return stackTrace;
        }

        static HANDLE CreateDIB(uint32_t width, uint32_t height, uint16_t bpp, const uint8_t* buffer)
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


        static default_string_type GetModulePath(HMODULE hModule)
        {
            TCHAR ownPth[MAX_PATH];

            if (hModule != nullptr && GetModuleFileName(hModule, ownPth, (sizeof(ownPth) / sizeof(ownPth[0]))) > 0)

                return default_string_type(ownPth);
            else
                return default_string_type();
        }

        static default_string_type GetDllPath()
        {
            return GetModulePath((HINSTANCE)&__ImageBase);
        }

        static default_string_type GetDllFolder()
        {
            using namespace std::experimental;
            return filesystem::path(GetDllPath()).parent_path().wstring();
        }

        static default_string_type GetExePath()
        {
            return GetModulePath(GetModuleHandle(nullptr));
        }

        static default_string_type GetExeFolder()
        {
            using namespace std::experimental;
            return filesystem::path(GetExePath()).parent_path().wstring();
        }

        static default_string_type GetAppDataFolder()
        {
            TCHAR szPath[MAX_PATH];

            if (SUCCEEDED(SHGetFolderPath(nullptr,
                CSIDL_APPDATA | CSIDL_FLAG_CREATE,
                nullptr,
                0,
                szPath)))
            {
                default_string_type result = szPath;
                result += TEXT("\\OIV");
                LLUtils::Utility::EnsureDirectory(result);
                return result;
            }

            return default_string_type();
        }

        
        static void find_files(default_string_type wrkdir, ListWString &lstFileData, bool recursive = false)
        {
            default_string_type wrkdirtemp = wrkdir;
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
                        default_string_type directoryName = wrkdirtemp + file_data.cFileName;
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
        template <class CHAR_TYPE = wchar_t , typename ustring =  std::basic_string<CHAR_TYPE>>
        static ustring GetLastErrorAsString()
        {
            //Get the error message, if any.
            DWORD errorMessageID = ::GetLastError();
            if (errorMessageID == 0)
                return ustring(); //No error message has been recorded

            CHAR_TYPE* messageBuffer = nullptr;
            size_t size = 0;
            if (typeid(CHAR_TYPE) == typeid(wchar_t))
            {
                size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<wchar_t*>(&messageBuffer), 0, NULL);
            }
            else
            {
                size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<char*>(&messageBuffer), 0, NULL);
            }

            ustring message(messageBuffer, size);

            //Free the buffer.
            LocalFree(messageBuffer);

            return message;
        }

        static void CopyTextToClipBoard(const std::wstring& text)
        {
            if (OpenClipboard(nullptr) != FALSE)
            {
                size_t sizeOfCharType = sizeof(std::wstring::value_type);
                size_t sizeInBytes = (text.size() + 1) * sizeOfCharType;
                HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, sizeInBytes);
                if (hg != nullptr)
                {
                    memcpy(GlobalLock(hg), text.c_str(), sizeInBytes);
                    GlobalUnlock(hg);
                    if (SetClipboardData(CF_UNICODETEXT, hg) == nullptr)
                        GlobalFree(hg);
                }

                CloseClipboard();
            }

        }

        static void nanosleep(uint64_t ns) 
        {
            class NanoSleep
            {
            public:
                void Wait(uint64_t ns)
                {
                    mLargeIntener.QuadPart = -static_cast<LONGLONG>(ns / 100); // std::max<int64_t>(100, ns) / 100;
                    if (!SetWaitableTimer(mTimer, &mLargeIntener, 0, nullptr, nullptr, FALSE))
                        throw std::logic_error("Error, could not set timer");

                    WaitForSingleObject(mTimer, INFINITE);
                }
                ~NanoSleep()
                {
                    CloseHandle(mTimer);
                }

            private:
                HANDLE mTimer = CreateWaitableTimer(NULL, TRUE, NULL);
                LARGE_INTEGER mLargeIntener;
            };
            
            static thread_local NanoSleep timer;
            timer.Wait(ns);
        }
    };
}