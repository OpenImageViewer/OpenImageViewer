#pragma once

#include <Image.h>
#include <OIVAppCore/FileAssociationPolicy.h>
#include <OIVAppCore/BrowseSessionController.h>

#include <cstdint>
#include <string>
#include <variant>

namespace OIV
{
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
    // Each payload carries its own type. There is no independent numeric tag that can disagree with the data.
    using CompletionPayload = std::variant<FileIndexResidencyReadyData, CandidateResidencyReadyData, CountColorsData,
                                           FileAssociationNotice, IFileWatcher::FileChangedEventArgs>;
    struct ViewerCompletion
    {
        CompletionPayload data;
        LLUtils::OperationId operationId;
    };
}  // namespace OIV
