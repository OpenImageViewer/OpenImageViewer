#pragma once

#include <Image.h>
#include <OIVAppCore/FileSessionController.h>

#include <cstdint>
#include <string>

namespace OIV
{
    enum class InterThreadMessages : uint16_t
    {
        FileChanged,
        FileIndexResidencyReady,
        CandidateResidencyReady,
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

    using CandidateResidencyReadyData = FileSessionController::CandidateResidencyCompletion;

    struct CountColorsData
    {
        void* image;
        int64_t colorCount;
    };
}  // namespace OIV
