#pragma once
#include "OIVBaseImage.h"
#include <Defs.h>
#include <ImageLoader.h>

namespace OIV
{
    // Decode without accessing the renderer. Metadata is reset before loading; decode
    // failure returns an empty image. The caller must wait for completion before reading either output.
    IMCodec::ImageSharedPtr DecodeFileImage(IMCodec::ImageLoader& loader, const LLUtils::native_string_type& fileName,
                                            IMCodec::ItemMetaDataSharedPtr& metaData,
                                            IMCodec::PluginTraverseMode traverseMode,
                                            IMCodec::ImageLoadFlags imageLoadFlags = IMCodec::ImageLoadFlags::None,
                                            const IMCodec::Parameters& params      = {});

    class OIVFileImage : public OIVBaseImage
    {
      public:

        const LLUtils::native_string_type& GetFileName() const;
        OIVFileImage(const LLUtils::native_string_type& fileName);
        OIVFileImage(const LLUtils::native_string_type& fileName, IMCodec::ImageSharedPtr image);
        ResultCode Load(IMCodec::ImageLoader* imageCodec, IMCodec::PluginTraverseMode loaderFlags,
                        IMCodec::ImageLoadFlags imageLoadFlags, const IMCodec::Parameters& params);
        ResultCode Load(IMCodec::ImageLoader* imageCodec, IMCodec::PluginTraverseMode loaderFlags);

      private:

        const LLUtils::native_string_type fFileName;
    };
}  // namespace OIV
