#pragma once

#include <Image.h>
#include <OIVShared/BrowseResidencyManager.h>

#include <cstdint>
#include <string>

namespace OIV
{
    enum class InterThreadMessages : uint16_t
    {
        FileChanged,
        FileIndexResidencyReady,
        FolderLoadResidencyReady,
        AutoScroll,
        FirstFrameDisplayed,
        LoadFileExternally,
        CountColors
    };

    struct FileIndexResidencyReadyData
    {
        std::wstring fileName;
        IMCodec::ImageSharedPtr image;
    };

    struct FolderLoadResidencyReadyData
    {
        BrowseResidencyManager::FileListSnapshot snapshot;
        std::wstring fileName;
        IMCodec::ImageSharedPtr image;
    };

    struct CountColorsData
    {
        void* image;
        int64_t colorCount;
    };
}  // namespace OIV
