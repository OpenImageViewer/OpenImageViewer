#pragma once
#include <list>
#include <set>
#include <filesystem>

#include "StringUtility.h"


namespace LLUtils
{

    class Utility
    {
    public:
        template <class T>
        static T Align(T num ,T alignement)
        {
            static_assert(std::is_integral<T>(),"Alignment works only with integrals");
            if (alignement < 1)
                throw std::logic_error("alignement must be a positive value");
            return (num + (alignement - 1)) / alignement * alignement;
        }
        

        template <typename T>
        static T Sign(T val)
        {
            return (T(0) < val) - (val < T(0));
        }

        template <class string_type>
        static bool EnsureDirectory(const string_type& path)
        {
            using namespace std::experimental;
            if (filesystem::exists(path) == false)
                return filesystem::create_directory(path);

            return filesystem::is_directory(path);
        }

        struct BlitBox
        {
            uint8_t* buffer;
            uint32_t rowPitch;
            uint32_t width;
            uint32_t height;
            uint32_t left;
            uint32_t top;
            uint32_t pixelSizeInbytes;
            
            int32_t GetStartOffset() const
            {
                return (top * rowPitch) + (left * pixelSizeInbytes);
                
            }


        };

        static void Blit(BlitBox& dst, const BlitBox& src)
        {
            const uint8_t* srcPos = src.buffer + src.GetStartOffset();
            uint8_t* dstPos = dst.buffer + dst.GetStartOffset();

            const uint32_t bytesPerCopy = src.pixelSizeInbytes * src.width;

            for (uint32_t y = src.top; y < src.height; y++)
            {
                memcpy(dstPos, srcPos, bytesPerCopy);
                dstPos += dst.rowPitch;
                srcPos += src.rowPitch;
            }
        }

        static ListWString FindFiles(ListWString& filesList, std::experimental::filesystem::path workingDir, std::wstring fileTypes,bool recursive)
        {
            using namespace std::experimental::filesystem;
            ListWString extensions = StringUtility::split(fileTypes, L';');
            std::set<std::wstring> extensionSet;
            for (const auto& ext : extensions)
                extensionSet.insert(ext);

            auto AddFileIfExtensionsMatches = [&](const path& filePath)
            {
                //TODO : use c++17 string_view instead of erasing the dot
                std::wstring extNoDot =  filePath.extension().wstring().erase(0, 1);

                if (extensionSet.find(extNoDot) != extensionSet.end())
                    filesList.push_back(filePath.wstring());

            };

            if (recursive == true)
                for (const auto& p : recursive_directory_iterator(workingDir))
                    AddFileIfExtensionsMatches(p);
            else
                for (const auto& p : directory_iterator(workingDir))
                    AddFileIfExtensionsMatches(p);

            return filesList;
        }
    };
}