#pragma once
#include "OIVBaseImage.h"
#include <defs.h>
#include <ImageLoader.h>

namespace OIV
{

    class OIVFileImage : public OIVBaseImage
    {
    public:
        const LLUtils::native_string_type& GetFileName() const;
        OIVFileImage(const LLUtils::native_string_type& fileName);
        ResultCode Load(IMCodec::ImageLoader* imageCodec, IMCodec::PluginTraverseMode loaderFlags, IMCodec::ImageLoadFlags imageLoadFlags, const IMCodec::Parameters& params);
        ResultCode Load(IMCodec::ImageLoader* imageCodec, IMCodec::PluginTraverseMode loaderFlags);
    private:
        const LLUtils::native_string_type fFileName;
    };
}