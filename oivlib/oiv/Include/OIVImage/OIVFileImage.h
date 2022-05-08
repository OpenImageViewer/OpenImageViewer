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
        ResultCode Load(IMCodec::ImageLoaderFlags loaderFlags, IMCodec::ImageLoadFlags imageLoadFlags);
        ResultCode Load(IMCodec::ImageLoaderFlags loaderFlags);

    private:
        thread_local static inline IMCodec::ImageLoader fImageLoader;

        const std::wstring fFileName;
    };
}