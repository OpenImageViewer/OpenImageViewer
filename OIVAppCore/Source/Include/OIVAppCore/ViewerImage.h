#pragma once
#include <Image.h>
#include <ImageProcessing/ImageSource.h>
#include <LLUtils/StringDefs.h>

namespace OIV
{
    // An opened document owns CPU data only. Creation boundaries normalize File paths
    // and leave paths empty for other sources. Published pixels are replacement-only.
    struct ViewerImage
    {
        // Opened pixels, including codec-owned subimages and animation information.
        IMCodec::ImageSharedPtr pixels;
        // Original metadata retained before automatic orientation is applied.
        IMCodec::ItemMetaDataSharedPtr metadata;
        // Origin used by source formatting and file-specific shell actions.
        ImageSource source = ImageSource::GeneratedByLib;
        // Normalized native file path for File, empty for every other source.
        LLUtils::native_string_type path;
    };
}  // namespace OIV
