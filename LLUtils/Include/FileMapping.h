#pragma once
#include <filesystem>
#include <windows.h>
namespace LLUtils
{
    class FileMapping
    {
        
    public:

        FileMapping(std::wstring filePath) : fFilePath(filePath)
        {
            Open();
        }

        ~FileMapping()
        {
            Close();
        }

        void Open()
        {
            Close();
            OpenImp();
        }

        void Close()
        {
            if (mView != nullptr && UnmapViewOfFile(mView) == 0)
                throw std::exception("Error unmapping file");

            if (mHandleMMF != nullptr && CloseHandle(mHandleMMF) == 0)
                throw std::exception("Error unmapping file");

            if (mHandleFile != nullptr && CloseHandle(mHandleFile) == 0)
                throw std::exception("Error unmapping file");

            mView = mHandleMMF = mHandleFile = nullptr;
        }

        void* GetBuffer() const
        {
            return mView;
        }
        std::size_t GetSize() const
        {
            return mSize;
        }

    private: //methods

        void OpenImp()
        {
            mHandleFile = CreateFileW(fFilePath.c_str(), GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL, nullptr);

            if (mHandleFile)
            {
                mHandleMMF = CreateFileMapping(mHandleFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
                if (mHandleMMF != nullptr)
                    mView = MapViewOfFile(mHandleMMF, FILE_MAP_READ, 0, 0, 0);
            }

            mSize = std::experimental::filesystem::file_size(fFilePath);
        }

    private: // member fields
        const std::wstring fFilePath;
        std::size_t mSize = 0;
        HANDLE mHandleMMF = nullptr;
        HANDLE mHandleFile = nullptr;
        void *mView = nullptr;
    };
}