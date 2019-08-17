#pragma once
#include <array>
#include "Platform.h"
#if LLUTILS_PLATFORM == LLUTILS_PLATFORM_WIN32
#include <windows.h>
#include <Shlobj.h>
#include <DbgHelp.h>
#endif

#include "StringDefs.h"
#include "Utility.h"
#include <StringUtility.h>

#if LLUTILS_PLATFORM == LLUTILS_PLATFORM_WIN32

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

#pragma push_macro("max"); 

#undef max
#endif

namespace LLUtils
{

    class PlatformUtility
    {
    public:
        struct StackTraceEntry
        {
			std::wstring moduleName;
            std::wstring name;
            std::wstring sourceFileName;
			uint64_t address = 0;
			uint32_t line = 0;
			uint32_t displacement = 0;
        };

        using StackTrace = std::vector<StackTraceEntry>;

        static StackTrace GetCallStack(int framesToSkip = 0)
        {
			StackTrace stackTrace;

#if LLUTILS_PLATFORM == LLUTILS_PLATFORM_WIN32
			constexpr size_t MaxStackTraceSize = std::numeric_limits<USHORT>::max();
            
			static thread_local std::array<void*, MaxStackTraceSize> stack;
			const HANDLE         process = GetCurrentProcess();
			            

			unsigned short frames = CaptureStackBackTrace(framesToSkip, MaxStackTraceSize, stack.data(), nullptr);
			
			static const std::string MutexPrefix = "OIV_Symbol_Api_Mutex";
			
			struct ScopedMutex
			{
				ScopedMutex(bool allowThrow) : mutexLock(CreateMutexA(nullptr, FALSE, (MutexPrefix + "_Lock").c_str()))
				{
					if (mutexLock != nullptr)
						WaitForSingleObject(mutexLock, INFINITE);
					else if (allowThrow == true)
						throw std::logic_error("Error, can not create mutex");
					
				}

				~ScopedMutex()
				{
					if (mutexLock != nullptr)
					{
						ReleaseMutex(mutexLock);
						CloseHandle(mutexLock);
					}
				}
			private:
				   const HANDLE mutexLock = nullptr;
			};

			ScopedMutex lock(false);

			constexpr size_t maxNameLength = 255;
			constexpr size_t sizeOfStruct = sizeof(SYMBOL_INFOW);
			auto symbolReservedMemory = std::make_unique<std::byte[]>(sizeOfStruct + (maxNameLength - 1) * sizeof(TCHAR));
			SYMBOL_INFOW * symbol = reinterpret_cast<SYMBOL_INFOW*>(symbolReservedMemory.get());

			if (SymInitializeW(process, nullptr, TRUE) == TRUE)
			{
				symbol->MaxNameLen = maxNameLength;
				symbol->SizeOfStruct = sizeOfStruct;
				
				stackTrace.resize(frames);
				for (size_t i = 0; i < frames; i++)
				{
					StackTraceEntry& entry = stackTrace[i];

					const DWORD64 memoryAddress = reinterpret_cast<DWORD64>(stack[i]);

					entry.address = memoryAddress;

					if (SymFromAddrW(process, memoryAddress, nullptr, symbol) == TRUE)
					{
						entry.name = symbol->Name;
						entry.address = symbol->Address;
					}

					IMAGEHLP_LINEW64 line;
					line.SizeOfStruct = sizeof(IMAGEHLP_LINEW64);
					DWORD disp;
					if (SymGetLineFromAddrW64(process, memoryAddress, &disp, &line) == TRUE)
					{
						entry.line = line.LineNumber;
						entry.displacement = disp;
						entry.sourceFileName = line.FileName;
					}

					IMAGEHLP_MODULEW64 module64;
					module64.SizeOfStruct = sizeof(IMAGEHLP_MODULEW64);

					if (SymGetModuleInfoW64(process, memoryAddress, &module64) == TRUE)
					{
						entry.moduleName = module64.ImageName;
					}
				}
				if (SymCleanup(process) == FALSE)
				{
					// something bad has happend.
				}
			}
            #else
            // TODO: implement stack back trace for more platforms
            #endif
			return stackTrace;
        }

        static native_string_type GetAppDataFolder()
        {
#if LLUTILS_PLATFORM == LLUTILS_PLATFORM_WIN32
            native_char_type szPath[MAX_PATH];

            if (SUCCEEDED(SHGetFolderPath(nullptr,
                CSIDL_APPDATA | CSIDL_FLAG_CREATE,
                nullptr,
                0,
                szPath)))
            {
                native_string_type result = szPath;
				return result;
            }

            return native_string_type();
#else
            throw std::logic_error("GetAppDataFolder: Not supported in linux yet.");
            //TODO: Make the exception call below work here
            //LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented,);
#endif
        }

#if LLUTILS_PLATFORM == LLUTILS_PLATFORM_WIN32
        static HANDLE CreateDIB(uint32_t width, uint32_t height, uint16_t bpp, const std::byte* buffer)
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
            native_char_type ownPth[MAX_PATH];
            if (hModule != nullptr && GetModuleFileName(hModule, ownPth, (sizeof(ownPth) / sizeof(ownPth[0]))) > 0)

                return StringUtility::ToDefaultString(native_string_type(ownPth));
            else
                return default_string_type();
        }

        static default_string_type GetDllPath()
        {
            return GetModulePath((HINSTANCE)&__ImageBase);
        }

        static default_string_type GetDllFolder()
        {
            using namespace std;
            return StringUtility::ToDefaultString(filesystem::path(GetDllPath()).parent_path().wstring());
        }

        static default_string_type GetExePath()
        {
            return GetModulePath(GetModuleHandle(nullptr));
        }

        static default_string_type GetExeFolder()
        {
            using namespace std;
            return StringUtility::ToDefaultString(filesystem::path(GetExePath()).parent_path().wstring());
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
    
#endif
     

        
        //Returns the last Win32 error, in string format. Returns an empty string if there is no error.
        template <class CHAR_TYPE = wchar_t , typename ustring =  std::basic_string<CHAR_TYPE>>
        static ustring GetLastErrorAsString()
        {

#if LLUTILS_PLATFORM == LLUTILS_PLATFORM_WIN32
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
#else
            return ustring();
#endif
        }
    };
}

#if LLUTILS_PLATFORM == LLUTILS_PLATFORM_WIN32
#pragma pop_macro("max")
#endif