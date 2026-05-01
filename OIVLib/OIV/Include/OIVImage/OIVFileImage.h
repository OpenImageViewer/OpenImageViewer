#pragma once
#include "OIVBaseImage.h"
#include <Defs.h>
#include <ImageLoader.h>

namespace OIV
{

    class OIVFileImage : public OIVBaseImage
    {
    public:
        const LLUtils::native_string_type& GetFileName() const;
        OIVFileImage(const LLUtils::native_string_type& fileName);
        OIVFileImage(const LLUtils::native_string_type& fileName, IMCodec::ImageSharedPtr image);
        ResultCode Load(IMCodec::ImageLoader* imageCodec, IMCodec::PluginTraverseMode loaderFlags, IMCodec::ImageLoadFlags imageLoadFlags, const IMCodec::Parameters& params);
        ResultCode Load(IMCodec::ImageLoader* imageCodec, IMCodec::PluginTraverseMode loaderFlags);
    private:
        const LLUtils::native_string_type fFileName;
    };
}
