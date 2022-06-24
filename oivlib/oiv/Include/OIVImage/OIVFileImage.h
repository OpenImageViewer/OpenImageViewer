#pragma once
#include "OIVBaseImage.h"
#include <defs.h>
#include <ImageLoader.h>

namespace OIV
{

    class OIVFileImage : public OIVBaseImage
    {
    public:
        const std::wstring& GetFileName() const;
        OIVFileImage(const std::wstring& fileName);
        ResultCode Load(IMCodec::IImageCodec* imageCodec, IMCodec::ImageLoaderFlags loaderFlags, IMCodec::ImageLoadFlags imageLoadFlags, const IMCodec::Parameters& params);
        ResultCode Load(IMCodec::IImageCodec* imageCodec, IMCodec::ImageLoaderFlags loaderFlags);
    private:
        const std::wstring fFileName;
    };
}