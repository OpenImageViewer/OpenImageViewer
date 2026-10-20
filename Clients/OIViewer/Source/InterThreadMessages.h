#pragma once

#include <Image.h>
#include <OIVAppCore/FileAssociationPolicy.h>
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
        CountColors,
        FileAssociations
    };

    struct FileIndexResidencyReadyData
    {
        LLUtils::native_string_type fileName;
        IMCodec::ImageSharedPtr image;
    };

    using CandidateResidencyReadyData = BrowseSessionController::BrowseCandidateCompletion;

    // Worker completion owns a source generation, never a presentation identity.
    struct CountColorsData
    {
        // Selected-source identity captured before the worker starts.
        uint64_t generation;
        // Unique color count or the existing failure sentinel.
        int64_t colorCount;
    };
}  // namespace OIV
