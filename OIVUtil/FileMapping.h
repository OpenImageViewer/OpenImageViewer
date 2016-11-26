#pragma once
#include <string>
#include <windows.h>
#include "Utility.h"
namespace OIV
{
    class FileMapping
    {
        std::wstring fFilePath;
        size_t mSize;
        HANDLE mHandleMMF;
        HANDLE mHandleFile;
        void *mView;
    public:

        FileMapping(std::wstring filePath)
        {
            fFilePath = filePath;
            mView = mHandleMMF = mHandleFile = nullptr;
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

        void OpenImp()
        {
            mHandleFile = CreateFileW(fFilePath.c_str(), GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL, NULL);

            if (mHandleFile)
            {
                mHandleMMF = CreateFileMapping(mHandleFile, NULL, PAGE_READONLY, 0, 0, NULL);
                if (mHandleMMF != NULL)
                    mView = MapViewOfFile(mHandleMMF, FILE_MAP_READ, 0, 0, 0);
            }

            mSize = Utility::GetFileSize(fFilePath.c_str());
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

        void* GetBuffer()
        {
            return mView;
        }

        size_t GetSize()
        {
            return mSize;
        }

    };
}