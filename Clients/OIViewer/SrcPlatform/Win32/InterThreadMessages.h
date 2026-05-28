#pragma once

#include <Image.h>
#include <OIVAppCore/BrowseSessionController.h>

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
        LLUtils::native_string_type fileName;
        IMCodec::ImageSharedPtr image;
    };

    using CandidateResidencyReadyData = BrowseSessionController::BrowseCandidateCompletion;

    struct CountColorsData
    {
        void* image;
        int64_t colorCount;
    };
}  // namespace OIV
