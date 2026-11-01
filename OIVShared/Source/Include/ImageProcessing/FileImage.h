#pragma once
#include <ImageLoader.h>
#include <LLUtils/StringDefs.h>

namespace OIV
{
    // Decode CPU data without a renderer. Reset metadata first and return empty pixels
    // on decode failure. Orientation applies before publishing main pixels and subimages.
    IMCodec::ImageSharedPtr DecodeFileImage(IMCodec::ImageLoader& loader, const LLUtils::native_string_type& fileName,
                                            IMCodec::ItemMetaDataSharedPtr& metadata,
                                            IMCodec::PluginTraverseMode traverseMode,
                                            IMCodec::ImageLoadFlags flags     = IMCodec::ImageLoadFlags::None,
                                            const IMCodec::Parameters& params = {});
}  // namespace OIV
